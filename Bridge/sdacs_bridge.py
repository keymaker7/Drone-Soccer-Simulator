"""Local guidance adapter using the actual swarm-drone-atc CPU APF implementation.

Unreal remains the source of position/velocity and owns every physics force.
This process produces short guidance targets; it never invents drone telemetry.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib
import importlib.util
import json
import math
import pathlib
import subprocess
import sys
import threading
import time
import uuid
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import numpy as np

EXPECTED_REPOSITORY = "github.com/sun475300-sudo/swarm-drone-atc"
ARENA_MIN = np.array([-7.75, -3.25, 0.25])
ARENA_MAX = np.array([7.75, 3.25, 4.25])
TARGET_HORIZON_M = 0.70
TARGET_SEPARATION_M = 1.10
# Unreal's position controller requests velocity = position error * 0.8 / s.
# This lookahead converts APF velocity advice back into that position interface.
TARGET_LOOKAHEAD_S = 1.25
PROTOCOL_VERSION = 2
MAX_CORRELATION_ID = 2**53-1


class StaleCommandError(ValueError):
    """A superseded or repeated command must not change planner state."""


class PlannerSession:
    def __init__(self):
        self.waypoints = {}
        self.hold_positions = {}
        self.takeoff_origins = {}
        self.takeoff_complete = set()
        self.return_stages = {}
        self.return_anchors = {}
        self.mode = None
        self.generation = 0
        self.last_request = 0


def _correlation(payload: dict) -> dict | None:
    fields = ("protocol_version", "session_id", "command_id", "request_id")
    if not any(field in payload for field in fields):
        return None  # Isolated compatibility path for earlier simulator builds.
    if payload.get("protocol_version") != PROTOCOL_VERSION or isinstance(payload.get("protocol_version"), bool):
        raise ValueError("protocol_version must be 2")
    session = payload.get("session_id")
    if not isinstance(session, str) or len(session) != 36:
        raise ValueError("session_id must be a canonical UUID")
    try:
        if str(uuid.UUID(session)) != session:
            raise ValueError("non-canonical session UUID")
    except (ValueError, AttributeError):
        raise ValueError("session_id must be a canonical UUID") from None
    for field in ("command_id", "request_id"):
        value = payload.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= MAX_CORRELATION_ID:
            raise ValueError(f"{field} must be a positive exact JSON integer")
    return {field: payload[field] for field in fields}


def _git(repo: pathlib.Path, *arguments: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(repo), *arguments], text=True, stderr=subprocess.STDOUT,
        encoding="utf-8", timeout=10,
    ).strip()


def _vector(value: object, name: str) -> np.ndarray:
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f"{name} must be a three-number list")
    if any(isinstance(v, bool) or not isinstance(v, (int, float)) for v in value):
        raise ValueError(f"{name} must contain numbers")
    result = np.asarray(value, dtype=float)
    if not np.all(np.isfinite(result)):
        raise ValueError(f"{name} must contain finite numbers")
    return result


def _minimum_distance(points: list[np.ndarray]) -> float | None:
    if len(points) < 2:
        return None
    return min(float(np.linalg.norm(a-b)) for i, a in enumerate(points) for b in points[i+1:])


class GuidanceBridge:
    def __init__(self, repo: str | pathlib.Path, *, bundled: bool = False) -> None:
        sys.dont_write_bytecode = True
        self.repo = pathlib.Path(repo).resolve()
        expected = self.repo / "simulation" / "apf_engine" / "apf.py"
        if bundled:
            # Distribution contains the actual MIT-licensed APF source, not a
            # working checkout or a machine-specific path. Reject damaged data.
            metadata = json.loads((self.repo / 'provenance.json').read_text(encoding='utf-8-sig'))
            self.origin = metadata['origin']
            self.commit = metadata['commit']
            if self.origin != 'https://github.com/sun475300-sudo/swarm-drone-atc.git' or len(self.commit) != 40 or any(c not in '0123456789abcdef' for c in self.commit):
                raise RuntimeError('Invalid bundled upstream provenance')
            for relative, digest in metadata['files'].items():
                path = (self.repo / relative).resolve()
                if not path.is_relative_to(self.repo) or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
                    raise RuntimeError('Bundled upstream hash mismatch')
            if 'simulation/apf_engine/apf.py' not in metadata['files'] or 'LICENSE' not in metadata['files']:
                raise RuntimeError('Bundled upstream source or licence missing')
            spec = importlib.util.spec_from_file_location('imperial_bundled_upstream_apf', expected)
            self.apf = importlib.util.module_from_spec(spec)
            sys.modules[spec.name] = self.apf
            spec.loader.exec_module(self.apf)
        else:
            self.origin = _git(self.repo, "remote", "get-url", "origin")
            if EXPECTED_REPOSITORY not in self.origin.lower().replace(":", "/"):
                raise RuntimeError(f"Expected swarm-drone-atc origin; found {self.origin}")
            self.commit = _git(self.repo, "rev-parse", "HEAD")
        # The user's source repository is an input, including its ignored files.
        sys.dont_write_bytecode = True
        if not bundled:
            sys.path.insert(0, str(self.repo))
            self.apf = importlib.import_module("simulation.apf_engine.apf")
        self.apf_path = pathlib.Path(self.apf.__file__).resolve()
        if self.apf_path != expected:
            raise RuntimeError(f"Loaded APF module from unexpected location: {self.apf_path}")
        self.apf_sha256 = hashlib.sha256(self.apf_path.read_bytes()).hexdigest()
        self.params = dict(self.apf.APF_PARAMS)
        self.params.update(
            k_att=0.70, k_rep_drone=6.0, k_rep_obs=0.015,
            d0_drone=2.4, d0_obs=0.55, max_force=4.0,
            altitude_k=0.0, target_alt=2.5,
        )
        self.lock = threading.Lock()
        self.waypoints: dict[int, int] = {}
        self.hold_positions: dict[int, np.ndarray] = {}
        self.takeoff_origins: dict[int, np.ndarray] = {}
        self.takeoff_complete: set[int] = set()
        self.return_stages: dict[int, str] = {}
        self.return_anchors: dict[int, np.ndarray] = {}
        self.mode: str | None = None
        self.requests = 0
        self.started = time.monotonic()
        self.sessions: OrderedDict[str, PlannerSession] = OrderedDict()

    def health(self) -> dict:
        return {
            "ok": True, "source": "swarm-drone-atc", "source_commit": self.commit,
            "protocol_version": PROTOCOL_VERSION, "supported_protocol_versions": [1, 2],
            "source_origin": self.origin, "apf_module": str(self.apf_path),
            "apf_sha256": self.apf_sha256, "backend": "upstream-numpy-cpu",
            "numpy_version": np.__version__, "upstream_params": self.params,
            "guidance_revision": "fida40-staged-return-v2",
            "position_lookahead_s": TARGET_LOOKAHEAD_S,
            "return_corridor_heights_m": [1.0, 2.2],
            "telemetry_authority": "Unreal Engine physics", "request_count": self.requests,
            "uptime_s": round(time.monotonic()-self.started, 3),
        }

    def reset(self) -> dict:
        with self.lock:
            self.waypoints.clear()
            self.hold_positions.clear()
            self.takeoff_origins.clear()
            self.takeoff_complete.clear()
            self.return_stages.clear()
            self.return_anchors.clear()
            self.mode = None
            self.requests = 0
            self.sessions.clear()
        return {"ok": True, "source": "swarm-drone-atc", "source_commit": self.commit}

    @staticmethod
    def _route(drone_id: int, home: np.ndarray) -> list[np.ndarray]:
        slot = drone_id % 5
        side = -1.0 if drone_id < 5 else 1.0
        # Separate opposing patrols vertically. A 20cm offset made head-on
        # repulsion push the lower Striker towards the floor in actual UE runs.
        altitude = 1.5 + slot*0.2 if drone_id < 5 else 2.9 + slot*0.15
        lane = float(np.clip(home[1], -3.2, 3.2))
        return [
            # Stay inside the goal planes, including the climb after takeoff.
            np.array([side*5.3, lane, min(2.1, altitude)]),
            np.array([side*5.3, lane, altitude]),
            np.array([-side*5.3, lane, altitude]),
            np.array([-side*5.3, float(np.clip(lane+0.65*side, -3.0, 3.0)), min(3.8, altitude+0.1)]),
            np.array([side*5.3, float(np.clip(lane+0.65*side, -3.0, 3.0)), altitude]),
        ]

    @staticmethod
    def _constrain(target: np.ndarray, position: np.ndarray) -> np.ndarray:
        target = np.clip(target, ARENA_MIN, ARENA_MAX)
        difference = target-position
        length = float(np.linalg.norm(difference))
        if length > TARGET_HORIZON_M:
            target = position+difference*(TARGET_HORIZON_M/length)
        return np.clip(target, ARENA_MIN, ARENA_MAX)

    def step(self, payload: object) -> dict:
        if not isinstance(payload, dict):
            raise ValueError("request must be a JSON object")
        correlation = _correlation(payload)
        mode = payload.get("mode")
        if mode not in ("mission", "hold", "return"):
            raise ValueError("mode must be mission, hold or return")
        dt = payload.get("dt")
        if isinstance(dt, bool) or not isinstance(dt, (int, float)) or not math.isfinite(dt) or not 0.001 <= dt <= 0.5:
            raise ValueError("dt must be finite and between 0.001 and 0.5 seconds")
        items = payload.get("drones")
        if not isinstance(items, list) or not 1 <= len(items) <= 10:
            raise ValueError("drones must contain between one and ten entries")
        drones = []
        identifiers: set[int] = set()
        for item in items:
            if not isinstance(item, dict):
                raise ValueError("each drone must be an object")
            drone_id = item.get("id")
            if isinstance(drone_id, bool) or not isinstance(drone_id, int) or not 0 <= drone_id <= 9:
                raise ValueError("drone id must be an integer from zero to nine")
            if drone_id in identifiers:
                raise ValueError("drone ids must be unique")
            identifiers.add(drone_id)
            position = _vector(item.get("position_m"), "position_m")
            velocity = _vector(item.get("velocity_mps"), "velocity_mps")
            home = _vector(item.get("home_m"), "home_m")
            if np.any(np.abs(position[:2]) > np.array([9.0, 5.0])) or not 0 <= position[2] <= 5.0:
                raise ValueError("position_m is outside the training arena's telemetry envelope")
            if float(np.linalg.norm(velocity)) > 50:
                raise ValueError("velocity_mps exceeds the telemetry limit")
            if np.any(home < ARENA_MIN) or np.any(home > ARENA_MAX):
                raise ValueError("home_m must be inside the safe arena bounds")
            drones.append((drone_id, position, velocity, home))
        drones.sort(key=lambda d: d[0])
        with self.lock:
            if correlation is None:
                return self._compute(mode, float(dt), drones)
            session_id = correlation["session_id"]
            state = self.sessions.get(session_id, PlannerSession())
            generation, sequence = correlation["command_id"], correlation["request_id"]
            if generation < state.generation or sequence <= state.last_request:
                raise StaleCommandError("superseded command or repeated request")
            if generation == state.generation and state.mode is not None and state.mode != mode:
                raise StaleCommandError("mode change requires a new command_id")
            if generation > state.generation:
                state = PlannerSession()
                state.generation = generation
            state.last_request = sequence
            self.sessions[session_id] = state
            self.sessions.move_to_end(session_id)
            while len(self.sessions) > 16:
                self.sessions.popitem(last=False)
            result = self._compute(mode, float(dt), drones, state)
            result.update(correlation)
            result["mode"] = mode
            return result

    def _compute(self, mode: str, dt: float, drones: list, state=None) -> dict:
        plan = self if state is None else state
        started = time.perf_counter()
        if mode != plan.mode:
            if mode == "hold":
                plan.hold_positions = {drone_id: position.copy() for drone_id, position, _, _ in drones}
            plan.return_stages.clear()
            plan.return_anchors.clear()
            plan.mode = mode
        states = [self.apf.APFState(position.copy(), velocity.copy(), str(drone_id))
                  for drone_id, position, velocity, _ in drones]
        goals: dict[str, np.ndarray] = {}
        stages: dict[int, str] = {}
        for drone_id, position, velocity, home in drones:
            plan.waypoints.setdefault(drone_id, 0)
            if mode == "hold":
                goals[str(drone_id)] = plan.hold_positions.setdefault(drone_id, position.copy())
                stages[drone_id] = "hold"
            elif mode == "return":
                horizontal = float(np.linalg.norm(position[:2]-home[:2]))
                stage = plan.return_stages.setdefault(drone_id, "return-land" if horizontal < 0.65 else "return-altitude")
                corridor_height = 1.0 if drone_id < 5 else 2.2
                if stage == "return-altitude":
                    anchor = plan.return_anchors.setdefault(drone_id, position.copy())
                    goals[str(drone_id)] = np.array([anchor[0], anchor[1], corridor_height])
                    if abs(float(position[2])-corridor_height) < 0.12 and abs(float(velocity[2])) < 0.35:
                        stage = "return-transit"
                if stage == "return-transit":
                    goals[str(drone_id)] = np.array([home[0], home[1], corridor_height])
                    if horizontal < 0.65 and float(np.linalg.norm(velocity[:2])) < 0.45:
                        stage = "return-land"
                if stage == "return-land":
                    goals[str(drone_id)] = home.copy()
                    # Descend to 25cm before UE
                    # disarms, rather than cutting motors up to 65cm high.
                    goals[str(drone_id)][2] = ARENA_MIN[2]
                plan.return_stages[drone_id] = stages[drone_id] = stage
                if position[2] <= 0.55 and abs(float(velocity[2])) < 0.35:
                    plan.takeoff_complete.discard(drone_id)
                    plan.takeoff_origins.pop(drone_id, None)
                    plan.waypoints[drone_id] = 0
            else:
                route = self._route(drone_id, home)
                # Pads at x +/-6.5 lie under the goals. A 40cm ball must leave
                # below the hoop's 2.5m outer bottom before climbing past it.
                takeoff_altitude = min(2.2, float(route[0][2]))
                # A landed aircraft needs a new climb, including re-launch without
                # a bridge restart. Keep an airborne HOLD/resume on its current leg.
                if (drone_id in plan.takeoff_complete and position[2] <= 0.55
                        and abs(float(velocity[2])) < 0.35):
                    plan.takeoff_complete.discard(drone_id)
                    plan.takeoff_origins.pop(drone_id, None)
                    plan.waypoints[drone_id] = 0
                if drone_id not in plan.takeoff_complete:
                    origin = plan.takeoff_origins.setdefault(drone_id, position.copy())
                    if position[2] >= takeoff_altitude-0.08:
                        plan.takeoff_complete.add(drone_id)
                    else:
                        goals[str(drone_id)] = np.array([origin[0], origin[1], takeoff_altitude])
                        stages[drone_id] = "takeoff"
                        continue
                stages[drone_id] = "patrol"
                index = plan.waypoints[drone_id]
                if float(np.linalg.norm(route[index % len(route)]-position)) < 0.45:
                    index += 1
                    plan.waypoints[drone_id] = index
                goals[str(drone_id)] = route[index % len(route)]

        # Both functions below are imported from the checked-out upstream file.
        # Real telemetry, including a manually piloted neighbour, forms the APF pool.
        control_params = self.params
        if mode == "return":
            # The FIDA launch pads are 1.25m apart. A 2.4m APF influence pushes
            # parked neighbours away from their pads forever. Return corridors
            # supply vertical separation; retain a 1.1m local repulsion envelope.
            control_params = {**self.params, "d0_drone": TARGET_SEPARATION_M}
        forces = self.apf.batch_compute_forces(states, goals, [], comm_range=30.0, params=control_params)
        conflicts = 0
        for i, (_, position, velocity, _) in enumerate(drones):
            for j in range(i+1, len(drones)):
                delta = drones[j][1]-position
                relative = drones[j][2]-velocity
                speed2 = float(np.dot(relative, relative))
                cpa_time = float(np.clip(-np.dot(delta, relative)/speed2, 0.0, 2.0)) if speed2 > 1e-9 else 0.0
                cpa_distance = float(np.linalg.norm(delta+relative*cpa_time))
                distance = float(np.linalg.norm(delta))
                if distance < 1.4 or (0 < cpa_time <= 2.0 and cpa_distance < 1.1):
                    conflicts += 1
                if distance < TARGET_SEPARATION_M or (0 < cpa_time <= 2.0 and cpa_distance < TARGET_SEPARATION_M):
                    # Higher ID yields laterally, removing a symmetric APF head-on
                    # local minimum without replacing upstream APF computation.
                    side = np.array([-delta[1], delta[0], 0.0])
                    length = float(np.linalg.norm(side))
                    side = side/length if length > 1e-6 else np.array([0.0, 1.0, 0.0])
                    forces[str(drones[j][0])] += side*1.2
        targets: list[np.ndarray] = []
        for drone_id, position, velocity, _ in drones:
            # Velocity damping makes the acceleration advice usable by a separate
            # UE force controller; this is adapter tuning, not measured airframe data.
            acceleration = forces[str(drone_id)]-velocity*1.4
            desired_velocity = self.apf.force_to_velocity(velocity, acceleration, dt, max_speed=2.0)
            target = position+desired_velocity*TARGET_LOOKAHEAD_S
            if stages[drone_id] == "takeoff":
                # Climb deliberately before permitting a cross-arena route. The
                # old centimetre-scale velocity offset crept along the floor.
                # APF still controls lateral avoidance and can veto upward motion
                # when a vertically adjacent aircraft produces downward repulsion.
                if forces[str(drone_id)][2] >= 0.0:
                    target[2] = min(float(goals[str(drone_id)][2]), float(position[2])+TARGET_HORIZON_M)
                else:
                    target[2] = min(float(target[2]), float(position[2]))
            elif stages[drone_id] == "return-altitude":
                # Align to the two vertically separated travel corridors before
                # crossing the opposing team; both stay below the goal-ring body.
                difference = float(goals[str(drone_id)][2]-position[2])
                if difference*float(forces[str(drone_id)][2]) >= 0.0:
                    target[2] = position[2]+float(np.clip(difference, -TARGET_HORIZON_M, TARGET_HORIZON_M))
            elif stages[drone_id] == "return-land":
                # Upstream attraction has a 10cm deadband and still repels the
                # ground. That can stall a stopped ball at 33-35cm forever.
                # Terminal guidance changes only the requested height; UE still
                # owns forces/landing and keeps its <28cm + low-speed ARM gate.
                # Retain APF escape advice when another ball occupies the column.
                home = goals[str(drone_id)]
                column_clear = all(other_id == drone_id or
                    float(np.linalg.norm(other_pos[:2]-position[:2])) >= TARGET_SEPARATION_M
                    for other_id, other_pos, _, _ in drones)
                if column_clear and float(np.linalg.norm(position[:2]-home[:2])) < 0.45 and position[2] <= 0.65:
                    target[2] = ARENA_MIN[2]
            targets.append(self._constrain(target, position))

        # Project short targets apart. This is an extra target-space guard: actual
        # separation is measured from UE telemetry and cannot be guaranteed by HTTP.
        for _ in range(32):
            changed = False
            for i in range(len(targets)):
                for j in range(i+1, len(targets)):
                    difference = targets[j]-targets[i]
                    distance = float(np.linalg.norm(difference))
                    if distance >= TARGET_SEPARATION_M-1e-6:
                        continue
                    direction = difference/distance if distance > 1e-8 else np.array([0.0, 1.0, 0.0])
                    correction = direction*(TARGET_SEPARATION_M-distance+1e-5)*0.5
                    targets[i] = self._constrain(targets[i]-correction, drones[i][1])
                    targets[j] = self._constrain(targets[j]+correction, drones[j][1])
                    changed = True
            if not changed:
                break
        self.requests += 1
        target_minimum = _minimum_distance(targets)
        return {
            "source": "swarm-drone-atc", "source_commit": self.commit,
            "targets": [{"id": drone_id, "target_m": target.tolist(), "waypoint": plan.waypoints[drone_id],
                         "stage": stages[drone_id]}
                        for (drone_id, _, _, _), target in zip(drones, targets)],
            "min_separation_m": _minimum_distance([position for _, position, _, _ in drones]),
            "conflicts": conflicts, "target_separation_m": target_minimum,
            "target_spacing_limited": target_minimum is not None and target_minimum < 1.099,
            "processing_ms": round((time.perf_counter()-started)*1000.0, 3),
        }


def make_handler(bridge: GuidanceBridge):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.0"

        def log_message(self, _format: str, *_args: object) -> None:
            return

        def _reply(self, status: int, body: dict) -> None:
            data = json.dumps(body, allow_nan=False, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def do_GET(self) -> None:
            if self.path == "/health":
                self._reply(200, bridge.health())
            else:
                self._reply(404, {"error": "unknown endpoint"})

        def do_POST(self) -> None:
            if self.path not in ("/step", "/reset"):
                self._reply(404, {"error": "unknown endpoint"})
                return
            try:
                length = int(self.headers.get("Content-Length", "0"))
                if not 0 <= length <= 65536:
                    raise ValueError("request body exceeds 65536 bytes")
                raw = self.rfile.read(length)
                payload = json.loads(raw) if raw else {}
                if self.path == "/reset":
                    if not isinstance(payload, dict):
                        raise ValueError("reset request must be a JSON object")
                    result = bridge.reset()
                else:
                    result = bridge.step(payload)
                self._reply(200, result)
            except StaleCommandError as error:
                self._reply(409, {"error": str(error), "source": "swarm-drone-atc"})
            except (ValueError, TypeError, OverflowError) as error:
                self._reply(400, {"error": str(error), "source": "swarm-drone-atc"})
            except Exception as error:
                print(f"SDACS bridge error: {type(error).__name__}: {error}", file=sys.stderr, flush=True)
                self._reply(500, {"error": "upstream APF computation failed; no fallback guidance produced"})
    return Handler


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default=str(pathlib.Path(__file__).resolve().parents[2]/"swarm-drone-atc"))
    parser.add_argument("--host", default="127.0.0.1", choices=["127.0.0.1"])
    parser.add_argument("--port", type=int, default=8766)
    args = parser.parse_args()
    try:
        bridge = GuidanceBridge(args.repo)
        server = ThreadingHTTPServer((args.host, args.port), make_handler(bridge))
        server.daemon_threads = True
        print(json.dumps({"ready": True, "url": f"http://{args.host}:{args.port}", **bridge.health()}), flush=True)
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        return 0
    except Exception as error:
        print(f"SDACS bridge startup failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

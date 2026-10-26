from __future__ import annotations
import concurrent.futures
import json
import os
import pathlib
import sys
import threading
import time
import unittest
from unittest import mock
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from sdacs_bridge import GuidanceBridge, make_handler


def request_payload(mode="mission"):
    return {"dt": 0.1, "mode": mode, "drones": [
        {"id": i, "position_m": [-6.5 if i < 5 else 6.5, -2.5+(i % 5)*1.25, 0.35],
         "velocity_mps": [0.0, 0.0, 0.0],
         "home_m": [-6.5 if i < 5 else 6.5, -2.5+(i % 5)*1.25, 0.35]}
        for i in range(10)]}


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        default_repo = pathlib.Path(__file__).resolve().parents[2]/"swarm-drone-atc"
        repo = pathlib.Path(os.environ.get("SDACS_REPO", str(default_repo)))
        if not (repo/"simulation/apf_engine/apf.py").is_file():
            raise RuntimeError("Set SDACS_REPO to the upstream swarm-drone-atc clone before running bridge tests.")
        cls.bridge = GuidanceBridge(str(repo))
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(cls.bridge))
        cls.server.daemon_threads = True
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.url = f"http://127.0.0.1:{cls.server.server_port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)

    def setUp(self):
        self.bridge.reset()

    def post(self, path, payload):
        data = json.dumps(payload).encode()
        req = urllib.request.Request(self.url+path, data=data, headers={"Content-Type": "application/json"})
        with urllib.request.urlopen(req, timeout=5) as response:
            return json.load(response)

    def test_actual_upstream_and_telemetry(self):
        with urllib.request.urlopen(self.url+"/health") as response:
            health = json.load(response)
        self.assertTrue(health["ok"])
        self.assertEqual(health["backend"], "upstream-numpy-cpu")
        self.assertEqual(pathlib.Path(health["apf_module"]).resolve(), self.bridge.repo/"simulation/apf_engine/apf.py")
        self.assertEqual(len(health["source_commit"]), 40)
        payload = request_payload()
        result = self.post("/step", payload)
        self.assertEqual(result["source"], "swarm-drone-atc")
        self.assertEqual(len(result["targets"]), 10)
        self.assertAlmostEqual(result["min_separation_m"], 1.25)
        self.assertGreaterEqual(result["target_separation_m"], 1.099)
        self.assertFalse(result["target_spacing_limited"])
        for target, drone in zip(result["targets"], payload["drones"]):
            point = np.asarray(target["target_m"])
            self.assertLessEqual(float(np.linalg.norm(point-np.asarray(drone["position_m"]))), 0.700001)
            self.assertTrue(np.all(point >= [-7.75, -3.25, 0.25]))
            self.assertTrue(np.all(point <= [7.75, 3.25, 4.25]))
        # Moving a real input changes the measured separation; there is no telemetry generator.
        payload["drones"][1]["position_m"][1] = -1.3
        changed = self.post("/step", payload)
        self.assertAlmostEqual(changed["min_separation_m"], 1.2)

    def test_validation_and_no_fallback(self):
        invalid = []
        for field, value in [("dt", -1), ("dt", float("nan")), ("dt", True), ("mode", "invented"), ("drones", [])]:
            payload = request_payload(); payload[field] = value; invalid.append(payload)
        for field, value in [("position_m", [0, 0]), ("velocity_mps", [float("inf"), 0, 0]),
                             ("home_m", [20, 0, 1]), ("id", True), ("id", 10)]:
            payload = request_payload(); payload["drones"][0][field] = value; invalid.append(payload)
        payload = request_payload(); payload["drones"][1]["id"] = 0; invalid.append(payload)
        invalid += [[], None, "not an object"]
        for payload in invalid:
            with self.subTest(payload=payload), self.assertRaises(urllib.error.HTTPError) as caught:
                self.post("/step", payload)
            self.assertEqual(caught.exception.code, 400)
        req = urllib.request.Request(self.url+"/step", data=b"{broken", headers={"Content-Type": "application/json"})
        with self.assertRaises(urllib.error.HTTPError) as caught:
            urllib.request.urlopen(req)
        self.assertEqual(caught.exception.code, 400)
        self.assertEqual(self.bridge.requests, 0)
        with mock.patch.object(self.bridge.apf, "batch_compute_forces", side_effect=RuntimeError("test upstream failure")):
            with self.assertRaises(urllib.error.HTTPError) as caught:
                self.post("/step", request_payload())
            self.assertEqual(caught.exception.code, 500)
            failure = json.load(caught.exception)
            self.assertNotIn("targets", failure)

    def test_upstream_functions_are_actually_called(self):
        with mock.patch.object(self.bridge.apf, "batch_compute_forces", wraps=self.bridge.apf.batch_compute_forces) as forces:
            with mock.patch.object(self.bridge.apf, "force_to_velocity", wraps=self.bridge.apf.force_to_velocity) as velocities:
                self.post("/step", request_payload())
                self.assertEqual(forces.call_count, 1)
                self.assertEqual(velocities.call_count, 10)

    def test_modes_waypoints_and_reset(self):
        payload = request_payload()
        self.post("/step", payload)
        payload["drones"][0]["position_m"][2] = 1.3
        progressed = self.post("/step", payload)
        self.assertEqual(progressed["targets"][0]["waypoint"], 1)
        held = self.post("/step", {**payload, "mode": "hold"})
        self.assertEqual(len(held["targets"]), 10)
        returned = self.post("/step", {**payload, "mode": "return"})
        self.assertLess(returned["targets"][0]["target_m"][2], 1.3)
        self.post("/reset", {})
        self.assertEqual(self.bridge.requests, 0)
        self.assertEqual(self.bridge.waypoints, {})
        self.assertEqual(self.bridge.takeoff_origins, {})
        self.assertEqual(self.bridge.takeoff_complete, set())

    def test_staged_takeoff_clearance_and_landed_relaunch(self):
        payload = request_payload()
        result = self.post("/step", payload)
        for target, drone in zip(result["targets"], payload["drones"]):
            self.assertEqual(target["stage"], "takeoff")
            self.assertEqual(target["waypoint"], 0)
            self.assertGreater(target["target_m"][2]-drone["position_m"][2], 0.65)
            self.assertAlmostEqual(target["target_m"][0], drone["position_m"][0])
            self.assertLessEqual(np.linalg.norm(np.array(target["target_m"])-drone["position_m"]), 0.700001)
        # A low altitude cannot skip directly into a horizontal patrol leg.
        payload["drones"][0]["position_m"][2] = 1.0
        low = self.post("/step", payload)
        self.assertEqual(low["targets"][0]["stage"], "takeoff")
        self.assertEqual(low["targets"][0]["waypoint"], 0)
        payload["drones"][0]["position_m"][2] = 1.23
        clear = self.post("/step", payload)
        self.assertEqual(clear["targets"][0]["stage"], "patrol")
        self.assertEqual(clear["targets"][0]["waypoint"], 1)
        self.assertGreater(clear["targets"][0]["target_m"][0], -6.5)
        # The centre-row orange pad sits directly beneath the FIDA hoop. Its
        # takeoff gate must release below centre z=2.3m, not collide at z=2.5m.
        payload["drones"][7]["position_m"][2] = 2.13
        below_hoop = self.post("/step", payload)
        self.assertEqual(below_hoop["targets"][7]["stage"], "patrol")
        self.assertEqual(below_hoop["targets"][7]["waypoint"], 1)
        self.assertLess(below_hoop["targets"][7]["target_m"][0], 6.5)
        # A later physical landing starts a fresh staged launch without restart.
        payload["drones"][0]["position_m"][2] = 0.35
        relaunched = self.post("/step", payload)
        self.assertEqual(relaunched["targets"][0]["stage"], "takeoff")
        self.assertEqual(relaunched["targets"][0]["waypoint"], 0)
        self.assertGreater(relaunched["targets"][0]["target_m"][2]-0.35, 0.65)
        # HOLD deliberately interrupts ascent and captures the actual pose.
        held = self.post("/step", {**payload, "mode": "hold"})
        self.assertEqual(held["targets"][0]["stage"], "hold")
        self.assertLess(held["targets"][0]["target_m"][2], 0.5)

    def test_position_controller_interface_takeoff_and_return(self):
        # This tests the interface gain against an ideal point-mass surrogate of
        # UE's outer position controller, not motor/PID or actual Unreal physics.
        payload = request_payload()
        minimum_actual_separation = float("inf")
        def tick():
            nonlocal minimum_actual_separation
            result = self.bridge.step(payload)
            for item, target in zip(payload["drones"], result["targets"]):
                position = np.asarray(item["position_m"], dtype=float)
                velocity = np.asarray(item["velocity_mps"], dtype=float)
                point = np.asarray(target["target_m"])
                self.assertLessEqual(float(np.linalg.norm(point-position)), 0.700001)
                self.assertTrue(np.all(point >= [-7.75, -3.25, 0.25]))
                self.assertTrue(np.all(point <= [7.75, 3.25, 4.25]))
                desired = (np.asarray(target["target_m"])-position)*0.8
                velocity += (desired-velocity)*1.6*payload["dt"]
                position += velocity*payload["dt"]
                item["position_m"] = position.tolist()
                item["velocity_mps"] = velocity.tolist()
            minimum_actual_separation = min(minimum_actual_separation, result["min_separation_m"])
            return result
        for _ in range(80):
            result = tick()
        minimum_height = min(item["position_m"][2] for item in payload["drones"])
        self.assertGreater(minimum_height, 1.0, "Guidance interface still creeps along the floor")
        self.assertTrue(any(target["waypoint"] > 0 for target in result["targets"]))
        # Exercise the full crossing return, rather than only descending beside
        # the launch pad: both teams start at the opposite end of the arena.
        for item in payload["drones"]:
            item["position_m"] = [-item["home_m"][0], item["home_m"][1], 2.5]
            item["velocity_mps"] = [0, 0, 0]
        payload["mode"] = "return"
        returned_after = None
        for index in range(600):
            result = tick()
            if index == 0:
                self.assertTrue(all(target["stage"] == "return-altitude" for target in result["targets"]))
            if all(item["position_m"][2] < 0.55 and
                   np.linalg.norm(np.array(item["position_m"][:2])-item["home_m"][:2]) < 0.45
                   for item in payload["drones"]):
                returned_after = (index+1)*payload["dt"]
                break
        self.assertIsNotNone(returned_after, "Cross-arena return guidance did not reach home within sixty seconds")
        self.assertGreaterEqual(minimum_actual_separation, 1.1, "Nominal staged return lost the 1.1m separation envelope")
        print(f"Ideal outer-controller interface: min altitude after 8 s={minimum_height:.3f} m; cross-arena return={returned_after:.1f} s; min separation={minimum_actual_separation:.3f} m")

    def test_concurrent_requests_and_budget(self):
        payload = request_payload()
        start = time.perf_counter()
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda _: self.post("/step", payload), range(40)))
        self.assertEqual(self.bridge.requests, 40)
        self.assertTrue(all(len(result["targets"]) == 10 for result in results))
        self.assertTrue(all(result["source_commit"] == self.bridge.commit for result in results))
        median = sorted(result["processing_ms"] for result in results)[len(results)//2]
        self.assertLess(median, 100.0, "Median compute cost cannot sustain ten Hz")
        print(f"Bridge 40 concurrent requests: compute median={median:.3f} ms; wall={time.perf_counter()-start:.3f} s")

    def test_conflict_and_infeasible_target_spacing_are_honest(self):
        payload = request_payload(); payload["drones"] = payload["drones"][:2]
        payload["drones"][0]["position_m"] = [-1.0, 0, 2]
        payload["drones"][1]["position_m"] = [1.0, 0, 2]
        payload["drones"][0]["velocity_mps"] = [1, 0, 0]
        payload["drones"][1]["velocity_mps"] = [-1, 0, 0]
        result = self.post("/step", payload)
        self.assertGreater(result["conflicts"], 0)
        self.assertAlmostEqual(result["min_separation_m"], 2.0)
        # Ten coincident bodies cannot all receive targets 1.1m apart inside 0.7m
        # horizons. Report that physical constraint rather than claiming safety.
        payload = request_payload()
        for drone in payload["drones"]: drone["position_m"] = [0, 0, 2]
        result = self.post("/step", payload)
        self.assertEqual(result["min_separation_m"], 0)
        self.assertTrue(result["target_spacing_limited"])


if __name__ == "__main__":
    unittest.main(verbosity=2)

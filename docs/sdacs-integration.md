# Actual SDACS integration

The adapter imports the separately cloned [swarm-drone-atc source](https://github.com/sun475300-sudo/swarm-drone-atc). Reviewed upstream commit: `1e9d826265069696d7e415c7778d17483d7a8580`. It calls that checkout's actual `simulation/apf_engine/apf.py`; Unreal remains the authority for positions, velocities and physical forces. The source checkout is not modified and bytecode writing is disabled before importing it.

## Setup and provenance

From this project directory, select an external source path:

```powershell
$SdacsRoot = [IO.Path]::GetFullPath((Join-Path $PWD '..\swarm-drone-atc'))
.\Scripts\Setup-Bridge.ps1 -Python 'C:\Path\To\python.exe' -RepoRoot $SdacsRoot
.\.venv\Scripts\python.exe .\Bridge\sdacs_bridge.py --repo $SdacsRoot --host 127.0.0.1 --port 8766
```

`GET /health` reports the source origin, commit, loaded APF file and SHA-256, NumPy version, guidance revision and compute backend. The service binds only loopback. Missing source/NumPy prevents startup. An upstream exception returns HTTP500 without fabricated guidance. The current guidance revision is `fida40-staged-return-v2`.

```powershell
$env:SDACS_REPO = $SdacsRoot
.\.venv\Scripts\python.exe .\Bridge\test_sdacs_bridge.py
```

Eight tests passed on Python3.12 / NumPy2.2.6: real upstream calls and provenance, validation/error behavior, mode/reset transitions, takeoff clearance/relaunch, controller-interface return, concurrency budget and honest conflict/infeasible-spacing reports. Forty concurrent10-drone requests measured2.472ms median APF computation in the recorded R4 check. The idealized external-controller test is not a substitute for actual Unreal flight tests; see [verification](verification.md).

## Unreal contract

Unreal sends ten actual poses at10Hz to `POST /step`:

```json
{"dt":0.1,"mode":"mission","drones":[{"id":0,"position_m":[-6.5,-2.5,0.35],"velocity_mps":[0,0,0],"home_m":[-6.5,-2.5,0.35]}]}
```

The example shows one record for readability. Modes are `mission`, `hold`, `return`; IDs0–4/5–9 are Blue/Red. Responses identify source and commit and contain id/target_m/waypoint/stage, actual minimum separation, conflicts and target-spacing feasibility. `POST /reset` resets adapter state without moving any Unreal body.

The adapter uses upstream `APFState`, dictionary `APF_PARAMS`, `batch_compute_forces`, and `force_to_velocity`. Indoor parameters use metres,2.4m normal repulsion influence,4m/s² upstream force limit and2m/s guidance speed limit. Upstream cruise-height correction is disabled and its5m floor-influence term is retained at reduced gain. These are experimental indoor choices, not calibrated drone specifications.

Each short target is bounded by x±7.75m,y±3.25m,z0.25–4.25m and a0.7m horizon. APF velocity advice uses1.25s lookahead to match the local position controller's0.8/s outer gain. Targets are projected toward1.1m separation; infeasible initial overlaps are explicitly reported rather than claiming guaranteed clearance. Actual physical separation depends on force tracking, latency and contact.

Takeoff first climbs vertically below the goal body's bottom, before crossing the court. Patrol endpoints use x±7.3m and role-specific heights. Return aligns Blue/Red to1.0/2.2m corridors before home transit and landing. Return APF influence is reduced to1.1m so grounded neighbours on1.25m-spaced pads do not repel returning aircraft indefinitely. Homes come from Unreal's actual role layout, including mirrored Red positions. A deterministic lateral yield helps break symmetric encounters.

Keyboard and native USB manual control are local to Unreal. Official match mode is manual LOS; APF guidance is available only in Practice. A stale bridge link stops new mission guidance and requests local hold; it does not create synthetic telemetry or teleport actors. Telemetry speed includes vertical motion.

The upstream full outdoor simulator uses kilometre-scale ATC geometry and a60m cruise. Its demo FastAPI telemetry contains synthetic sine-wave positions. Neither is substituted for indoor soccer-body physics or reported as actual flight state here.
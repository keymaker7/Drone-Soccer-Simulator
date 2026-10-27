# Codex working notes - Imperial Wings Drone Soccer Simulator

- Read `README.md`, `IMPERIAL_WINGS_CODEX_TASKS.md`, `docs/verification.md`, and
  `docs/model-reference.md` before changing the project.
- The current user goal is Windows Unreal drone soccer with **LOS control by
  default**, from a pilot outside the selected team's own end. View 1 is LOS,
  view 2 is an external observer, and view 3 is the arena overview. Preserve
  legacy API names where needed; do not make an onboard/FPV camera the default.
- Right-drag and wheel affect the LOS head direction and field of view. Keep
  Canvas HUD input usable when changing views or possessing another aircraft.
- Preserve Unreal module and public C++ API names. Keep flight based on actual
  motor forces and torques; use position teleportation only for explicit reset.
- Use the documented FIDA 2026 Class40 profile: 16 x 7 x 4.5 m recommended court,
  40 cm ball, 60 cm goal opening, chosen legal goal centre height 3.0 m.
  Read `docs/rules-sources-2026.md`; do not mix FAI and FIDA specifications.
- Official match play is manual. SDACS autonomous guidance is a Practice aid.
  A match start must not arm or launch aircraft automatically; respect the
  Ready/Break/Finished arming gates and the selected input source.
- Default to keyboard and disarmed motors until the actual Boxer/USB device,
  channel mapping, ranges, neutral values, inversion, and low throttle are
  verified. Never fabricate physical controller input or calibration evidence.
- Support user-selected Mode 1 (right vertical throttle) and Mode 2 (left
  vertical throttle, legacy default). Preserve staged setup and explicit
  review/save/enable; do not remap a saved profile on layout selection. R5 tested idle rejection and
  cancel with the actual Boxer, not completed physical mapping or USB flight.
- The user subsequently requested ANGLE for every aircraft. Manual roll/pitch
  now command a 35-degree maximum target tilt and self-level at neutral; yaw and
  throttle remain manual. Do not restore Acro as the default. R6 verified ten
  actual Chaos bodies at 20/30/60/120/240 FPS and ten LOS stations/pad locations.
- Actual Boxer calibration is now saved: X/Y/V/Z for roll/pitch/yaw/throttle,
  normal directions, genuine full ranges and neutral/low-throttle confirmation.
  R6 loaded it and enabled USB with all aircraft disarmed (45 UI checks).
  Continuous physical USB flight, cable loss and response directions remain
  incomplete. Preserve the local real profile; never replace it with fixtures.
- The airframe is original procedural geometry informed by HELSEL STRIKER V3
  reference material. Do not copy manufacturer meshes, logos, manual pages or
  photographs into the repository. Use citations and distinguish visual
  observations from documented dimensions and prototype assumptions.
- Treat advanced physics coefficients as experimental unless real measurements
  support them. Do not claim exact STRIKER dynamics, calibrated IMU/motors,
  independent fixed-thread PID, USB latency, or contact fidelity without tests.
- Never claim an Unreal compile, actual runtime flight result, complete UI
  workflow or Boxer mapping without local evidence. Keep previous R2 results
  separate from tests of newer LOS, mesh and physics changes.
- Record milestone files, limitations and concrete test outcomes in the task
  checklist and verification document. Continue the highest-value incomplete
  item, keeping edits scoped and reversible.
- The user requires local testing before GitHub publication. Validate the changed
  feature, the packaged Unreal runtime, and the relocated release ZIP before
  committing/pushing a release or uploading its executable. Compare the tested
  build fingerprint with the published source and archive checksum. If a required
  local check fails, fix it and rerun the affected checks before publication.
  Keep physical-controller/other-PC checks explicitly pending when unavailable;
  never present synthetic input or same-PC tests as physical hardware validation.
- Keep machine-specific UE/Python/SDACS paths configurable. Preserve upstream
  SDACS provenance and licence; use the real APF source, not synthetic telemetry.
- Keep generated binaries, environments, reports, calibration data, downloaded
  reference PDFs/images and credentials out of Git. Preserve authored Unreal
  `.uasset`/`.umap` and original `ArtSource` textures as project source assets.

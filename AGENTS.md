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
- Keep machine-specific UE/Python/SDACS paths configurable. Preserve upstream
  SDACS provenance and licence; use the real APF source, not synthetic telemetry.
- Keep generated binaries, environments, reports, calibration data, downloaded
  reference PDFs/images and credentials out of Git. Preserve authored Unreal
  `.uasset`/`.umap` and original `ArtSource` textures as project source assets.

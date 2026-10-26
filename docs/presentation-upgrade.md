# Presentation upgrade — 2026-10-26

This change adds `AImperialPresentationEffects`, spawned by the game mode. It is
separate from the aircraft controller, referee, USB setup and existing net solver.
No flight force, motor health, role, arming rule or scoring rule is changed here.

## Contact and rotor effects

- A small translucent floor-dust burst uses the aircraft's actual hit impulse,
  contact normal and contact position. Contact delta-v is estimated from impulse
  and aircraft mass. Resting contacts are suppressed with a threshold and cooldown.
- An armed aircraft above 3 N thrust, within 18–65 cm world height, can stir small
  ground motes. A ground trace must confirm an approximately horizontal floor.
  This samples existing thrust; it does not add a second downwash force.
- Dust is cosmetic and intentionally restrained on this indoor court. Surface dust
  loading is an assumed presentation choice, not measured contamination.
- No arbitrary sparks, explosion, broken propeller or motor-failure animation is
  substituted for a real damage model. Damage and Striker succession remain separate
  planned work.
- One fixed instance pool caps all dust at 96/192/256 particles. Expired particles
  are hidden and reused. Idle frames do not update a hidden pool's transforms.
  Cosmetic randomness uses a private stream and does not affect flight randomness.

## Image settings

The default `High` profile uses native 100% screen percentage, anisotropy16,
restrained sharpening, low bloom, slightly increased contrast and neutral colour.
Motion blur, chromatic aberration and film grain remain disabled for LOS clarity.
Two broad, low-intensity fill lights help separate the white cage and dark frame.
Lumen lighting/final-gather/reflection quality is1.5 in High,1 in Balanced and2 in
Cinematic. Balanced uses83% internal resolution; Cinematic uses100%. These are
starting presets, not certified GPU performance targets. No ray-tracing hardware
requirement or new manufacturer asset is introduced.

Command-line controls (not yet a settings-menu feature):

```text
-ImperialVisualQuality=Balanced
-ImperialVisualQuality=High
-ImperialVisualQuality=Cinematic
-ImperialNoDust
-ImperialBaselineVisuals
```

`ImperialBaselineVisuals` disables this actor's grade/fill/dust for a comparison.
The quality settings are not written into a user's saved settings. This actor is
intended for one game world; multi-world editor CVar restoration is not implemented.

## Verification

- Editor C++ compile succeeded: `Saved/presentation-editor-build.log`.
- Final editor cosmetic fixture passed Balanced96,High192,Cinematic256. Each
  reached its cap, retained exactly that many mesh instances and expired all motes.
  Evidence: `Saved/Verification/presentation-20261026-115828-013`.
- Fresh actual Chaos scenario at60 fixed game FPS passed hover, floor/wall contact,
  elastic net response and physical Striker goal. Hover error0.284cm, maximum net
  deflection6.721cm. Evidence:
  `Saved/Verification/runtime-20261026-115642-815/results.json`.
- Actual1920×1080 GPU renders were captured and visually inspected:
  `Saved/Verification/presentation-before.png` and `presentation-after.png`.
  These are editor-game offscreen renders. Their HUD FPS is not a paired benchmark:
  another physics test overlapped the second render.
- The fixture stresses particle allocation/lifetime only, with explicitly synthetic
  cosmetic bursts. It neither arms aircraft nor proves real contact/downwash
  appearance. The Chaos test is distinct from that cosmetic fixture.
- Full UI regression, sustained10-aircraft GPU benchmark, visual calibration on
  RTX5060 and real USB flight were not repeated for this presentation change.
- Isolated Windows build/cook/stage/package/archive succeeded, ExitCode0:
  `Saved/presentation-isolated-package-console.log`. Output is
  `dist/PresentationUpgrade/Windows`, preserving the currently running package.
- Fresh packaged cosmetic fixtures passed all three presets:
  `Saved/Verification/presentation-20261026-120323-271`.
- Fresh packaged Chaos60Hz scenario passed:
  `Saved/Verification/presentation-packaged-physics-60.json`.
- Packaged1920×1080 render was visually inspected after15 seconds of warm-up:
  `Saved/Verification/presentation-packaged-warm.png`. An earlier6-second capture
  had incomplete HUD glyphs; the15-second capture showed complete text. First-use
  font/render warm-up remains a limitation to investigate, not a passed full UI test.

Reproduce with `Tests/run-presentation-tests.ps1`, or add `-Packaged` after building
the Windows package. For the isolated build, also pass
`-PackageRoot E:\ImperialWingsSimulator\dist\PresentationUpgrade\Windows`.
Tests use unique output paths and require a fresh successful
process result; they cannot pass using an older JSON file.

Quality tuning follows the distinction between Lumen quality and GPU cost in
[Epic's performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine).

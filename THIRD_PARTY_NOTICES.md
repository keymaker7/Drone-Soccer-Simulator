# Third-party notices

## SDACS / swarm-drone-atc

The guidance bridge imports the separately installed
[swarm-drone-atc repository](https://github.com/sun475300-sudo/swarm-drone-atc).
The reviewed source revision is `1e9d826265069696d7e415c7778d17483d7a8580`.
Its actual APF module remains the source of the batch APF and velocity-conversion
functions. The following notice is copied verbatim from that revision's `LICENSE`
and applies to that upstream software.

```text
MIT License

Copyright (c) 2026 SDACS Capstone Team, Mokpo National University
(장선우 / Sunwoo Jang, Dept. of Drone Mechanical Engineering)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Runtime dependencies

Unreal Engine is obtained separately from Epic Games. NumPy is installed into a
local Python environment from `Bridge/requirements.txt`; its installed package
contains its own dependency and licence notices. Engine files, Python environments
and packaged executables are excluded from this source repository.

## HELSEL reference material

[STRIKER V3 product information](https://www.skyballdrone.com/products/striker-v3-arf-semi-assembled-set-1)
and the
[official HELSEL assembly manual](https://www.helselgroup.com/_files/ugd/f9f7fd_f8c9cc9a0b484b0e9491aad3a0abb093.pdf)
are reference sources. The manual reserves the product/manual rights to HELSEL
and restricts reproduction and publication. No manufacturer manual, photograph,
logo, CAD or mesh is included here. The project's original procedural model is a
reference-informed approximation, with no manufacturer endorsement or measured
flight-equivalence claim.

## Sporting-rule references

FIDA and FAI source documents are cited in `docs/rules-sources-2026.md`. Downloaded
rulebooks and page renders remain local verification inputs under ignored
`Saved/`; they are not part of the public source tree.

## Portable Windows distribution

The release ZIP includes the compiled Unreal game, an original .NET Framework launcher,
a PyInstaller one-folder Python runtime, NumPy, and the MIT-licensed APF source from
the recorded upstream commit. `release-manifest.json` records the APF/LICENCE SHA-256
values. No manufacturer reference media, developer environment, or pilot profile is bundled.
Python and PyInstaller licence notices are in `Licenses`. NumPy's complete BSD and
bundled-library notices are preserved in `Guidance/_internal/numpy-2.2.6.dist-info/LICENSE.txt`.
The upstream MIT licence is also in `Licenses/swarm-drone-atc-MIT.txt`.
Microsoft's signed Visual C++ redistributable is included for the required runtime;
its installer presents its own terms. Unreal runtime notices remain in `Windows/NOTICES.txt`.

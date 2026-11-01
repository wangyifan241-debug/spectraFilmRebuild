# SpektraLab Windows · spectraFilmRebuild

[简体中文](README.zh-CN.md) · [Repository](https://github.com/wangyifan241-debug/spectraFilmRebuild)

A Windows port of SpektraLab: a native Qt Quick editor, C++ spectral film/print
simulation and Vulkan rendering. This repository maintains the Windows application
and its material-library extensions. It is a work in progress, **not a
feature-complete or officially endorsed macOS release**.

## Current functionality

- RAW, JPG/JPEG and PNG (8/16-bit) import, per-photo editing, undo/redo, cropping and geometry.
- Film and print-material selection, enlarger filters, grain, halation and glare;
  slide-film direct scanning and black-and-white material pairing.
- Scene measurement/fitting, post-develop adjustments, original/split comparison,
  film edges, dates and half-frame pairs.
- Versioned custom materials: import, validation, source information,
  enable/disable and removal. Missing materials are reported explicitly.
- Full-size TIFF/JPEG/PNG export and selected-photo batch export.
- A dark Windows interface and an original film-strip application icon.

There are currently 37 runtime material profiles (27 shooting films and 10 print
materials). Pro Image 100 is **not a selectable film**: density consistency and
exposure calibration remain unresolved behind quality gates. Research data and
manufacturer names do not imply completed profiles or brand endorsement.

## Run on Windows

Use a newly built Windows Preview installer, then start **SpektraLab** from the
Start menu. This repository does not yet promise a published binary release.
Never use an old installer marked `NOT-FOR-DISTRIBUTION`.

Requires 64-bit Windows, a Vulkan-capable GPU and a working graphics driver.
Qt and runtime libraries are packaged with the application; recipients do not
need Python, Node or developer tools. Keep the installed files together.
Previews are unsigned: verify the source and accompanying SHA256 before running.
Do not disable Windows security protections.

Installation is per-user. Update by exiting and uninstalling the old preview
before installing into an empty directory. Uninstallation preserves photos and
application edit records. Test photo copies before production use.

## Build and test

Use PowerShell 7. Dependency setup, pinned versions and corresponding-source
requirements are documented in [the Windows guide](windows_UI/README.md).

```powershell
./windows_UI/build.ps1 -SourceDirectory C:/src/SpektraLab `
  -BuildDirectory C:/src/SpektraLab/build/windows `
  -DependencyDirectory C:/src/deps -Jobs 6 -RunTests
```

Use an ASCII path or a directory junction if MinGW cannot handle your checkout
path. Start the deployed `app/SpektraLabQt.exe`; do not move the executable
away from its DLLs, resources or licenses.

The Windows guide also describes NSIS packaging. Packaging requires a tested
Release build, an exact source inventory and verified corresponding sources
for the shipped Qt SDK. Stale or mismatched builds are refused. Packages carry
source archives and notices, not test RAW photographs or GPU drivers. Installed
startup, overwrite refusal, exact-file uninstall and source verification are
separate gates, not substitutes for an independent clean-PC test.

## Limits and status

The editor uses a longest-edge 3400-pixel preview; export renders at native size.
Some post-develop/geometry operations still use CPU paths. HDR, complete output
color-space options, full macOS UI/automation parity and cross-device numerical
and performance validation remain unfinished. Ordinary post-develop numeric
sliders apply on release; not every control is continuously GPU-live.
Developer-machine NVIDIA validation does not certify other GPUs or clean
recipient systems. See [the port plan](windows_UI/WINDOWS_PORT_PLAN.md) and
individual validation records for actual results and failures.

Original macOS sources remain under `modern_UI/`; architecture documents may
describe that product. Windows UI/build code is under `windows_UI/`; shared
engine code/resources are under `engine/`. There is no Python renderer at runtime.

## Origins and licenses

Based on [James Qiu's SpektraLab](https://github.com/JamesQiu2005/SpektraLab)
and [Andrea Volpato's spektrafilm](https://github.com/andreavolpato/spektrafilm).
Windows-specific changes are maintained in this repository. Code is
[GPL-3.0-or-later](LICENSE); original attribution and copyright notices remain.
spektrafilm profiles/derived LUTs are CC BY-SA 4.0. Added materials have their own
provenance and notices. Qt, LibRaw, compiler runtimes and other dependencies
retain their respective licenses. Consult packaged `licenses/`, `Source/`,
material manifests and resource notices before redistribution.
Film and manufacturer trademarks belong to their owners.

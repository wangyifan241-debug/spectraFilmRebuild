# Windows responsiveness and export agreement — 2026-11-01

Status: runnable Release build; Draft PR #3 remains for review. This is not a formal release approval. No installer was rebuilt in this change.

## Confirmed defects and changes

| Location | Evidence / change |
| --- | --- |
| `windows_UI/src/PreviewController.cpp:220` | Post adjustments, straighten and comparison-image preparation formerly ran synchronously on the GUI thread. A worker view lane now prepares immutable images, coalesces revisions and publishes only the current frame/revision. It cannot clear an engine operation. Immediate export and exception recovery have regression cases. |
| `engine/host/colour_transform.cpp:87,231` | A transfer-function table was rebuilt for every band and a fresh thread team created for each conversion. Cache the exact same table and reuse a bounded worker team; concurrent callers and worker exceptions are tested. No CCTF or colour matrix was changed. |
| `windows_UI/qml/Main.qml:747,782` | The destination dialog opened while its modal export popup was still active. Open it after popup closure. The QML workflow activates the actual batch button, accepts an owned folder, checks its PNG, repeats with the same name and verifies unchanged bytes plus explicit skipped-file feedback. The automated folder uses Qt's non-native dialog option; the physical Windows folder picker still needs visible manual acceptance. |
| `windows_UI/src/PreviewController.cpp:417`, `Main.qml:516` | Export success includes the destination. A persistent result banner makes write/skip/error feedback visible. All-skipped batches explicitly suggest an empty folder. Existing files remain protected. |
| `engine/src/pipeline/engine.cpp:1106`, `pipeline.cpp:993` | CineStill's headroom normalization used a different global maximum at preview and native resolution. Measure the full-source pre-headroom maximum in 128-row bands and cache it for the live tier. Native resolution retains its previous path. No measured profile, spectral equation, halation strength, PSF or shader was edited. No public API/schema field was added. |

The source-reference calculation performs input decode/geometry, the session's same meter EV, placement/upsampling/exposure, and optional boost. The maximum is global, including a partial final band. With boost enabled it makes two passes using the global pre-boost denominator. Its cache is invalidated with shoot/film changes. It consumes no film grain/glare random stream. It adds work to the first applicable preview, rather than compromising the native file to mimic a downscaled image.

## Windows validation

Windows 11 x64, NVIDIA RTX 3050 Laptop (4 GiB), Qt 6.11.3, MinGW GCC 13.1; actual Vulkan backend. About 16 GiB system RAM. An existing user app remained open, so wall timings are observations on this machine, not controlled cross-machine benchmarks.

Build/test command (PowerShell, ASCII junction to the same checkout):

```powershell
& C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1 `
  -SourceDirectory C:/Users/Strickland/SpektraLab-work `
  -BuildDirectory C:/Users/Strickland/SpektraLab-work/build/qt-responsive `
  -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 `
  -Jobs 6 -RunTests
```

Release compiled. **37/37 CTest passed, 0 failures, 0 skipped, 322.38 seconds**. See `CTest.txt` and `qt-build.json`. The QML integration reports 235 fields and scene captures; it uses a hidden Windows Qt window, not a visible human interaction test. The new source-reference test passes 8 full-plane/banded numerical comparisons (portrait/landscape, black/outlier, boost 0/3, partial final band), exactly equal float maxima. Existing colour, material, encoding and GPU tests passed unchanged.

Additional owned synthetic-DNG controller workflow:

```powershell
./spk_qt_editing_workflow_test.exe ./app/resources `
  ../editing-responsive-fixture/owned.dng ../editing-responsive-fixture/bad.NEF `
  ../editing-responsive-evidence
```

Exit 0; **114 checks passed** (`editing-report.json`). Fixtures generated with `owned_fixture.cpp`; this is not real-camera validation. Store/undo/redo/crop/straighten/persistence/export and rejected-input recovery exercised.

Earlier integration attempt: **35/36 passed**, with QML `crop_changes_display_without_redeveloping` failing because it inspected the picture synchronously after crop became asynchronous. The test now waits for completion and preserves the pixel and frame-identity assertions. The final run above includes the new batch UI path and new source-reference test. No failed gate was removed/skipped. One standalone probe initially used the wrong resource directory and timed out; it was rerun with the same `app/resources` path as CTest.

Build source inventory and executable SHA256 were independently recalculated after tests and both match `qt-build.json`:

```text
source 3a787c4f4ea355818317e216f5f12dd4506f7ab0a537a311b89f8a978666f360
exe    56bca624713ce12e79d13bed644807d9828f50c1e3a6dda7e16e21a008082200
```

## Responsiveness measurement

`probe.cpp` uses an owned uniform 2400×1600 PNG, private edits directory, grain/halation/glare off, then +0.4 EV and 2° straighten. It measures setter duration, completion, a 5ms GUI heartbeat, and active-image hashes. Old and new static probes were alternated sequentially in the same session using their corresponding compiled controller objects/libraries. Neither measure proves visible gesture smoothness.

| Measurement (ms) | Old | New |
| --- | ---: | ---: |
| Post setter blocks GUI | 1189 | 0 |
| Maximum heartbeat gap during post update | 1189 | 14 |
| Post picture ready | 1189 | 1474 |
| Straighten setter blocks GUI | 1271 | 0 |
| Maximum heartbeat gap during straighten | 1272 | 17 |
| Straighten picture ready | 1271 | 2922 |
| First original getter **including hash** | 1966 | 83 |
| Uncropped getter **including hash** | 1289 | 83 |
| 100 histogram reads | 185 | 2 |
| Initial open including comparison preparation | 2195 | 4600 |

Raw measurements: `performance-old.json`, `performance-new.json`. All four compared image hashes match. Geometry correctness also has heterogeneous fixtures in the separate workflow tests. Hashing the cached image accounts for the ~83ms getter measurement; production getters return shared cached QImages. This change improves GUI availability, **not total render latency**: initial opening now prepares comparison images, and straighten prepares the matching original too. Further GPU-side post processing/lazy comparison preparation remains an optimization opportunity. Do not advertise an overall render speedup.

Standalone build recipes: from `build/qt-responsive`, invoke the same MinGW `g++` with `-std=c++20 @CMakeFiles/spk_qt_responsiveness_test.dir/includes_CXX.rsp`, `probe.cpp`, that test target's `mocs_compilation.cpp.obj`, `PreviewController.cpp.obj`, `FrameOutput.cpp.obj`, `EditStore.cpp.obj`, and `@CMakeFiles/spk_qt_responsiveness_test.dir/linkLibs.rsp`. Set PATH to the MinGW/Qt bin folders and `QT_QPA_PLATFORM=offscreen`; pass `./app/resources`. Old probe uses the corresponding old target objects. `highlight_golden.cpp` needs only its source and the raster-input test's include/link response files.

## Real Sony ARW preview/export reproduction

The user's DSC03865.ARW and its saved CineStill 800T / Portra Endura / 35mm settings were read locally. Every render opens an owned RAW copy in ignored `build/export-repro-*`; the edit store is read-only. No user photo or rendered image is included in Git. The original TIFF initially reproduced the issue; an independently rendered fresh native TIFF reproduced it too, disproving a stale-file-only explanation. Native display pixels also reproduced it before file encoding, excluding an ICC-label-only explanation.

Measured pre-headroom maximum:

```text
old preview 3400×2428: 30.73447227
native      7168×5120: 49.60367203
new preview 3400×2428: 49.60367203 (automatically measured; no override)
```

A temporary manual-maximum diagnostic established causality and was removed. `SPEKTRALAB_HEADROOM_REFERENCE` is absent from production sources. An optional `SPEKTRALAB_HEADROOM_LOG` diagnostic remains.

The user's original TIFF was no longer present at its supplied Downloads path on the final repeat: the first repeat returned error after successfully producing the corrected live picture. The repeat using the owned fresh native TIFF from the earlier successful reproduction exited **0**. This is recorded rather than presented as a successful reread of a missing file. No original photograph was overwritten.

`image_compare.cpp` scales both previews to the same 1400px native-display reference and computes encoded sRGB differences over all RGB samples:

| Native-reference difference, 8-bit code units | Before | After |
| --- | ---: | ---: |
| Mean absolute error | 6.00835 | 2.83084 |
| RMSE | 16.2152 | 4.51723 |

This **is not pixel identity or a perceptual ΔE**. Grain, resampling, resolution-dependent spatial filters and quantization still differ between preview and original-size render. The confirmed global-normalization discrepancy is fixed; residual agreement needs more camera/image coverage.

Same-machine deterministic synthetic render (`highlight_golden.cpp`, grain/glare disabled) proves the native image did not change:

```text
full before/after a56b4f55cbb4e5ff77c209aade214dd95a23d6df762cbe01587b9d6dd8ffebe1
live before      fe8591ab1c31da8053dfb687ff2db8bcd182c5049c64b7be1382b6717c86df69
live after       e441aed41f233e982e55153249a910113d57633ecf12a42481f13fe14efc5674
```

These hashes are evidence on this GPU, not portable goldens imposed across GPU vendors. The permanent test compares full-plane and banded statistics numerically instead.

## Remaining limits

No macOS build, Python oracle parity, clean-Windows installation, visible native picker test, other-GPU test, or new installer validation was performed here. Existing review.2 installer predates these changes. Native exports can still take time; first applicable preview must measure its source reference. The test PNG timings and synthetic DNG checks are explicitly separate from the real Sony RAW reproduction. Formal release blockers in the earlier PR review still apply.

Run the new deployed application at `build/qt-responsive/app/SpektraLabQt.exe`; close the older application normally first. The source, Qt DLLs, plugins, licenses and engine resources for this build are colocated in `app/`.

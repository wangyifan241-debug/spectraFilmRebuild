# Integrated Preview 0.2 — verification and remaining issues

Status: Ready for User Test (not Accepted/Done). Build code: develop / d0afe41a70a7c3bd7e0fd94bc4d2502aae779e5c. Release, Qt 6.11.3, MinGW-w64 13.1.0, LibRaw 0.22.2, Vulkan. Full hashes and paths: DELIVERY.json, qt-build.json, SHA256SUMS.txt.

## Results actually run

- Final Windows CTest: 43/43 passed, zero skipped, 335.87 s. Includes native Vulkan math/transfer/grain/DI and bounded blur oracles, legacy C ABI, material library/import, Qt geometry/frame/date/pair, persistence/close recovery, Process/scene fitting, float transport, capability detection, full offscreen QML workflow. This is Windows validation; no macOS/Python oracle run claimed.
- Real Sony DSC02443.ARW copied into owned fixtures; both round5 generations render 732x1024 through the actual Vulkan scanner. Generation pixels differ; controller destruction/recreation, saved recipe restore, photo switching and Portra/CineStill roundtrip are exact. See sony-round5.json.
- Separate actual Sony Process workflow: 192 checks passed, including controlled decoded-radiance gain, solved/effective reprint parameters, history, reopen, native export and readback. Only this fixture/recipe is certified by this run; round5 original export remains blocked.
- Actual qwindows main EXE rendered both candidates; screenshots inspected at build/integrated-validation/harman_phoenix_200_round5.png and harman_phoenix_ii_200_round5.png. Both show selected material, actual canvas, navigator, histogram and unverified/proxy labeling. These are native Qt window scene captures, not Windows compositor/physical monitor calibration.
- Native main long self-test: FAILED after successful RAW, canvas, fit/100%, Process QML handlers, actual neutral C/M/Y, paired undo/redo, repeat reprint and visible diagnostics. Failure: automatic brightness preview did not run within the harness's 5-second stage deadline. Full raw report retained in native-main.json. Subsequent stages did not run in this native attempt. Do not describe it as an entirely passed native workflow or assert the timeout's cause is proven.
- Installer: 1,728 installed file hashes verified, isolated PATH startup, overwrite refusal and safe uninstall passed, recipient file and original registration preserved. Local Windows machine only, not clean-machine certification.
- Computer-use initialization failed twice (kernel assets, os error 3); physical pointer automation not verified. Existing user-opened round5 app was left untouched.

## Function disposition

| State | Functions |
|---|---|
| Integrated / usable | Existing RAW/raster import, stable film/paper, before/after/split, pre/post editing, crop/rotate/flip/straighten, histogram/navigator/zoom, library/per-photo state/history/save/reopen; custom versioned material library; scene fitting; frame/date/half-frame; sRGB positive exports and ProPhoto negative TIFF; tested installer/uninstaller |
| Integrated / experimental | Phoenix I/II Round 5 real Vulkan <=1024 preview with distinct IDs and saved recipe; Process neutral reset/reprint plus effective-state diagnostics; automatic negative inversion |
| Integrated / groundwork | Windows Advanced Color detection, pre-clamp RGB32F engine transport. Current application output remains SDR |
| Not integrated into main editor | Earlier 23-candidate fixed/frozen/recalibrated research selector and diagnostic DLL modes; preserved original worktrees/branches. Third-party comparison ingestion remains design, not an implemented product module |
| Blocked / unfinished | Round5 original export and frame/date/pair/scene fit/Process combinations; true HDR canvas/switch/export; physical film accuracy and independent real-film calibration |

## Remaining issues only (no automatic development task)

1. Native Sony long workflow's 5-second automatic-brightness stage failed; user should check edit completion/interaction on the provided app. Offscreen full workflow and separate real RAW controller workflow passed but do not erase this failure.
2. Round5 native export is explicitly refused after historical device-lost; no upscaled substitute. Native positive export succeeded only for the tested standard-material path.
3. Round5 scanner does not use paper/filter/glare or print-session Process; unsupported combinations are refused/disabled. Some existing controls remain visible; use USER-TEST.md for applicability.
4. Actual HDR rendering/export, calibrated/HDR display, clean recipient-machine, multi-GPU and exhaustive material/output combinations remain unverified.
5. Earlier round2/3 research selectors remain available in isolated historical worktrees rather than a second main-editor renderer.
6. Packaged READ-ME-FIRST contains inherited general Preview descriptions (37-material count and 3400 preview/native export). For this integrated build, candidate exceptions and current applicability in USER-TEST.md and this report take precedence: round5 <=1024, no original export. No claim of all-material export success.

## Run and rollback

Run D:/spk-integrated-build/app/SpektraLabQt.exe with the complete app directory, or use build/ip02/payload/SpektraLabQt.exe. Installer: build/ip02/SpektraLab-0.2-integrated-preview-windows-x64-setup.exe. Installation requires a new empty short path; inherited installer registration refuses an existing registered Preview rather than overwriting it. Portable app can be used alongside it. Integrated application data is isolated under SpektraLab / SpektraLab Windows Integrated Preview 0.2, so old-version edit records remain intact. Do not copy only the EXE. Source photos are never rewritten.

Import Sony ARW, select Phoenix 200 Round 5 Experimental / Phoenix II 200 Round 5 Experimental in the same main editor. Compare to corresponding Experimental v1 by switching the film. Close/reopen and reimport the same source to restore edits. Detailed manual checks: USER-TEST.md. Git baseline/each PR disposition/conflict choices and backup: INTEGRATION.md.

No PR created/merged, no main change, no push, no new research. Stop and await user acceptance.

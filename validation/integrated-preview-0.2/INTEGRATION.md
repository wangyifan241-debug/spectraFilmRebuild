# Windows Integrated Preview 0.2 — integration record

Status: Ready for User Test. User acceptance pending. Physical Film Accuracy: Not Verified.

## Baseline and protection

Integration branch: develop. Started from 00405ee8 (complete Windows Qt/Vulkan application plus Phoenix v1 and validation infrastructure), NOT the landing-page main. 6053f948 preserves all pre-existing tracked/untracked work, including negative inversion, native negative TIFF, edit-close recovery and unified AGENTS rules. Before mutation, changed/untracked files from every worktree and all Git refs were copied to D:/学习资料/SpektraLab-integration-backup-20261104. Original worktrees remain intact.

Selected source commits: 34498abe (round4 scanner/reference), 3b618ed8 (round5 evidence), e8b9abfb (main-editor round5), 493c7e85 + e4b654ff (Process and bounded Vulkan blur submissions), 6a0ea1fe (Advanced Color), 72f4865e (RGB32F transport). Applied individually with conflict inspection; no bulk PR merge, push, or stable-main change. Existing docs add/add conflicts selected the later round4 executable specification; previous versions remain in 6053f948 and backup. Build-list conflicts retain both DI encoding and round4 scan kernels, close recovery and capability tests. Render-original conflict retains both the negative TIFF path and explicit round5 export refusal.

## PR audit and disposition

- #1 superseded by #2; complete Windows baseline already present. No second import.
- #2 Qt/Vulkan/RAW/material library/scene fitting/geometry/frame/date/pair/export/install: retained existing implementation.
- #3 async edits, history, versioned exposure, atomic export, install safety: already ancestral; retained.
- #4 Phoenix v1: retained original versioned IDs/resources, no tuning.
- #5 automatic negative inversion/base removal/native negative TIFF: preserved from local working tree; dedicated DI shader and shared output transform retained.
- #6 provenance, paired-data integrity, held-out validation: baseline tools retained; historical results do not count as this build's execution.
- #7 round2 experiments and corrected color measurements: retained source branches and local research files. Inconclusive candidates not promoted to defaults.
- #8 third-party benchmark and ingestion design: preserved docs; no nonexistent benchmark executable claimed.
- #9 round3 fixed-gain and spectral diagnosis: retained original branch, not installed as a second renderer or physical calibration.
- #10 23 research candidates / fixed-gain experimental modes: retained isolated worktree; not added wholesale to main editor. They depend on research build flags/frozen gain and are not required for the chosen round5 preview recipe. v1 and round5 main-editor choices retained.
- #11 round4 actual scanner implementation and reference/evidence: incorporated as shared dependency of round5.
- #12 corrected same-session solve/reprint, effective-state diagnostics, undo/redo and bounded IIR/FIR submissions: incorporated. Round5 spectral scans explicitly refuse Process/scene fitting because they do not use the print-session workflow.
- #13 Advanced Color detection plus pre-clamp RGB32F transport: incorporated. Main canvas remains SDR; no HDR toggle/render/export claim.

## Expected usable functions (verification recorded separately)

Qt main editor, RAW/raster import, standard film/paper selection, pre/post edit, grain/halation/glare, crop/rotate/flip/straighten, before/after and split, histogram/navigator/zoom, per-photo selection/settings/history/save/reopen, custom versioned materials, scene measurement/fitting, film frame/date/half-frame pairing, color-managed sRGB TIFF/PNG/JPEG and native negative ProPhoto TIFF. These existing controls are retained, not newly implemented in this integration.

Experimental: Phoenix 200 and Phoenix II Round 5 Experimental, distinct IDs ending _round5, actual Vulkan film and shared scanner at <=1024 long edge. Old v1 remains separately selectable. Grain/halation optional; glare/paper/filter changes do not participate. Native export, frame/date/pair/scene fitting and Process are unavailable for these candidates. Physical accuracy unverified.

Process on standard print materials: reports existing neutral pack/metering, resets manual Y/M to neutral, forces same-session reprint; neutral may correctly produce no visual change. Not a new subjective or complete automatic color-optimization algorithm.

## Known limits / no automatic follow-up

Round5 native export blocked after prior device-lost; do not upscale preview as native output. True HDR canvas/export unfinished. Clean recipient machine and calibrated/HDR display not certified. Computer-use initialization twice failed with os error 3; native application-driven main-window tests and captures are distinct from physical pointer validation. Preserve all original photographs. User visual acceptance remains required. No new research or PR is to be opened at completion.

## Integration fixes found by execution

First full Windows run: 42/43 passed; spk_qt_review_controller failed at temporary edit-lock retry. The pre-existing independent-session fallback had bypassed transient lock retries. Integration retains bounded retries for that lock; version/source/resource conflicts still have independent-session recovery. A close checkpoint failure now refuses close instead of silently allowing loss. The retry regression retains pending-state/retry-bound assertions; close recovery is covered by the separate real-controller test. Initial failed logs are retained.

The Preview application name/data namespace is now SpektraLab Windows Integrated Preview 0.2. The former SpektraLab Windows data namespace remains untouched. This changes the default storage location, not photo/source identity or existing record contents. --edit-store remains available.

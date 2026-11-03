# Windows Qt Round 5 — Ready for User Test

2026-11-04. This delivery is ordinary preview integration, not final film validation or release acceptance. Physical Film Accuracy: **Not Verified**. Checkpoint: `3b618ed8`.

## Run

Workspace: `C:/Users/Strickland/.codex/worktrees/phoenix-round5/SpektraLab`.

Start `build/round5-editor/app/SpektraLabQt.exe`. Keep the entire app directory (DLLs, QML, resources and licenses). Import a Sony ARW, then select **Phoenix 200 Round 5 Experimental** or **Phoenix II 200 Round 5 Experimental** in the film dropdown or negative-film list. Settings save in the existing per-photo edit store, not beside the RAW. Close and reopen the same source to restore the selection. Original Phoenix v1 and other materials remain available.

Optional isolated launch:

```powershell
./build/round5-editor/app/SpektraLabQt.exe --open C:/Users/Strickland/Downloads/DSC02443.ARW --film harman_phoenix_200_round5 --edit-store C:/Users/Strickland/.codex/worktrees/phoenix-round5/SpektraLab/build/manual-round5-edits
```

Existing per-photo settings take precedence over the CLI initial film selection. For manual testing use a fresh absolute edit-store directory or change the film in the editor.

## Actual integration

`Main.qml` film selection → `PreviewController::setFilmIndex` → version-pinned `RenderSettings` → `PreviewHost::open_raw` / `render` → `render_phoenix_preview` → shared `phoenix5::Scanner<Context>` → Vulkan film development / spectral transmission / fixed inverse curves / fitted matrix → shared luminance shoulder → encoded ProPhoto RGBA16 → existing `FrameOutput` / `OutputTransform` → `FrameCanvas`.

The scanner implementation was extracted from the preserved experiment into `engine/src/desktop/phoenix_round5_scan.hpp`; the experiment now includes that same implementation. No external EXE, static photograph or 3D effect LUT is invoked by the editor. Preview CPU work covers setup, readback, the existing shoulder/ProPhoto encoding and display conversion; film and scanner calculations use Vulkan.

New IDs: `harman_phoenix_200_round5`, `harman_phoenix_ii_200_round5`; version `round5-scan-v1`. Their material data and render-model estimates are exact copies of the respective v1 material. Stock identity, display label and provenance/version metadata differ. The change from R4 to R5 is the scanner-to-ProPhoto matrix solve: neutral-constrained ridge fitting over 320 fixed synthetic probes, rather than a local neutral Jacobian. Sensitivity, dye basis, base density and characteristic curves were not retuned. D50 detector weights, inverse neutral calibration and output shoulder remain the same. This is an output/scanner conversion experiment, not newly measured Phoenix physics.

## Executed checks

- Real Sony `DSC02443.ARW`: both candidates render at 732×1024 through the actual editor controller and Vulkan backend; different pixel hashes.
- Destroy/recreate controller, reload saved image: complete settings equal and preview pixels exact for each generation.
- Switch to Portra and CineStill: real renders succeed; switch back: candidate pixels exact.
- Open `DSC02337.ARW`, then return to first photo: material ID and pixels retained.
- Real Qt main-window captures for both selections, plus a separate application process reopening the saved II recipe. Captures live in `build/round5-editor-I-window.png`, `round5-editor-II-window.png`, `round5-editor-reopened-window.png`.
- Existing independent R5 image comparison: maximum display difference <0.499 of one 8-bit channel step; mean absolute difference ≈0.273/255. The independent image is sRGB16, while the editor preview is sRGB8. This is software agreement, not real-film accuracy.
- No full-resolution render/export or device-lost stress test was executed in this integration phase. No device-lost occurred in the executed ordinary-preview checks.

Evidence: `integration/controller-raw.json`, `integration/pixel-agreement.json`, `integration/acceptance.log`.

## Limits

- Round 5 preview is explicitly capped at 1024 long edge to match the existing research input. Other stocks retain their existing preview policy. A fresh Vulkan context/calibration is built per candidate rerender; latency optimization is deferred.
- Original-size Round 5 export is **Blocked** and fails explicitly before GPU work. Earlier original-size `VkResult -4` remains unresolved. No TDR changes or error suppression.
- Film edges, date backs, pairs and scene-placement mapping are rejected for these experimental scan candidates. Paper, print filters and print glare do not participate. Print brightness remains a positive-scan gain; grain/halation start off on candidate selection and may be enabled. Existing post-develop display adjustments remain on the editor path.
- Ordinary preview passing does not prove full application regression, clean-machine packaging compatibility, monitor calibration, professional colour fidelity or release readiness. The official installer script requires the complete CTest gate; it has not been bypassed. A portable preview can be distributed for manual testing; no signed installer is claimed.
- Colour remains Changed / Unverified. Warmer skin is not proven improvement; orange skin and cyan/purple whites remain risks. Independent public reference originals were not downloaded, so no actual reference-image comparison is claimed.

Stop here for user acceptance. Further colour-model changes and original-size device-lost investigation are separate follow-up work.

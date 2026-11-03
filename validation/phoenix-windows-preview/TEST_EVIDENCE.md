# Preview integration test evidence

## Final source gates

Executed on Windows with NVIDIA GeForce RTX 3050 Laptop GPU, native Vulkan,
float32. Development tests run Qt offscreen where noted; they still invoke the
real Vulkan film renderer. This is not a monitor calibration measurement.

| Gate | Result | Evidence |
|---|---|---|
| 23 materials, Frozen 23 + frozen neutral calibration 14 | 1187 checks pass; 37/37 RGBA16 comparisons exact | normal-results.json; final-normal.log |
| Fixed gain, all 23, isolated process | 781 checks pass; 23/23 RGBA16 comparisons exact | fixed-results.json; final-fixed.log |
| Manifest/profile integrity, 23 UI identities, unsupported mode refusal, JPEG/TIFF import | 149 checks pass | integrity-results.json; final-integrity.log |
| Actual QML workflow and scene captures | pass; 237 report fields | final-qml.log |
| Geometry, preview/exports, PNG8/PNG16/TIFF16/JPEG, ICC, cleanup and no overwrite | pass | final-frame-output.log |
| Linear EV / neutral identity / colour-space conversions | pass | final-colour.log |
| Normal engine option OFF: Portra 400 and Gold 200 vs original research normal binary | 2/2 byte-identical | normal-algorithm-regression.json |

The normal/fixed controller gates each decode an actual Sony ARW, render its
preview and export an original-resolution TIFF; each selected mode exports all
four formats and reopens its recipe with an exact preview match. A synthetic
32×32 sRGB image is decoded by the same GUI host; its exact linear ProPhoto
pixels are sent to the independent, accepted research reference. Comparison is
on native encoded ProPhoto RGBA16 before the Qt display conversion, not on
screenshots. These synthetic parity results do not prove arbitrary-image or
real-film accuracy. The RAW and derived photograph files remain local.

## Failures found and retained

Earlier integration attempts failed and are not counted as final passes:

- Building other UI targets recopied the base neutral table over the complete
  experiment table. A later R2 candidate inherited the previously selected
  filter pack and diverged from the reference. The pack is now the final command
  of the shared resource target; runtime hashes reject the incomplete table.
- A development rebuild changed resources while a gate was running. Final
  controller gates use immutable copied resource snapshots.
- The Qt SDK has no TIFF image plugin. TIFF was also incorrectly routed to
  LibRaw on import. The gate now reads TIFF through the existing native decoder,
  checks the embedded ICC, and the host uses the existing raster path for TIFF.
- Experimental selection initially reset controls when selecting any formal
  material, breaking a slide workflow assertion. Reset now applies only when
  leaving Phoenix; the final QML gate passes.
- Two export/colour command invocations omitted their required resource argument;
  corrected invocations passed. These were invocation errors, not skipped gates.

The retained historical logs distinguish these failures from final gates.
No test exception or threshold was added to make a candidate pass.

## Standalone package and native large-RAW follow-up

The local delivery directory is `output/PhoenixPreview-0.1-local/`.
`package-integrity.json` verifies all 1713 packaged files and the exact executable
against the completed build. The package includes Qt/compiler runtimes,
corresponding application/library source, SDK source overlay and notices.
An intentionally substituted executable is rejected before creating output
(`package-negative-control.json`). The material/manifest controls now each use
a complete valid resource fixture and assert their own specific error: removing
one profile cannot accidentally pass because another required resource is absent.

The application was launched from an unrelated working directory with only
Windows system PATH entries and Qt development variables removed. Real ARW
pixels were displayed in its real Windows canvas, with v1 material identity
confirmed in the saved recipe and accessibility tree. No diagnostic DLL, SDK,
Node or Python was used at runtime.

The comprehensive native 36 MP ARW workflow is **not fully passing**:

- The initial 5-second post-export brightness deadline failed. A full-resolution
  export releases the preview session, requiring a full session rebuild. The
  self-test now uses its existing controller correctness deadline of 120 seconds
  for that case and records elapsed time. Observed rebuilds took 5346 and 8750 ms.
  This is a test deadline correction, not a rendering performance improvement.
- One subsequent run failed `slide_auto_scan_fujifilm_velvia_100`; a repeat
  passed the slide stages but exceeded the 360-second overall workflow deadline.
  Both reports are retained in `native-large-raw-*.json`. Their causal status is
  unresolved; do not describe these as a verified pre-existing defect or a
  confirmed material-model error.

Thus the requested synthetic candidate comparisons and targeted gates pass,
while this additional generic large-RAW native workflow has two failed attempts.
The earlier short-deadline failure is also retained. Smaller owned-fixture QML
workflow and both Phoenix generations' ARW preview/full-resolution export pass.
Private photographs and their scene captures are not committed or uploaded.

## Limits

No full macOS/Metal, Qt upstream, LibRaw upstream or Python-oracle suite was
rerun. Physical film fidelity, native file-dialog interaction and colour-managed
monitor accuracy are not asserted. Diagnostic spectra/XYZ/Lab remain in the
research tools; the preview application exposes the final photographic render.
The frozen gray calibration has no Round 2 entries and cannot silently apply
to them. All candidates lacking real measurements remain Inconclusive.

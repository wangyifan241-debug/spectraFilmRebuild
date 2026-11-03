# Preview integration test evidence

## Final source gates

Executed on Windows with NVIDIA GeForce RTX 3050 Laptop GPU, native Vulkan,
float32. Development tests run Qt offscreen where noted; they still invoke the
real Vulkan film renderer. This is not a monitor calibration measurement.

| Gate | Result | Evidence |
|---|---|---|
| 23 materials, Frozen 23 + frozen neutral calibration 14 | 1187 checks pass; 37/37 RGBA16 comparisons exact | normal-results.json; final-normal.log |
| Fixed gain, all 23, isolated process | 781 checks pass; 23/23 RGBA16 comparisons exact | fixed-results.json; final-fixed.log |
| Manifest/profile integrity, 23 UI identities, unsupported mode refusal, JPEG/TIFF import | 28 checks pass | integrity-results.json; final-integrity.log |
| Actual QML workflow and scene captures | pass; 235 report fields | final-qml.log |
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

## Limits

No full macOS/Metal, Qt upstream, LibRaw upstream or Python-oracle suite was
rerun. Physical film fidelity, native file-dialog interaction and colour-managed
monitor accuracy are not asserted. Diagnostic spectra/XYZ/Lab remain in the
research tools; the preview application exposes the final photographic render.
The frozen gray calibration has no Round 2 entries and cannot silently apply
to them. All candidates lacking real measurements remain Inconclusive.

# Windows Phoenix Experimental Preview — local delivery

## Delivered scope

An independent Windows Qt/Vulkan experimental build adds 23 preserved materials:
2 Phoenix Experimental v1, 9 Round 2 candidates and 12 Round 3 candidates.
The manifest retains original IDs, versions, SHA256 values and hypotheses.
The existing UI supplies comparison, zoom, previews, original-resolution
TIFF/PNG/JPEG export and recipe persistence. A separate Experimental panel
groups both generations by research round, displays identity/physical changes,
and resets to the same-generation v1.

No product material or research profile was changed. The research PRs are not
merged or edited. The source is frozen at accepted research commit
`73784949cda9a99e859d3b3a82ac27784a5e7b3b`; this PR contains only the preview
integration and its evidence relative to that frozen base.

Frozen Print is available for all 23 materials. The accepted neutral presets
apply to 2 v1 and 12 Round 3 materials; Round 2 explicitly refuses that mode.
The presets are research starting points, not optimal calibration for arbitrary
photos. Fixed Internal Gain uses an isolated experimental process/store with
the accepted same-generation parent gain, without loading the diagnostic DLL.
The ordinary build option is OFF and retains its automatic gain algorithm.
Research stage traces and spectrum/XYZ/Lab diagnostics remain separate tools.

## Local program

Complete directory: `output/PhoenixPreview-0.1-local/`.
Launch `Start-Phoenix.cmd`, or `Start-Fixed-Gain-Research.cmd` for the isolated
research viewer. Keep the complete directory together. It requires a Vulkan
GPU driver; Qt SDK, Node and Python are not runtime dependencies.

The executable/source package is bound to source commit
`03c214f41ebe7cea30e0340fd8843aae242b779d` and executable SHA256
`f6dc75ff50d0262ae8f82f5181bdaf6e8215392be1c726a0b89155329e60046d`.
Subsequent commits archive validation/report data and do not change that binary.
`package-integrity.json` records all 1713 verified files and the package-manifest
hash. The local app was actually launched and displayed a real Sony ARW using
Phoenix v1; it is not merely a source or screenshot delivery.

## Validation and remaining work

- Frozen/neutral: 1187 assertions; all 37 research-renderer comparisons exact.
- Fixed gain: 781 assertions; all 23 research-renderer comparisons exact.
- Independent resource/integrity controls: 149 assertions pass.
- QML workflow: pass, 237 report fields. Targeted frame/export/ICC and colour
  gates pass. Normal engine Portra and Gold outputs are byte-identical to the
  accepted normal reference on the fixed synthetic input.
- Two additional large-RAW native workflow attempts remain failed: one slide
  selection assertion and one 360-second overall deadline. The earlier
  5-second rebuild deadline was corrected only in the self-test; its original
  failure is retained. See TEST_EVIDENCE.md for exact scope and limitations.

Remaining P1: diagnose the generic large-RAW native workflow failure/race and
complete that long gate without timeout. These results are insufficient to
claim complete Windows parity. Remaining P2: no clean second-machine install
verification or code signing; no automated native file-dialog or calibrated
monitor validation. The portable directory is a local experimental build.

`photographic_fidelity_validated=false`; `photographic_gate=Not Verified`;
real-film accuracy metrics remain null. Unsupported hypotheses remain
Inconclusive. No fourth-round fitting was started.

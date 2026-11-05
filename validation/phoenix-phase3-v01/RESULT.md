# Phoenix Research v0.1: first candidate rejected by engineering gates

Executed on Windows / NVIDIA GeForce RTX 3050 Laptop GPU, driver 595.95,
2026-11-05. Real Vulkan film pipeline and spectral shaders; CPU device/tone
reconstruction, with all stage buffers retained as float32. Scan only, not paper.

## Actual result

`DecoupledDeviceToneV01` is implemented and rendered, but **not accepted**.
Phoenix I's green-biased ramp decreases from Y=0.51827359 at EV +3.5 to
Y=0.51756692 at +3.625. The reciprocal chromatic transmission reconstruction
does not guarantee monotonic luminance away from the neutral axis. This is a
real float-buffer failure, independent of PNG quantization or GUI behavior.

The synthetic skin-like display sequences also exceed the preregistered
15-degree adjacent-hue limit: I 23.61°, II 16.10°. This measurement uses RGB8
readback of the exported PNG and a chroma >2 mask; shadow quantization can
contribute, so it is a conservative display diagnostic, not proof of a
discontinuous underlying film response or accurate skin modelling.

The source code remains one candidate with no post-result coefficient tuning.
Per the handoff failure rule, no Research materials were registered in the GUI,
and no GUI executable is promoted as a verified improvement. The compiled EXE
below is the diagnostic render/comparison runner. No real Qt material-toggle
acceptance is claimed; that conditional step is intentionally not entered.

## Passed checks

- Real GPU shader domain probe: maximum absolute error 9.54e-8; non-symmetric
  row-major matrix multiplication, log transmission and curve endpoint tests.
- Device calibration: 400 known smooth spectral transmissions; 100 disjoint
  held-out spectra, XYZ MAE 0.003173, maximum error 0.028331 (white Y=1).
  Device matrix uses no film or source RGB targets. This limited spectral
  family does not establish accuracy for arbitrary narrowband transmissions.
- Both Round 5 RAW float buffers exactly match historical Phase 0 hashes.
  Same-run repeat control is exact too; no legacy source or profile changed.
- Neutral channel spread: I 0.1513%, II 0.0267%, below fixed 0.5% tolerance.
- All candidate RAW/ramp outputs finite, with no negative output channels.
  Values above one are retained in float files as linear headroom.
- No increase in final PNG endpoint clipping under the shared display path.
  This is an endpoint statistic, not a claim of an unclipped RAW decode: the
  inherited compatible16 RAW decoder clips before the shared input stage.

## Review artifacts

Open `delta_and_side_by_side.html` here. It contains actual digital, legacy,
research, 10x difference, skin ROI and exposure plots. `metrics_and_settings.json`
contains all 28 checks, stage statistics, recipes/settings and resource hashes.
The Sony ROI is a consistency-only comparison; no unpaired reference skin fit.
The immutable float/TIFF buffers remain in local `build/phoenix-phase3-v01-run1`.
No Phase 0–2 analyses were repeated; historical floats were used only as hashes.

Diagnostic EXE:
`D:\学习资料\SpektraLab\build\phoenix-research-v01-ascii\app\spk_phoenix_research_v01.exe`

SHA256: `36d3df73d4f0991f5cfea8f364d2091988fcf998b86dd085758bef94dc7aae1b`

Fixed RAW SHA256:
`2f0fbdac0097e7fb2a91c049fdb869ff45354208a9e60da5e0df471d94f91202`

`research-build.json` pins the executable and exact source inventory, including
untracked sources at build time. It records parent HEAD 67c2dcd0; the implementation
was uncommitted then and is identified by file hashes, not that parent commit alone.
`SHA256.json` inventories the review artifacts. `synthetic.log`, `raw.log` and
`analysis.log` retain actual execution results. Build configure/compiler/deploy log:
`build/phoenix-research-v01-ascii-build.log`.

## Reproduction

Use the existing ASCII junction `C:/Users/Strickland/SpektraLab-work`; Qt's import
scanner fails on the physical Chinese source path. Build via that path's
`windows_UI/phoenix_research_v01/build.ps1` with `-DependencyDirectory
C:/Users/Strickland/SpektraLab-deps-20261028`. Run `run.ps1` with the built EXE,
`C:/Users/Strickland/Downloads/DSC02443.ARW`, and a new output directory.
Analysis needs numpy, Pillow, scikit-image and matplotlib; Python is offline
analysis only, never a dependency of the EXE or engine at runtime.

Original-resolution export protection, formal profiles and Round 5 are intact.
Stop here for user visual review; do not automatically try another tone policy.

# Phoenix Research v0.2 — one controlled correction, still not promoted

Executed 2026-11-05 on Windows, NVIDIA RTX 3050 Laptop GPU / driver 595.95.
Existing Draft PR #18 only. No material registration, merge or export-guard change.

## What changed and why

v0.1's per-channel `P=e*N(e)/T` has an unconstrained weighted luminance.
Its log derivative includes `-d ln(T_c)/dEV`, allowing unequal channel
amplification and a non-neutral brightness reversal even when scalar e rises.
The existing device transform is unchanged. The sole v0.2 candidate uses a
bounded opponent log-transmission residual and explicitly normalizes output Y
to e. The formula, fixed bounds, signal domains and limitations were declared
in `POLICY_V02.md` before execution. No original-image mixing or global
saturation setting is used; no threshold was loosened after observing output.

The fixed radial bound is a research assumption, not a measured film value.
This construction enforces neutral preservation and bounded ratios, but does
not establish correct skin hue or identify a unique film reconstruction.

## Engineering result: 52 of 54 checks pass

| Check | Phoenix I | Phoenix II |
|---|---:|---:|
| Neutral relative channel spread | 0.1513% | 0.0267% |
| Minimum Y step across all 8 ramps | -1.42e-10 | -1.48e-10 |
| Skin maximum hue step, original float32 ProPhoto -> Lab D50 | 3.18° | 1.67° |
| Skin maximum hue step, sRGB16 display -> Lab D65 (diagnostic) | 3.52° | 5.94° |
| Skin maximum hue step, RGB8 sRGB -> Lab D65 | **23.62° FAIL** | **16.10° FAIL** |

All hue statistics use the unchanged 15-degree threshold where adjacent Lab
chroma exceeds 2. D50 linear and D65 display measurements are separate domains;
their numerical difference is not a film-accuracy measure. The same display
path at 16 bits has much smaller hue steps than the RGB8 readback. This is
evidence of a quantization contribution; the failed RGB8 gates remain failed.
v0.1's newly measured linear hue steps were already 3.18° / 1.67°: its old
RGB8 failures alone were not proof of an intrinsic linear hue discontinuity.

The true v0.1 green-ramp failure is corrected: Y now rises 0.28002633 ->
0.28381718 between +3.5 and +3.625 EV instead of falling 0.51827359 ->
0.51756692. Tiny negative steps elsewhere are float32 rounding on endpoint
plateaus and pass the original -1e-5 tolerance. This is nondecreasing within
that tolerance, not a claim of strictly increasing output at every exposure.

For RAW, relative Y normalization error is below 5.85e-8; maximum channel
ratios are 3.093 / 1.824 against the fixed bound 16. Outputs are finite with no
negative channels. Linear values >1 remain in float buffers. No display
endpoint clipping increase beyond the original tolerance was observed.
Shared device recipe and held-out results are exactly unchanged. Digital
input, Round 5 and recomputed v0.1 float **and PNG** controls match historical
outputs exactly for both RAW and synthetic inputs.

## Visual limitations remain

The four-way comparison includes face, arm, white uniform, dark clothing and
background crops, all at identical coordinates. These are consistency checks,
not calibrated or paired-film targets. The observed change is small:

- Phoenix I arm Lab a/b changes 17.37/28.51 -> 15.48/25.89; uniform b changes
  -30.93 -> -28.48. The warm skin and strong blue uniform remain visible.
- Phoenix II face chroma changes 7.04 -> 6.96. The gray/low-color appearance
  is not resolved by this structural correction. No blood-color offset was fitted.
- Inherited scalar tone endpoints remain: 79,876 / 80,584 of 749,568 RAW
  pixels reach the lower -6 EV reconstruction floor, with none reaching the
  upper endpoint. Those shadow plateaus are explicitly retained; extending
  tone range would be another candidate and is not attempted here.

Do not promote v0.2 to the material library or describe it as verified film
accuracy. The two RGB8 failures are retained, Windows presets are not added,
and subjective optimization stops here for user visual review.

## Deliverables and provenance

- `delta_and_side_by_side.html`: actual Digital/Round5/v0.1/v0.2, region crops,
  10x differences and exposure plots. `comparison.png`: four-way overview.
- `metrics_and_settings.json`: all 54 checks, separate hue domains, ROI
  statistics, device recipe, resource SHA256 and per-stage float statistics.
- `raw/` and `synthetic/`: PNGs, resolved settings, scanner recipes and reports.
- `compiled-source/`: exact research C++ bytes matching the build receipt.
- `build.log`, `initial-build.log`, `raw.log`, `synthetic.log`, `analysis.log`:
  actual logs. Initial build linked but declined its receipt because an offline
  analysis script was added during compilation; final stable build passed.
- `research-build.json`: exact EXE/source inventory at build time, parent HEAD
  dab243fc with then-uncommitted v0.2 sources. Offline ROI/report analysis was
  finalized afterward; it does not change the compiled C++ or candidate output.
- `local-buffer-SHA256.json`: all 48 float/TIFF output hashes. Full files
  remain under `build/phoenix-phase3-v02-run1`; v0.1 history remains untouched.
- `SHA256.json`: exact-byte review artifact hashes (newline conversion disabled).

Diagnostic EXE (not an accepted material-toggle GUI build):
`D:\学习资料\SpektraLab\build\phoenix-research-v02-ascii\app\spk_phoenix_research_v02.exe`

SHA256: `c6fe81916a9b57f7d0fea6a46e914ea9a74518ade5b6db803d7671b2ac4a83fd`

RAW SHA256: `2f0fbdac0097e7fb2a91c049fdb869ff45354208a9e60da5e0df471d94f91202`.
Same compatible16/as-shot WB/linear ProPhoto decode, preview stride 7,
732x1024 output, EV 0, grain/halation/glare off, shared shoulder and sRGB
display conversion. Scan only; no optical-paper pipeline. The inherited RAW
decode and display conversion have their existing clipping behavior, unchanged.

## Reproduce without changing history

Use the ASCII source junction `C:/Users/Strickland/SpektraLab-work` and
`windows_UI/phoenix_research_v02/build.ps1` with the existing dependency root
`C:/Users/Strickland/SpektraLab-deps-20261028`, targeting a separate build folder.
Then use `run.ps1 -Executable <new EXE> -Raw
C:/Users/Strickland/Downloads/DSC02443.ARW -NewOutputDirectory <new folder>`.
The analyzer requires historical `build/phoenix-phase3-v01-run1` for immutable
control comparisons. Python analysis uses numpy/Pillow/scikit-image/matplotlib/
tifffile; none is needed by the EXE. Render exits 0; completed but rejected
engineering analysis exits 2. This is an intentional gate failure, not a crash.

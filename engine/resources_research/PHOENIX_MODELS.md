# Phoenix I / II experimental spectral models

Both are independent v0.1.0 profiles, `harman_phoenix_200_exp_v1` and
`harman_phoenix_ii_200_exp_v1`. User authorization on 2026-11-02 explicitly
permits source-labelled proxies to obtain usable initial versions. Earlier
research conclusions about eligibility as fully calibrated materials remain
historical; these models do not claim that calibration.

## Architecture audit

- Engine `Profile` owns spectral sensitivity, base and dye spectra, sampled
  exposure curves and emulsion sublayers. Keep these in the normal material
  loader; no separate Phoenix shader, image filter or LUT-only rendering.
- `digest` selects material effect baselines, then honors existing preview/debug
  bypasses. User exposure, grain and halation amounts retain existing SHOOT
  invalidation, negative cache and spatial scaling semantics.
- Windows `MaterialLibrary` snapshots bundled resources, exposes source/quality
  metadata, and persists SHA256 material identities. Existing catalogue discovery
  makes the two bundled profiles selectable without changing QML controls.
- `PreviewController` owns selection and debounce/worker scheduling; existing
  exposure/grain/halation controls drive the unchanged C ABI.
- Inputs decode to linear ProPhoto; spectral film/development/print/scan occurs
  in the engine. Frames own their output transform. `FrameOutput` grades and
  converts both display and full-resolution export through the same path.
  No extra transfer function, display LUT or post-render Phoenix color shift.
- Export captures the displayed material/settings and immutable source. Tests
  compare equal-resolution preview and native PNG, exercise TIFF/PNG16/JPEG,
  and save/reopen per-photo material versions.

## Evidence and mathematical choices

The raw charts, extracted CSV, per-channel missing values, fixed source hashes
and scripts are in `phoenix200/` and `phoenix2/`. Original PDF/source screenshots
are kept locally with their own copyright and are excluded from app packaging.
The II characteristic candidate has an unresolved Phoenix I caption conflict;
its use for II is a B/C-level provisional hypothesis, never an official II fact.
Both profiles explicitly retain `photographic_fidelity_validated:false`.

`phoenix-model-assumptions.json` is the adjustable model specification. A/B/C
evidence grades distinguish manufacturer charts, provisional secondary sources,
and explicit estimates. Values are not inferred from arbitrary scanner JPEGs.

1. Assume printed spectral ordinate is logarithmic; correct the I tungsten
   spectrogram with a 2856 K Planck spectrum. II uses its own D55-labelled chart.
   This interpretation is unconfirmed and deliberately replaceable. Normalize
   channels to the donor's weighted 5500 K blackbody response as a white-balance
   approximation, not an absolute ISO calibration. Missing tails use a small
   sensitivity floor; gaps between traced points are model interpolation.
2. Use Gold 200 dye shapes as a named proxy. Replace the orange mask with an
   explicitly modeled purple base spectrum. Do not claim these are HARMAN dyes
   or measured base density; preserve CC BY-SA attribution on the derivative.
3. Use relative chart density increments, with a declared low-exposure R/B toe
   completion for I. A declared Gaussian synthetic densitometer forwards the
   proxy dye/base spectra by Beer–Lambert integration; a bounded 0–8 nonlinear
   inversion obtains CMY layer amounts. The meter is **not Status M** and does
   not establish real-film density agreement.
4. Fit a nonnegative three-Gaussian-CDF mixture per channel, sample 256 exposure
   values and three coherent sublayers. Exposure axis zero is an explicit
   relative-chart anchor assumption. Toe, straight-line and shoulder responses
   occur in the film stage, not as a post-print style LUT.
5. Grain particle areas and Gaussian halation strengths/sizes are estimates on
   the existing engine models. II has smaller grain and weaker halo as a
   qualitative model choice. Numeric values have no measured HARMAN status.
6. Generic negative-film DIR coupling at full strength folds the transformed
   exposure axis for the high-contrast I reconstruction (a failing rendered
   gray-ramp regression exposed abrupt color reversals). The estimated material
   coupling baseline is 0.15, independently multiplying the user amount at the
   coupling stage. Original chart response arrays stay unchanged. Native and
   offline gates require strictly increasing transformed exposure axes.

## Reproduction and upgrade

Run each stock's digitizer after downloading its fixed source files (see its
README); then run from repository root:

```powershell
node engine/resources_research/build-phoenix.cjs
node engine/resources_research/verify-phoenix.cjs
```

Outputs go to `engine/resources_experimental/profiles/`, with a companion
neutral-filter database that preserves existing entries and adds explicit
generic [0,65,55] print packs for the two stocks. This C-level output-stage
starting estimate prevents previous-stock state leaking into a fresh open.
CMake overlays these profiles, the filter database and notice onto resources. No generator,
Node, Python or network is required to build/run using the tracked profiles.
`phoenix-model-report.json` records inversion and fitted-model residuals to the
assumed instrument; they are not errors against physical film.

For Portra Endura / TH-KG3, `phoenix-print-calibration.json` replaces the generic
packs with a model-grey balance solved through the actual Vulkan chain:
I [0,57.09484,79.04972], II [0,85.57272,96.63273]. This is output-stage calibration
to the implemented model, not physical film characterization.
`spk_phoenix_calibrate <bundled-resources> <report.json>` reproduces the solve;
the standalone target is offline and not linked into the editor.
The generator rejects calibration whose source profile hash has changed.
For revised data, generate with `--uncalibrated`, build/sync resources, run the
calibration tool, then regenerate normally. Never reuse old packs silently.

For new measurements, replace the relevant data/interpretation while retaining
old sources; change material version and stock ID to v2, regenerate and test.
Keep v1 files available for old recipes. A custom-material import can deliver
an additional version without overwriting the bundled stock, but does not
automatically import global print-filter calibration tables.

## Verification boundary

The offline gate checks source hashes, finite arrays, monotone response and
surrogate closure. `--counterfactual` intentionally injects NaN and must fail.
`spk_qt_phoenix` exercises native import validation, optional effect parameter
rejection, grey ramp, synthetic 24 color patches, ±2/±6 EV, effects and bypass,
distinct generations, Portra restoration, recipe reopen and actual encoders.
These are software/model consistency checks. No matched physical HARMAN
exposures were measured; photographic fidelity remains **unvalidated**.

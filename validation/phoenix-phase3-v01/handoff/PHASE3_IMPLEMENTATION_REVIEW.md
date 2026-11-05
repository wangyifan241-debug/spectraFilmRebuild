# SpektraLab Phoenix Research v0.1 — independent code review and execution handoff

Date: 2026-11-05. Review scope: GitHub `wangyifan241-debug/spectraFilmRebuild`, Phase 2 Draft PR #16, Phase 3 protocol Draft PR #17, and code on `codex/phoenix-phase2-model-comparison`. This report is a source review and implementation specification. **No Windows executable, fresh Vulkan render, new Research material or fitted film coefficients were produced here.**

## 1. Verified code facts

### Scanner construction (`engine/src/desktop/phoenix_round5_scan.hpp`)

- `spk::phoenix5::Scanner<Context>` builds three Gaussian spectral detectors (centres 610, 540, 450 nm; sigma 30, 30, 25 nm) under D50. Detector weights are stock-validity-masked via finite base/dye values. The scanner uses zero base-density input for this integration (`base` is a zero-filled 81-value vector); interpret actual baseline handling alongside the upstream negative density semantics; do **not** infer a new physical correction from this snippet alone.
- Creates **129 neutral probes**: `0.184 * 2^EV`, EV = −6 ... +6, measured by the same film pipeline then spectral scan. Inverts per-channel response using the monotonic `axis` and `target=log10(original neutral input)`. The `spk_curves` GPU stage applies this inverse. This deliberately maps neutral film response toward input exposure across the sampled range.
- Runs **320 synthetic chromatic probes** (five integer EVs −2...+2, four levels per RGB component). Uses inverted detector signals, and fits a 3×3 matrix whose dependent variable is original input RGB. This can reconstruct the digital source and suppress film-related chromatic differences; the Phase 0 ablation measurements independently support suppression. This **does not** demonstrate that simply deleting the inversion or matrix yields an accurate film model.
- The final functions are `spectral(negative)`, `invert(logT)` and `positive(logs)` using GPU stages `spk_spectral_epilogue`, `spk_curves` and `spk_round4_scan` respectively. The calibration has stock-dependent parameters, and `Scanner` is instantiated separately for each film.
- The saved scanner `recipe` contains wavelength, detector weights, neutral axis, matrix, source provenance, and validity exclusions; use these existing fields as comparison evidence.

### Windows preview (`engine/src/desktop/phoenix_round5_preview.hpp`)

- `render_phoenix_preview` chooses one of two Round 5 stock IDs, decodes linear ProPhoto RAW, down-samples to an approx. **1024-pixel long-edge preview tier**, forces auto exposure off, builds the film pipeline and a separate calibration film pipeline, then calls `scanner.positive(scanner.invert(scanner.spectral(negative)))`.
- It multiplies linear output by `settings.print_exposure`, then applies a shared luminance shoulder and ProPhoto encoding. **It does not execute an optical paper-print pipeline.** The `kodak_portra_endura` argument supplied to `init_params` is not proof that an optical print stage ran. Avoid labelling such outputs as film+paper without an explicit stage trace.
- The current method deliberately supports preview only. Original-size Round 5 export remains guarded elsewhere and must stay protected.

### Evidence from the completed Phase 0/2 research

- A/B/C/D scan ablation on the fixed RAW: mean inter-film CIEDE2000 approximately 1.684 / 4.641 / 2.531 / 5.686 in the earlier D50 comparison. These measurements only show differences between software-generated output, not film accuracy.
- Phase 2 Lab D65 on the same Sony RAW using a common display shoulder: Round 5 I vs II mean ΔE00 **1.6736**. Do **not** numerically combine with the D50 series.
- Reference sample: Phoenix I 16 images from 5 authors, Phoenix II 16 from 9 authors. Leave-one-author-out changes several effect signs and author bootstrap intervals include zero. Phoenix I human-portrait references = 0; no defensible red-skin colour fit.
- Device candidate E failed neutral preservation on synthetic extremes, despite clean RAW range checks. Do **not** promote E, A/B/C/D as final scanners.

## 2. Priority implementation decision

**Implement the first candidate as a stock-independent scanner-device calibration experiment on the physically appropriate signal representation, with a clearly separated per-stock tone reconstruction policy.** Do not change film measurement profiles or derive stock-specific colour coefficients from the 32 unpaired JPEGs.

This is a *hypothesis requiring measured validation*, not a claim that a complete device transform is already uniquely identifiable from the current data. Device calibration from known synthetic spectral transmissions can constrain detector->XYZ behaviour; it cannot by itself identify a unique photographic-film rendering look. Keep the optical-transmission transform and inverse-positive reconstruction in separate named functions / layers and document where each acts.

**Design constraint:** the current `spk_round4_scan` expects the numerical representation produced by `invert()`; do not feed a transmission-calibrated matrix directly to it unless the stage input domain and matrix orientation have been derived and unit-tested. This is the failure mode identified in experimental candidate E.

Recommended minimal organization:

```
ResearchScanPolicy {
  LegacyRound5,                 // bit-identical control
  DecoupledDeviceToneV01       // experimental, never production default
}

Film negative -> spectral transmission/measurement -> [device-calibration stage]
              -> [explicit positive/tone reconstruction] -> display colour transform
```

The diagram is conceptual; the physical domain for `spectral()` must be confirmed against `spk_spectral_epilogue` and `spk_curves` shader implementations before changing call order. **No arbitrary matrix coefficients are prescribed.** Before writing production integration, the Codex run should inspect GPU shader implementations, units and matrix direction; establish device calibration using independent targets, then run synthetic probes. Only proceed to GUI when gating diagnostics pass.

## 3. Proposed first experimental sequence (short, visible, reversible)

1. Capture an exact legacy Round 5 control with pinned input hash, executable SHA256, resource hashes, film IDs, scanner recipe, exposure and colour-management settings. Do not rerun historical analyses without need.
2. Write a small research-only stage-domain unit / integration probe and verify: measured spectral input units, output of `spectral`, `invert`, `positive`, 3×3 matrix multiplication convention and zero/grey baseline. Check that any proposed device calibration is defined on the correct linear physical signal (not an arbitrary post-log or inverted value).
3. Implement **one** `DecoupledDeviceToneV01` policy in the independent research harness, behind an explicit opt-in switch. Leave `LegacyRound5` byte-identical and all existing stock JSON unmodified. If the stage-domain probe invalidates the hypothesis, stop this candidate and report its rejection with evidence.
4. Run on synthetic neutral EV −6...+6 and R/G/B-biased exposure ramps, record float32 per-stage outputs, continuity, monotonicity, neutral drift, negative/nonfinite values, high-light clipping and display gamut. Do not classify valid intermediate values >1 as output clipping.
5. Run `DSC02443.ARW` with same decode, WB, exposure EV=0, long-edge preview tier, shoulder and display conversion as control. Grain/halation/glare disabled for deterministic comparisons. Report both stocks, baseline, experimental, difference maps and all resulting hashes.
6. Do not infer true skin hue from an unpaired blue-stock orange-parasol portrait. Run existing synthetic skin sweeps and Sony skin ROI for consistency-only checks; investigate unexpected shifts instead of fitting unsupported targets.
7. **Visual gate**: if engineering tests pass, optionally package two `Research v0.1 — Experimental` materials in *test-only overlay* with distinct IDs, retain legacy Round 5 in menu, build an identifiable Windows EXE and capture real GUI material toggles. Stop after one candidate, show actual images to user before any further colour look tuning.
8. If tests fail, generate the actual failure images and a 1-page diagnosis. Do not build or advertise a research EXE as a verified improvement; do not start another candidate by default.

## 4. Required acceptance checks

| Check | Minimum evidence | Rejection rule |
|---|---|---|
| Legacy preservation | Exact baseline PNG/float hashes, unchanged stock profiles | Any unexplained change |
| Correct stage domain | Explicit shader maths and unit/representation trace | Matrix calibrated on incompatible signal domain |
| Independent calibration | Known synthetic spectral targets and held-out spectral target tests | Original digital RGB/film output used as device calibration target without disclosure |
| Neutral response | EV sweep and neutral-channel residual plots | Unexplained large drift or changes of grey axis |
| Exposure | Monotonic luminance, finite intermediate values, smooth derivative | Discontinuities, reversals, nonfinite values |
| RGB gamut / highlights | Raw linear stats + display clipping, same shoulder/profile | Hidden clamping or avoidable severe clipping |
| Skin consistency | Sony ROI + synthetic sweeps, no fake reference pairing | Abrupt hue switching, unsupported skin target |
| Paper/scan attribution | Explicit scan-only and optional labelled paper stage | Calling a scan-only preview optical-paper simulation |
| GUI execution | Actual Qt window output, exact preset IDs, 2-way toggle, hashes | Research profile listed but not rendered |
| Provenance | Original file SHAs, output settings, commit and executable SHA | Unrepeatable result |

**Important:** research v0.1 is an experimental comparison candidate, not a claim of validated Phoenix chemistry. A visual difference must be described as a hypothesis. Prefer a weak but traceable candidate to an artificially exaggerated look.

## 5. Explicit handoff boundary

### Completed independently in this review

- Inspected actual GitHub code for scanner calibration, Windows preview and engine host plus PR #16 report and PR #17 protocol.
- Identified precise scanner suppression mechanisms, execution stages, paper-stage ambiguity and incompatible-domain risk for candidate E.
- Proposed one bounded test-policy implementation, evidence gates and visually inspectable deliverables.

### Must be executed in the user's Windows / NVIDIA / Codex environment

- Inspect actual `spk_spectral_epilogue`, `spk_curves`, `spk_round4_scan` shader source and confirm the calibration domain numerically.
- Write, compile and run the research-only C++/Vulkan implementation, produce real float32 and PNG outputs on supplied Sony RAW.
- Run Qt QQuickWindow GUI validation; package and hash the Windows EXE only if candidate passes engineering checks.
- Confirm deployment paths, GPU failures, latest local unpushed state and source asset hashes.

## 6. PR hygiene

PR #16 contains executed Phase 2 analysis; PR #17 is DESIGN ONLY. A future implementation belongs in a separate, plainly named Draft PR; leave both existing PRs unmerged. Keep original resolution export guard. Do not overwrite Round 5. No automatic claims of real-film accuracy, statistical significance, or successful Windows builds.

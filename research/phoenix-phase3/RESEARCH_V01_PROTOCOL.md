# Phoenix Research v0.1 — controlled experiment protocol

2026-11-05. Status: **design only, not an implemented or fitted model**. The Phase 2 comparison did not identify defensible numeric stock parameters. No Research PNG, test preset or Windows EXE exists for this design. Do not load this document as a material profile.

## Products and ownership
Reserve separate future IDs `harman_phoenix_200_research_v01` and `harman_phoenix_ii_200_research_v01`, displayed as Phoenix 200 Research v0.1 and Phoenix II 200 Research v0.1. Keep the Round 5 IDs, files and selectable behavior unchanged. Preserve the existing linear ProPhoto decode, spectral film pipeline, profile format, LUT interpolation and grain/halation implementation. Do not change a measured film profile to hide a scan/display problem.

First experiments belong in the independent research harness, not production defaults. Any runtime candidate must pin source commit, base profile SHA256, scanner policy, full resolved settings, dataset/manifest hashes, output transform and stochastic seed. Research presets are registered in a test-only resource overlay only after an actual candidate passes engineering gates. No schema or wire change is authorized by this protocol.

## Questions, controls and hypotheses
1. **Control R0:** exact Round 5 response and scan; retained PNG/float hashes are regression witnesses.
2. **Device calibration question:** can a shared, stock-independent instrument calibration operate on physical scanner/transmission signals, without fitting the original image's ProPhoto RGB? Calibrate known spectral targets at the correct signal stage; test on separate spectral targets. Do not apply candidate E's transmission matrix after an incompatible positive inversion. Reuse the spectral detector implementation, but keep device calibration separate from neutral/film reconstruction.
3. **Tone question:** can a common, interpretable print/scan tone policy preserve stock differences while giving a stable neutral anchor? Compare complete optical-print and scan-only pathways with explicit paper state. A single neutral exposure/white-balance anchor is distinct from independently inverting each stock's full characteristic curve. Do not promote affine tangent B or identity C/D directly. If a low-dimensional tone family is proposed, its coefficients remain unfitted until supported by controlled exposure sequences and scene-stratified reference evidence.
4. **Color question:** test a bounded residual chroma mapping only if hue/semantic-region evidence is consistent across more than one independent author/source group. Require the neutral axis to remain fixed and chroma changes to approach zero near neutral. Do not infer a saturation boost from current pooled histograms or force Sony skin to match the orange-parasol reference. No such numeric mapping is supported by the present batch.

No experiment treats a larger inter-stock DeltaE or a smaller unpaired JPEG histogram distance as accuracy. A/B/C/D/E remain diagnostic controls and are never automatic deployment candidates.

## Preregistered constraints and checks
These are proposed engineering gates, not already passed tests or measured material truths.
- Scene content and geometry remain unchanged; no per-image semantic repainting or generative model.
- Neutral ramp at linear RGB 0.184 × 2^EV, EV -6..+6 in 0.125 increments. Inspect all channels and luminance; retain float data. Anchor gains must not encode a fitted colored look. Any allowed neutral tolerance must be declared before a candidate run and checked against quantization/control precision; do not loosen it after viewing failure.
- Skin: preserve local hue ordering and avoid abrupt hue/chroma changes across exposure. Use synthetic skin-like sweeps plus held-out real portraits; source scans are qualitative bounds with lighting/scanner covariates, not paired targets. Current red-reference portrait coverage is zero, so no data-driven red skin offset is estimable.
- Exposure: continuous output and monotonic luminance; test intermediate exposure values rather than only integer EV. Check finite values, local derivative discontinuities, smooth highlight shoulder and no new banding relative to the same-precision control.
- Headroom: retain linear values above one where physically meaningful. Report negative/out-of-gamut channels separately from final display clipping. No unexplained clamping to make a test green. Compare clipping against R0 under the same gamut/display transform; investigate any increase rather than accepting an arbitrary absolute percentage.
- Grain/glare off for pointwise comparisons; separately seeded stochastic stability later. Do not fit grain/halation from resized JPEGs. No material-grain change in v0.1 without separate evidence.
- Paper: run scan-only and explicit paper conditions as separate labeled experiments. Do not hide stock differences with an unreported paper or printer-light change.

## Evidence required before choosing numeric coefficients
Use the current author/scene hierarchy as nuisance structure, not one pooled target. Hold out whole photographers, duplicate/source clusters and rolls. Quantify effect stability under scene and source exclusions. Author bootstrap on the current small collection is descriptive only; all observed intervals include zero. Independent scanner/whitepoint targets and exposure-response data can constrain instrument behavior without requiring paired digital RAW/film photographs. Paired capture is not a prerequisite for future progress.

Phase 0's five-second broad-test deadline is a known responsiveness/capability issue; it does not invalidate immutable saved float buffers. Bound queue/generation/state tests independently. Original-size Round 5 export remains protected: do not bypass it for a larger research sample. Use the audited preview-size input for first evidence and clearly label its resolution.

## First implementation and visual stop gate
Implement at most one predeclared instrument/tone hypothesis after its stage and parameter constraints are concrete. Produce one fixed Sony RAW digital/Round5/Research comparison plus a small preregistered set of additional conditions, all with exact settings, difference images, neutral/skin/exposure checks and HTML. Keep original Round 5 selectable. If engineering checks pass, add two distinct test-overlay Research presets and build an identifiable Windows EXE with SHA256. Then **stop all subjective look optimization and ask the user to inspect the images/program**. Do not iterate unseen looks.

This protocol has no selected strength, LUT, curve coefficients, sensor matrix or fitted material. Therefore it does not satisfy implementation/visual acceptance by itself. Current deliverable is Phase 2's actual Round 5 HTML and this controlled v0.1 design; model implementation remains pending.

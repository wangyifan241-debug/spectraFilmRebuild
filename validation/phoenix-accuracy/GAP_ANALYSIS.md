# Phoenix accuracy infrastructure gap analysis — 2026-11-02

Scope: infrastructure only. Frozen parent is Draft PR #4 at
`ba076b2befe6586ad3095f897ebf40ea7e9ffd6c`. Do not change/merge that PR, replace
Experimental v1, fit new parameters, or import another Phoenix CSV package.

## Reuse rather than duplicate

| Existing capability | Evidence | Reuse / missing capability |
|---|---|---|
| Model/source hashes, finite data, monotonic response and DIR axes | `engine/resources_research/verify-phoenix.cjs` | Keep as engineering gate. Its surrogate closure is not photographic accuracy. Add a frozen PR snapshot inventory. |
| Gray/exposure/synthetic patches | `windows_UI/tests/phoenix_material.cpp` | Reuse. Do not create another synthetic film simulator or call synthetic patches real-film measurements. |
| Preview/export and persistence | Same native test, `spk_qt_frame_output`, `spk_qt_material_workflow` | Reuse exact existing tests and published logs. Add evidence import and versioned reporting, not another output path. |
| Existing fixed rendered PNGs | `validation/phoenix-review-20261102/images/` | Pin input/output hashes as engineering-only fixtures. Never use them as photographic references. |
| Manufacturer curve provenance | `phoenix200/`, `phoenix2/`, assumptions and calibration JSON | Preserve bytes; include recorded external hashes even when original PDFs are unavailable. |
| Independent real-film reference manifest | Absent | Add explicit stock/generation, negative/roll/scene identity, source/license, RAW/scan linkage, exposure/process/scanner/ICC/processing conditions and pairing evidence. Unknown fields stay unknown. |
| Leakage-free train/validation/test | Absent | Group transitively by physical negative/scene/RAW/scan identity; deterministic split and reject conflicting declarations. Different scans of one negative cannot cross splits. |
| Paired photographic metrics | Absent | Add independently sourced D50 Lab/relative-Y observation interface, ΔE00, gray L* response and Y error. Require actual files, hashes, conditions and pairing evidence before releasing metrics. |
| High-resolution scan statistics | Grain/halation functional toggles only | Add spatially calibrated linear scan ROI interfaces for detrended grain statistics and radial halation profiles. No claim of real grain/PSF matching without files/conditions. |
| Version-to-version reports | Narrative reports only | Add immutable, versioned JSON/HTML reports with separate engineering/photographic gates and frozen-input render hashes/comparisons. Missing references => Not Verified, metrics null. |
| Controlled candidate comparisons | Absent | Validate new material IDs/versions/provenance, detect changed physical leaves and permit exactly one hypothesis category per candidate. Compare supplied renderer artifacts; optional configured existing renderer execution only in private staging. No automatic fitting or baseline replacement. |

## Implementation boundary

- A standalone offline Node toolchain is sufficient for manifest/observation validation,
  metrics, candidate staging/comparison and HTML/JSON. No application/runtime dependency.
- Observation files are an explicit interchange format: independently extracted
  D50 Lab/Y patches and linear, spatially calibrated scan ROIs. Extraction method,
  ICC transform and source hashes must be recorded. The infrastructure does not
  silently decode arbitrary RAWs, invert negatives, register unrelated images,
  assume sRGB, or normalize two images until they agree.
- Reuse a caller-configured existing renderer via executable/argv templates in
  private resource staging. Never write a candidate over the baseline materials.
- No real photographic references are present. Ship an empty manifest and an
  inactive template; production photographic gate must remain Not Verified.
- Unit tests may use explicitly synthetic observations solely to test arithmetic
  and rejection behavior; such test inputs must never satisfy the photographic gate.
- Thresholds are versioned provisional project acceptance policy, not a claim
  that the present materials meet perceptual/physical accuracy standards.

## Required failure cases

Baseline mutation; missing/hash-mismatched files; incomplete pairing/ICC/process
conditions; synthetic samples mislabeled for photography; cross-split duplicate
negatives/scans; mismatched patch IDs/white points/scales; insufficient spatial
resolution; changed renderer/recipe; multiple hypothesis categories; reused v1
ID/version; stale candidate artifact hash; no paired validation/test samples.

## Review and iteration sequence

1. Commit this gap analysis before implementation.
2. Pin baseline objects/data and published synthetic renders without changing them.
3. Implement independent manifests/splits, observation metrics/statistics and reports.
4. Implement one-hypothesis candidate comparison and separate acceptance gates.
5. Run new infrastructure regression tests plus the existing research gate; retain
   the native engineering test commands/results instead of duplicating those tests.
6. Open a separate Draft PR based on #4; no merge or model tuning in this round.

Future work: obtain licensed, paired physical references; independently review
extraction/calibration; acquire adequate groups in all splits; only then propose
a new version changing one hypothesis category and evaluate on validation data.
Held-out test results are for final evaluation, never candidate selection.

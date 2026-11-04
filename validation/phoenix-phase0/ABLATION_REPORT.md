# Phase 0 scan ablation — executed findings

Status: In Development, not accepted. No real-film accuracy established.

Actual engine runs: synthetic and DSC02443.ARW, PHOENIX_PHASE0_ABLATION=1, both exit 0. Production Scanner unchanged. Research main.cpp uses actual film/spectral/inverse/GPU-positive stages. B replaces full neutral inverse with its local affine tangent at 18.4% gray; C uses full inverse and identity matrix; D combines affine mapping and identity. A is unchanged Round 5. B uses the original fitted matrix as a controlled intervention, not a newly calibrated device. C/D can have severe colour errors and are diagnostic controls, NOT improvements. An independent physically interpretable device calibration candidate remains unimplemented.

## RAW inter-film differences (sRGB PNG -> inverse TRC -> XYZ D65 -> Bradford D50 -> Lab; CIEDE2000)
| Cell | RGB MAE /255 | mean DE00 | P95 DE00 |
|---|---:|---:|---:|
| A full inverse + regression | 2.1702 | 1.6837 | 3.2591 |
| B affine neutral + regression | 10.5467 | 4.6412 | 7.5457 |
| C full inverse + identity | 3.4377 | 2.5313 | 4.7702 |
| D affine neutral + identity | 11.9375 | 5.6862 | 8.8575 |

Within these fixed conditions, replacing the full neutral inverse while holding the matrix fixed increases inter-film mean DE00 by 2.9576; replacing the matrix while holding full inverse increases it by .8476. Combined increase is 4.0025, not the sum: interventions interact. This supports suppression of inter-film differences by the scanner, particularly full tone inversion. It does not prove every part of that suppression is undesirable, or establish how real film should look. Larger differences can be artifacts. The affine tangent preserves the mid-gray anchor but extrapolates toward extremes and must be judged alongside clipping and neutral response.

Analysis exports per-stage nonfinite/range statistics and neutral RGB response CSVs for the synthetic ramps. Each stock/mode has digital-reference RGB MAE, mean/P95 DE00, endpoint fractions and absolute RGB x10 difference PNGs. Actual linear ProPhoto .f32 outputs and scanner recipes are preserved. Analysis uses an 8-bit reading of PNGs; floating stage hashes independently retain full precision. Generic 0..1 range counts on log/density stages are not output clipping measurements.

## Evidence and commands
- build/phoenix-phase0-ablation-synth and build/phoenix-phase0-ablation-raw: actual outputs, report.json with resource/executable/input hashes, recipe and affine anchor/slope JSON.
- python validation/phoenix-phase0/analyze_ablation.py build/phoenix-phase0-ablation-raw (and synthetic): analysis/index.html, metrics.json, differences and CSVs.
- python validation/phoenix-phase0/check_ablation.py: PASS 4 exact baseline preservation cases, 12 branch sensitivity checks. No original baseline rerenders required.
- node --test tools/phoenix-validation/tests.cjs tools/phoenix-validation/review-tests.cjs: 51 passed, 0 failed/skipped. These validate existing research tooling, not UI behavior.
- Launch environment and previous baseline commands: preliminary-audit.md; add PHOENIX_PHASE0_ABLATION=1 to use new research branch. Use unique output directory. Build receipt was refreshed with current source inventory and executable SHA.

## Remaining Phase 0 requirements
Independent device calibration / decoupled candidate; actual window preset/original toggles, asynchronous/cache ordering and parameter regressions; channel response plots and visual clipping assessment; full application/resource delta between old and integrated EXEs. Earlier startup failure exact cause remains unresolved, but no longer blocks the controlled experiments. Export protection stays active. Current narrow tests do not cover all Phase 0 acceptance requirements. Do not advance to formal fitting.

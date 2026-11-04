# Phoenix Film Reconstruction — project status

Updated: 2026-11-04. Governing roadmap: original user Phase 0–6 plan. Open-source, non-commercial research. Scanner diagnosis is a Phase 0 subtask, not a replacement project. Software execution is not film accuracy evidence.

| Phase | Status | Remaining gate |
|---|---|---|
| 0 Runtime audit / repair | In Development | Current RAW and synthetic baseline renders completed; neutral-curve/matrix ablations, new difference metrics, UI/state regressions, final cause report and independent PR remain |
| 1 Reference dataset | Not started as a formal phase | Source investigation / manifest schema / screening tools may be prepared before Phase 0 acceptance; actual usable counts and licenses required |
| 2 Film signature extraction | Not started | Screened real references, controlled distribution analysis and scan/post-processing uncertainty |
| 3 Unpaired model | Not started | Phase 0 acceptance and dataset/signature evidence; no formal refit before Phase 0 acceptance |
| 4 Material integration | Not started | Separate Research materials, provenance, paper-off condition, preserved Round 5 |
| 5 Visual validation | Not started | At least 10 held-out scenes, exposure/seed stability, reference distributions and human evaluation |
| 6 Windows delivery | Not started | Actual app preset/before-after/preview-export/full-resolution/batch evidence |

Each phase retains separate artifacts, executed tests and an independent PR. No automatic merge. Do not overwrite Round 5, modify official film parameters during Phase 0, or bypass export guards. Public-reference preparation is not permission to fit parameters early. No commercial prerequisite is introduced.

## Current verified execution
- Synthetic baseline: build/phoenix-phase0-synth-02; exit 0.
- DSC02443.ARW baseline: build/phoenix-phase0-raw-02; exit 0; stdout reports Experiment complete; report.json exists (308296 bytes).
- Both use current independently built research executable. These are baseline scans and print-stage experiments already present in the tool, not the requested A/B/C/D neutral/matrix ablations.
- Historical measurement reproduction is recorded separately in preliminary-audit.md and history-recomputed.txt; do not repeat it or substitute it for new ablations.
- Successful launch conditions are documented; the exact earlier fail-fast cause is unresolved and no longer blocks baseline execution.

Closeout update: four response figures generated from existing buffers. Research-only independent spectral device candidate implemented and executed on synthetic input (exit 0), with accuracy explicitly unverified. Output regression executed and passed. Controller workflow run pending; RAW candidate E, actual GUI toggles and final acceptance remain outstanding. Phase 0 stays In Development. Phase 1 source/manifest preparation may proceed, but formal fitting remains gated; Phase 2 signatures -> Phase 3 unpaired model -> Phase 4 distinct Research materials -> Phase 5 held-out visual validation -> Phase 6 Windows delivery remain the governing sequence.

Latest acceptance: controller workflow PASS 114 checks, candidate E RAW exit 0 with measured neutral-stability limitations. Real-window self-test FAILED automatic brightness preview did not run; screenshots/report preserved. Phase 0 In Development, GUI acceptance Blocked; Phase 1 preparation only, Phase 2–6 pending per original roadmap. Real-film accuracy Not Verified; protected original-size export still blocked. No production substitution authorized by these experiments.

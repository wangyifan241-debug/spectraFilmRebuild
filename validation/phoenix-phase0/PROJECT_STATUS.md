# Phoenix Film Reconstruction — current project status
Updated 2026-11-04. This file supersedes earlier pending-state notes.

| Phase | Status | Evidence / next gate |
|---|---|---|
| 0 Runtime audit | In Development | Independent Phoenix GUI PASS (exit 0); controller workflow PASS 114; A–E actual renders and curves complete. Original automatic-brightness self-test diagnostic rerun pending; final acceptance not asserted |
| 1 Real reference dataset | Preparation permitted | Source/manifest/screening preparation only; no formal fitting before Phase 0 acceptance |
| 2 Film signatures | Not started | Analyze screened real references and scanner/post-processing confounds |
| 3 Unpaired model | Not started | Use real data; do not promote current ablations by visual difference alone |
| 4 Materials | Not started | Separate Research materials; retain Round 5 and provenance |
| 5 Visual validation | Not started | At least 10 held-out scenes and human assessment |
| 6 Windows delivery | Not started | Full application/output/batch acceptance and original-size restrictions resolved through evidence |

Open-source non-commercial research remains the main roadmap. No production profiles/default scanner changes, no develop merge, no export guard bypass.

Candidate E RAW is COMPLETE, exit 0; measured neutral drift makes it unsuitable for direct production integration. Real-film accuracy remains Not Verified.

Existing Draft PR #14 is the sole Phase 0 PR. a1415455 is preserved. Remote publication succeeded; review base codex/phoenix-phase0-base pins 4bbabb87 because origin has no develop. No replacement PR.

Current evidence: phoenix-gui-report.json, gui02-exit.txt, ablation-regression.json, metrics-regression.tap, device-range-checks.json, ABLATION_REPORT.md. Full GUI images remain under build/phoenix-phase0-gui-dedicated-02. Controller report: build/phoenix-phase0-ui-regression/report.json. Independent QQuickWindow captures validate app scene content, not physical monitor calibration.

Final current gate classification: Blocked. Original broad self-test reproduced a five-second completion deadline failure with busy=true, dirty=true, failed=false, Portra selected and render in flight. This is distinct from the PASS dedicated Phoenix window gate. Evidence: brightness-diagnostic-report.json. Do not describe Phase 0 as Passed until this remaining gate is resolved or explicitly accepted as a documented capability boundary.

User priority decision supersedes previous Blocked label: Phase 0 = Partially Validated / Known Issues, temporarily closed. Remaining issues are tracked in ENGINEERING_BACKLOG.md; no claim of full acceptance. Phase 1 is now In Development. Phase 2 may begin exploratory statistics on a credible first batch without waiting for 200 per stock; Phase 3 fitting requires assessment of directly blocking engineering issues. Phase 4–6 remain pending. No production scanner substitution.

Phase 1 expansion: In Development, 125 new candidate source-page records, only 3 downloaded images total and 2 visually reviewed for exploratory color/tone; formal fitting 0. Phase 2 exploratory per-image statistics begun with explicit ICC/untagged assumptions and watermark exclusion. This is not a completed film signature. Phase 3–6 remain pending; Phase 0 stays Partially Validated / Known Issues.

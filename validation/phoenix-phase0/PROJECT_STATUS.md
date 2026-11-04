# Phoenix Film Reconstruction — current status
Updated 2026-11-05. Original Phase 0–6 route remains authoritative; no PR was merged.

| Phase | Actual status | Evidence / limits |
|---|---|---|
| 0 | Partially Validated / Known Issues | Retained #14 and engineering backlog; scanner controls not promoted |
| 1 | First exploratory batch available; ongoing provenance work | Existing 32 private exploratory photos, 16 per stock; author/scanner/rights imbalance remains; collection paused for this task |
| 2 | Executed exploratory model comparison, ready for review | Author bootstrap/leave-one-out and scene groups; real saved Sony RAW/scan PNGs reused with fingerprints; skin ROIs and local HTML; Draft PR #16 |
| 3 | Controlled v0.1 protocol only | No numeric fit, implemented Research preset, Research PNG or new EXE; evidence does not yet identify defensible stock parameters |
| 4 | Not started | Future separate Research materials; preserve Round 5 |
| 5 | Not started | Independent held-out scenes/conditions and user visual review remain |
| 6 | Not started | Windows integration later; original-size export protection unchanged |

Current visual artifact: build/phoenix-phase2/index.html. Source/metric conditions: research/phoenix-phase2/REPORT.md and comparison.json. Current metrics use shared-shoulder digital baseline and Lab D65, not the earlier unshouldered/D50 measurements.

Phase 3 protocol: research/phoenix-phase3/RESEARCH_V01_PROTOCOL.md. No arbitrary increase in saturation, direct promotion of affine/identity/E scan controls, or material-profile rewrite. Once a first actual candidate and test build exist, stop subjective tuning and obtain user visual feedback before iteration. There is no such candidate build yet.

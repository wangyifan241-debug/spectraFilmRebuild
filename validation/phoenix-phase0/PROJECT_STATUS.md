# Phoenix Film Reconstruction — current project status
Updated 2026-11-04. Governing route remains the original Phase 0–6 plan.

| Phase | Current status | Actual evidence / remaining work |
|---|---|---|
| 0 Runtime audit | Partially Validated / Known Issues | Retained Draft PR #14 and engineering backlog; no repeated audit or forced merge |
| 1 Reference dataset | In Development | 38 local files, 32 unique private exploratory samples (16 per stock), 14 exploratory authors; source/license/scene balance still incomplete |
| 2 Film signatures | Exploratory analysis started | Actual Lab D65 / OKLab / linear-Y distributions on 32 images, author-balanced plots; no intrinsic film-response conclusion |
| 3 Unpaired model | Not started | No formal fitting; assess directly blocking engineering issues before fitting |
| 4 Research materials | Not started | Preserve Round 5; distinct materials/provenance later |
| 5 Visual validation | Not started | Independent held-out scenes, exposures/seeds, reference controls and human assessment later |
| 6 Windows delivery | Not started | Protected original-size export remains unresolved; no bypass |

The supplied-source batch imported 22 existing user files without redownload and downloaded 12 additional public image variants. Two resized/cropped mirrors, three small previews and one generation-unconfirmed image are excluded from exploratory statistics. Formal-fitting accepted remains zero. Unknown scanner is an explicit uncertainty, not an automatic exploration exclusion. Non-CC source images stay private and are not committed to GitHub.

Phase 1 continues on existing Draft PR #15. No duplicate PR and no branch merge. Reports: research/phoenix-reference-dataset/PROVIDED_BATCH_REPORT.md and provided_batch_summary.json. Five provenance-rule tests and three original-integrity/mask/sample tests executed successfully. Raw data lives only under build/phoenix-reference-dataset; supplied originals remain unchanged.

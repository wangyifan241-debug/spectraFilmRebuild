# Windows parity round 6 — isolation record

Date: 2026-11-03 Asia/Shanghai.

## Verified environment

- Original workspace: D:/学习资料/SpektraLab, branch codex/phoenix-validation-infrastructure-20261102 at 00405ee8ad5a448b09e64b1bf9195f1bb429f9a9. Dirty before this task; no existing files changed by round 6.
- Round 5: C:/Users/Strickland/.codex/worktrees/phoenix-round5/SpektraLab, branch codex/phoenix-round5-public-data at 34498abe26aa2624ebf5499d4d17ca47fbdb159b. Untracked validation/phoenix-round5/ and windows_UI/phoenix_round5/ observed. These directories must remain untouched.
- Round 6: C:/Users/Strickland/.codex/worktrees/windows-parity-round6/SpektraLab, branch codex/windows-parity-round6.
- Selected Windows baseline: 1bc1066c (codex/pr2-stability-colour-review-20261031), existing non-Phoenix Windows stability/colour branch. This is a provisional development baseline, not a claim that this branch is merged or approved.
- origin/main is 5e9568cad4bd64f7e720d04563639a071c701950 (initial landing page only). It cannot supply the Windows implementation.
- Remote Mac HEAD observed: 02d82b56cf65f0f8e802a708612edc4e9a4790af. The Mac source bundled in the selected Windows baseline is not yet verified against this HEAD.

## Output isolation

All future build/cache/test artifacts must be created under this worktree, using build/round6, build/round6-cache and validation/windows-parity-round6 respectively. No existing build or fixture output may be reused or overwritten. Real user settings and photo sidecars must not be used by tests.

## Initial source findings (not a completed parity matrix)

- engine/src/pipeline/windows_unported.hpp: windows_unported_error explicitly refuses digital intermediate and contrast mask. Scene latitude is explicitly supported. Film overscan/date are accepted on Windows. Acceptance is not evidence of visible correctness.
- modern_UI/Spektrafilm/Spektrafilm/Model/Session.swift: Mac opens through the engine, solves exposure and later the enlarger filter pack; inspect solveNow and all callers before implementing Process.
- ARCHITECTURE.md sections 7.5–7.8 specify working-space/display conversion, per-frame state, surface agreement, full-resolution soft proof and atomic export. These are acceptance requirements, not proof of Windows parity.

## Required next gates

Complete source-level matrix against pinned Mac source for Process/Solve, HDR, ICC, scene fit, native preview, post processing, half-frame, borders/date, export/proof, photo state/sync, CLI/MCP and settings/cache/diagnostics. Record exact functions, call chains, implementation category and acceptance tests; distinguish unverified from absent.

Process and HDR require separate implementation branches and draft PRs. Shared pipeline/printing/output-transform/parameter changes must document reason, scope and round-5 compatibility. Do not edit Phoenix material resources. Each new feature requires per-image save/restore, undo/redo and preview/export agreement tests. No integration or merge is authorized by a green unit test alone; create a separate integration branch only after modules have passed their gates.

## Status

Isolation completed. Full matrix, implementation, builds, UI verification and draft PRs remain outstanding. No feature-completeness or runtime-parity claim is made.

# Phoenix Phase 0 — preliminary audit (2026-11-04)
Status: In Development. No film refit authorized by this gate yet.

## Observed evidence
- HEAD: 4bbabb874447dd368ba314fe1394d9726f67d20f. Both requested commits exist but are not ancestors (git merge-base --is-ancestor exit 1). This does not establish whether equivalent changes were transplanted.
- No running SpektraLab/spk_qt process was returned by Win32_Process inspection. The executable actually used by the user remains unidentified.
- Candidate package: build/ip02/payload/SpektraLabQt.exe; SHA256 81B06DAF4F4719FB8B8A95595BE6B820129C8BBB29FF72D98164B649F557002D. Both Round 5 profile JSON files exist under its resources/profiles. Presence is not proof of runtime resolution.
- UI Main.qml:445-447 binds beforeImage to originalImage and showOriginal to originalMode/heldOriginal. FrameCanvas.cpp independently stores both buffers and increments texture revisions.
- PreviewController.cpp:138 rejects retired generations. This is static evidence only; race regression remains pending.
- phoenix_round5_preview.hpp initializes the selected Round 5 film with hardcoded kodak_portra_endura, runs film, then spectral scan -> inverse neutral response -> synthetic calibrated positive -> print_exposure multiplier -> highlight shoulder -> encoded ProPhoto RGBA16. It does not run the ordinary paper-print stage. Paper UI effects require explicit audit; do not assume paper is applied.
- phoenix_round5_scan.hpp builds a separate inverse neutral curve and RGB reconstruction matrix for each film. The matrix is fitted to 320 synthetic input probes, targeting reconstruction of the original input. This can suppress film differences by design. It is a testable leading hypothesis for weak before/after differences, NOT an experimentally established root cause.
- preview_host.cpp:678 blocks Round 5 original-resolution export for a documented device-lost investigation; :726 and :767 reject Process/scene fitting.
- Existing phoenix_round5_preview.cpp checks unequal hashes across stocks and exact reopen/switch-back persistence. Unequal hashes do not establish perceptible difference or film accuracy.

## Remaining gate requirements
Identify actual installed executable and resource origin; compare package inventory to source changes; record resolved parameters; fixed-input nonidentity test through real window; save original/render/difference images; compute canonical pixel hashes, MAE and color-managed D50 Lab DeltaE00; exercise pending-task ordering and export; add regression with demonstrated failure on broken behavior. No new tests or runtime image measurements have yet been performed. No accuracy claim or completed phase/PR is made.

## Commands used
PowerShell Get-CimInstance Win32_Process; Get-FileHash; Get-ChildItem; rg; git show; git merge-base --is-ancestor.

## Runtime follow-up
Historical evidence found read-only in C:/Users/Strickland/.codex/worktrees/phoenix-round5/SpektraLab. Five PNGs copied to build/phoenix-phase0-history-recheck; original analyze.cjs executed against copies, exit 0. Results reproduced: Phoenix I vs digital MAE 5.179223677/255, mean DE00 2.796812667, P95 4.517935114; Phoenix II mean DE00 1.607090137. Full recomputation: history-recomputed.txt and build/phoenix-phase0-history-recheck/metrics.json. This reproduces measurement of historical images, not a fresh render.
Old editor EXE full SHA256 verified D3FB0C549A6C6CBE4E56DB4BE97BBD2AA76741E615F2DB4C6F19E758CFDA4290. git diff e8b9abfb HEAD for shared scanner and preview headers returned empty: these two source implementations are identical. This does not prove all editor code or packaged resources identical.
Independent current research executable compiled and deployed successfully via ASCII junction at build/phoenix-phase0-ascii/app. First RAW invocation exited 1 with no output directory; subsequent synthetic invocation with compiler/Qt runtime PATH exited -1073740286 (0xC0000602), without a report. New render/ablation evidence is Blocked pending startup failure diagnosis. No production parameters, export guards or user caches changed. No new regression pass or Phase 0 pass claimed.

## Successful current render (follow-up)
Synthetic render completed exit 0 with actual PNG/TIFF, float32 stage buffers, per-film scanner recipes and report.json in build/phoenix-phase0-synth-02. Command used from build/phoenix-phase0-ascii/app: spk_phoenix_round5.exe --render <app>/resources synthetic <repo>/build/phoenix-phase0-synth-02. Process-local environment: QT_QPA_PLATFORM=windows; QT_QPA_PLATFORM_PLUGIN_PATH=<app>/platforms; QT_DEBUG_PLUGINS=1; QT_MESSAGE_PATTERN=[%{time process} %{type} %{category}] %{message}; compiler bin prepended to PATH. No global environment changes. stdout/stderr and exit saved as synth02-*.txt. Captured stderr is empty, so complete plugin diagnostics were not obtained via this redirection. Successful execution disproves a persistent startup blocker under these conditions; exact historical cause is not established.
DSC02443.ARW located at C:/Users/Strickland/Downloads/DSC02443.ARW. Same invocation with RAW input is in progress, output build/phoenix-phase0-raw-02; both Round 5 scan PNGs already exist, but completion/exit remains pending at this observation. These are baseline scanner renders, NOT the requested neutral/matrix ablations. No ablation result or full regression pass is claimed.

RAW completion confirmed: DSC02443.ARW run exited 0; stdout Experiment complete; build/phoenix-phase0-raw-02/report.json exists, 308296 bytes. Previous pending state is superseded. New A/B/C/D scan ablations remain outstanding.

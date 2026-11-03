# Process session-chain correction — 2026-11-04

This supersedes the first Preview's Process implementation. PR #12 remains Draft.

## Mac contract audited

`Session.ensureDeveloped` waits for the current decode and returns/reuses the live session. `openInService` resets the scheduler, calls solve(exposure), stores solvedEV as informational frame state, and reprints. `solveNow` calls ensureDeveloped, solveFilterPack, then client.render(.reprint). solveFilterPack calls solve(both), resets relative Y/M shifts and invalidates/resets the scheduler so a stale sent-parameter cache cannot suppress the subsequent update.

C++ `spk_solve` is read-only: exposure calls ensure_meter and reports EV; filter_pack digests a temporary Params copy with the database-neutral flag and reports C/M/Y. It does not write these reported values into session->params. The database neutral pack is already applied by digest when the normal session is built. Therefore the measured EV must NOT be added to the user's exposure-compensation wire field, nor the neutral filter values added again as shifts. A neutral picture may legitimately remain visually identical.

## Corrected Windows chain

Qt Process button → PreviewController::process → PreviewHost::process(shown immutable Frame) → reuse interactive spk_session (or rebuild from the same retained prepared decode after an export/measurement released GPU storage) → spk_solve(both) → validate all four numeric fields and verify color-negative neutral pack against live resolved params → reset relative Y/M through RenderSettings → render/spk_reprint on that interactive session → capture spk_get_params and spk_progress from the actual rendered frame → publish Frame and record one per-image undo step.

The Process operation always reprints, including when Y/M are already zero. It no longer destroys the interactive session to run an unrelated temporary probe. Retired snapshot requests are refused rather than silently solving a different photograph. Rebuilding after a deliberate session release retains the original decode/recipe and remeasures derived metering; no additive compensation is replayed.

Undo/redo and saved recipes retain the existing editing schema. Derived EV and resolved neutral values are captured from each actual rendered frame and atomically saved in a separate `<sidecar>.process.json` informational record. They are not control values and are never loaded back as gains; reopening regenerates fresh derived values from the saved recipe. This avoids turning a stale measurement into a physical input. Raw/source files are untouched.

## Main editor diagnostics

Left rail → 放大机 · 相纸亮度 → 配光诊断. The dialog exposes before parameters, solved_params (measured EV and neutral C/M/Y), exposure_ev_by_method, actual reprint resolved parameters and applied auto_exposure_ev, parameter_diff, session rebuild flag and change/no-change reason. Film/paper IDs and manual Y/M are in the resolved blocks. This replaces the earlier uninformative Process-complete-only behavior.

## Export blocker found and addressed

Real Sony ARW testing reproduced `Original render: spk_iir_df_acc: Vulkan compute dispatch failed (VkResult -4)`. Windows System event 4101 reported NVIDIA driver recovery. The corrected Vulkan backend batches IIR submissions by complete independent line/channel chains (four workgroups per submission), passing an explicit line/channel base. Every individual forward/backward recurrence retains its original order, initialization and float-pair arithmetic. Image planes are not spatially tiled; no blur halo or material parameter changes are introduced. Pipeline-layout and shader changes must ship together. Errors now include the failed kernel name.

Expanded gpu_vulkan_smoke shapes cross submission boundaries on both axes and compare against the existing independent float64 and analytic references, including in-place behavior and inactive channels. Narrow smoke + Process session tests passed after this change. A Sony rerun passed its formerly failing native export/readback and reopen steps, then exposed a synthetic-only assumption that every real scene must accept automatic fit. Synthetic acceptance remains mandatory; real-photo fit rejection is checked for zero mutation instead.

## Evidence and remaining acceptance

Tests assert actual rendered neutral values equal solved C/M/Y, actual applied metering equals the measured baseline, neutral Process creates a new reprint, repeated Process does not accumulate compensation, paired Y/M undo/redo, informational measurement persistence, reopen and original-size export agreement. Controlled one-stop scaling uses the same decoded scene to check the exposure meter's gain invariance.

The main application's QML self-test now dispatches the actual Process button, checks reprint, undo/redo, neutral repeat and resolved values, opens the actual diagnostic dialog and captures it. Native Windows execution is a separate gate from offscreen CTest; physical pointer/OS capture and HDR luminance hardware verification remain distinct.

Final full gates and Sony rerun results must be read from the accompanying delivery report; this note alone does not assert those pending runs passed. Dedicated finalized Phoenix I/II material fixtures and a full film/paper Process matrix are not yet certified. No Phoenix/material resources were changed. Shared Vulkan edits are execution scheduling and error context only; final integration still requires both rounds' pinned resources and non-Phoenix regression testing.

Additional native evidence and follow-up: the application-driven native Windows run exercised the actual QML Process handler, paired undo/redo, neutral repeat, rendered C/M/Y equality and diagnostic-open handler (249 report fields passed). Visual review caught the dialog outside the visible main window despite visible=true. The final source reparents it to Overlay and centers it; the application test now verifies its mapped scene rectangle lies fully inside the main window, and the text area uses the viewport width. These final UI changes require a fresh native capture; the earlier dim-only capture is not an accepted diagnostic UI pass.

The next Sony final run exposed a second watchdog failure in spk_sep_fir_acc. FIR submissions are now bounded to 256 workgroups (65,536 output pixels) using a pixel-base push constant. Each invocation still reads its full-plane neighborhood; this is not a spatial tile and requires no new halo approximation. A >65,536-pixel FIR oracle shape crosses this submission boundary and retains independent edge/accumulation checks. The final full gate and Sony rerun must validate both FIR and IIR fixes together before treating original export as certified.

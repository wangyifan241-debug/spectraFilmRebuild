# HDR module stage 1: main-editor display diagnostics

This branch is independent of Process and based on the same Windows baseline (1bc1066c plus isolation notes). It adds Advanced Color diagnostics to the existing FrameCanvas and the main editor histogram note. It does not implement an HDR renderer, an HDR toggle, float output transport or HDR export.

WindowsDisplay.hpp maps Qt screen GDI names to active DisplayConfig source/target paths, then queries DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO. Unknown API results and ambiguous clone targets remain unknown. Advanced Color is deliberately not labelled HDR: wide-gamut SDR can use Advanced Color too. The UI explicitly says application output is SDR. Screen changes refresh the information together with the existing ICC profile lookup.

Validation: Qt application and all test targets compiled. Full Windows CTest 38/38 passed (252.67 s), including the new invalid-display test. On this machine the actual attached display reports known=true, Advanced Color supported=false, enabled=false, 8 bits/channel, Windows status 0. Actual HDR image/display behavior cannot be certified on this SDR monitor.

No pipeline, printing, output-transform, material profile, parameter wire or per-photo recipe changes. This is monitor/window diagnostics and has no undoable photographic state. Full HDR must introduce per-photo output intent, save/restore, undo/redo, preview/export agreement, range-preserving transport and verified SDR fallback before enabling a main-editor HDR switch. Current Mac MetalCanvasView explicitly disables extended-range display; the requested HDR chain therefore extends both products' present display behavior rather than copying an existing active Mac canvas mode.

Remaining: true HDR main canvas/output, switch/fallback, runtime screen/profile-change observation beyond screen migration, UI pointer/OS capture, native HDR display verification and packaging. Do not merge this as completed HDR functionality.

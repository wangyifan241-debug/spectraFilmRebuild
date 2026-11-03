# HDR module stage 1: main-editor display diagnostics

This branch is independent of Process and based on the same Windows baseline (1bc1066c plus isolation notes). It adds Advanced Color diagnostics to the existing FrameCanvas and the main editor histogram note. It does not implement an HDR renderer, an HDR toggle, float output transport or HDR export.

WindowsDisplay.hpp maps Qt screen GDI names to active DisplayConfig source/target paths, then queries DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO. Unknown API results and ambiguous clone targets remain unknown. Advanced Color is deliberately not labelled HDR: wide-gamut SDR can use Advanced Color too. The UI explicitly says application output is SDR. Screen changes refresh the information together with the existing ICC profile lookup.

Validation: Qt application and all test targets compiled. Full Windows CTest 38/38 passed (252.67 s), including the new invalid-display test. On this machine the actual attached display reports known=true, Advanced Color supported=false, enabled=false, 8 bits/channel, Windows status 0. Actual HDR image/display behavior cannot be certified on this SDR monitor.

No pipeline, printing, output-transform, material profile, parameter wire or per-photo recipe changes. This is monitor/window diagnostics and has no undoable photographic state. Full HDR must introduce per-photo output intent, save/restore, undo/redo, preview/export agreement, range-preserving transport and verified SDR fallback before enabling a main-editor HDR switch. Current Mac MetalCanvasView explicitly disables extended-range display; the requested HDR chain therefore extends both products' present display behavior rather than copying an existing active Mac canvas mode.

Remaining: true HDR main canvas/output, switch/fallback, runtime screen/profile-change observation beyond screen migration, UI pointer/OS capture, native HDR display verification and packaging. Do not merge this as completed HDR functionality.

## Stage 2: range-preserving engine readback (2026-11-04)

Added `spk_reprint_rgb32f` and a separately owned `spk_float_result` without modifying the existing result layout. The readback occurs from the actual pipeline RGB buffer before `spk_to_rgba16` clamps/quantizes it. The new result uses the session's resolved gamut/TRC; linear-light consumers must explicitly disable output CCTF. The API and ownership contract are recorded in API-SPEC-callable-render-service.md. No material/profile or spectral/printing equation changes.

A real Vulkan fixture compares float values with a separate packed render under the same deterministic recipe, validates dimensions, finite values and lifetime after engine destruction, and checks repeated freeing of a cleared result. Its peak is 0.961798; this fixture does NOT demonstrate above-white HDR image content or HDR display.

Qt 6.11.3 release source inspection: qsgplaintexture.cpp accepts RGBX16FPx4 / premultiplied RGBA16FPx4 and creates RGBA16F textures when the RHI supports them. qsgrhisupport.cpp::applySwapChainFormat accepts scRGB, HDR10 and extended-linear P3 requests, checks isFormatSupported against the window, and falls back to SDR. An FP16 image alone does not select an HDR swapchain. These are implementation hooks, not proof that the shipping Qt binary, our scene graph, color management and output are configured correctly.

Additional source finding: the current Portra Endura EDR table has maximum target log2 Y=-0.0589642746, below one in linear Y. It is paper-response mapping, not a guarantee of above-white scene radiance. Simply enabling this node or multiplying an SDR image is insufficient for the requested HDR feature. Output reference white, scRGB absolute luminance interpretation, gamut handling and retained extended content must be designed together outside the material model.

Still unfinished: main-editor float image path and grading, actual swapchain state verification, HDR/SDR per-image intent and persistence, runtime switch/fallback, ICC/white-point/luminance semantics, HDR export format/encoding, and HDR hardware acceptance. No new HDR functionality is claimed from this transport test. The source archive inspected is the checksum-pinned Qt release corresponding-source archive used by the Preview package; it is not a certification of every prebuilt SDK internal source revision.

Stage-2 automated gate: application and all configured Windows targets built; CTest 39/39 passed (311.89 s). This includes the unchanged legacy C ABI fixture, display-capability diagnostics and the new RGB32F readback gate. It is not HDR image/display/export acceptance. The last runnable HDR-branch app is build/round6-hdr/app/SpektraLabQt.exe; its main editor remains SDR. No new HDR Preview installer is labelled complete while the requested canvas/output module is unfinished.

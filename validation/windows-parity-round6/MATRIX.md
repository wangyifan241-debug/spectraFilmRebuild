# Round 6 source parity matrix

Baseline Windows: 1bc1066c; reference: pinned Mac HEAD 02d82b56cf65f0f8e802a708612edc4e9a4790af, fetched read-only. Mac source diff inspected against Windows baseline: Session/Sidecar add external sidecar watching and three-way synchronization; Renderer changes display resources. Process solveNow/solveFilterPack remains the same. Remaining matrix entries describe source availability; cross-platform runtime parity remains unverified. Categories describe source availability, not runtime certification.

Paths under Mac mean modern_UI/Spektrafilm/Spektrafilm/. Windows means windows_UI/src/.

| Area | Classification before round 6 | Mac source / chain | Windows source / chain | Acceptance / outstanding |
|---|---|---|---|---|
| Process / Solve | engine present, UI missing | Model/Session.swift solveNow → solveFilterPack → client.solve(both) → reprint | engine.cpp spk_solve; PreviewHost only used exposure for pair composition | Explicit Process; reset relative Y/M together; retain exposure/material; save/reopen, undo/redo, exports |
| EDR/HDR display | neither current Mac nor Windows canvas is HDR | Canvas/MetalCanvasView.swift explicitly sets wantsExtendedDynamicRangeContent=false (also verified at pinned upstream HEAD); ROMM encoded 0–1 | Frame.bgra8 → FrameOutput displayFrame → FrameCanvas QSG texture (SDR) | Requested new capability requires float scene/display buffers, HDR swapchain, monitor capability and SDR fallback; no film model changes. Engine paper EDR mapping is separate from display HDR |
| HDR output | unverified / partial infrastructure | Export/Exporter.swift, canvas output shader | engine/host/output.hpp OutputTransform::to_linear; FrameOutput quantized output | Extended range must survive render/readback/grade/export; metadata/TRC validation required |
| ICC input | implemented, parity unverified | Import decoder → working ProPhoto | preview_host open_raw → ICC raster reader | Encoded and linear equivalent fixtures agree, invalid profiles refused |
| ICC monitor | partial | tagged canvas → ColorSync | FrameCanvas::refreshMonitorProfile → GetICMProfileW → QColorSpace; upload conversion | Known source difference: Windows FrameOutput maps ProPhoto through engine CAM16 into sRGB before monitor ICC; Mac tags working ProPhoto directly for ColorSync. Export/proof and monitor rendering must be separated deliberately. Different monitors, invalid profile, screen move/profile change and Windows Advanced Color interaction remain unverified |
| Scene fit | implemented, parity unverified | Session scene latitude calls | PreviewController::requestScene → PreviewHost::scene_latitude → spk_scene_latitude | Real Vulkan fixtures, rejected fits retain state; clipping/shadow invariants |
| Native preview | export backend present, UI incomplete | Session full render → Renderer | PreviewHost::render_original; editor retains reduced frame | 1:1 must request decoded native resolution, cancellation and photo changes cannot publish stale frames |
| Post processing | partial | Canvas/Renderer.swift + Shaders.metal | FrameOutput::applyDevelop, curves/zones/vignette | Match operation order and working encoding; full parameter matrix and surface agreement |
| Half-frame | implemented, parity unverified | Import/PairComposer.swift; Model/HalfFramePair.swift | PairProject.hpp → PreviewHost::open_pair → compose_pair | Independent source/geometry/exposure, restore and final native export |
| Borders / date | implemented, parity unverified | Model/Session+FilmEdge.swift; engine framing | RenderSettings edge/date → prepare_input → engine | Date before film response; frame/crop/native/export geometry agreement |
| Export | partial | Exporter.filePixels → write atomic; recipe target gamut/TRC | PreviewController::exportImage → render_original → output::exportFrame | TIFF/PNG/JPEG, atomic no overwrite, native size; output gamut recipes currently incomplete |
| Soft proof | missing dedicated surface | Session + export page use filePixels | export conversion exists; editor sRGB preview only | Proof at output recipe resolution matches reopened exported pixels |
| Photo library / parameter sync | partial | Model/Session.swift library/sync + sidecars | PreviewController library_, copy/sync; EditStore::Store | Independent per-photo history, select/reopen; preserve blocked sidecars and immutable material identity. Latest Mac Session.sidecarStoreChanged / SidecarWatch live external-edit synchronization has no equivalent Windows watcher verified |
| CLI / MCP | UI missing; host infrastructure present | Agent/AgentCLI.swift, MCPServer.swift → AgentWorkspace/AgentTools | main.cpp app/test entry; engine host tools separate | Shared controller/session implementation, schema, busy refusal, sidecar agreement; no equivalent Windows endpoint verified |
| Settings / cache / diagnostics | partial, parity unverified | Windows/SettingsWindow.swift; Session caches/logging | main.cpp settings, PreviewController caches/status, host Timings | Isolated test settings, retention/clear controls, reproducible error logs and resource provenance |
| DI / contrast mask | explicitly missing Vulkan | engine Metal pipeline | windows_unported.hpp rejects flags | Portable kernels, boundary/schema/parity tests; no silent acceptance |

## Module dependency and compatibility

1. Process UI/host probe (no new wire fields, no engine/material mutation). Process calls the existing engine on an immutable frame and resets relative Y/M through the existing per-photo history/save/render path.
2. Display capability diagnostics and ICC audit, before enabling HDR. A detected HDR display alone does not certify an HDR rendering path.
3. Float output transport and target transform, followed by HDR display/export and proof. Quantized RGBA16 cannot carry encoded values above one; avoid a UI toggle that promises unavailable headroom.
4. Native preview/proof, remaining export recipes, then library and automation.

Shared changes so far: engine/src/desktop/preview_host.{hpp,cpp} adds a read-only solve probe and releases/reconstructs the interactive GPU session just like scene measurement. No pipeline, printing, OutputTransform, wire parameters, profiles or baked resources are changed. Round 5 remains compatible at source and material level; final integration still requires both Phoenix generations and non-Phoenix regression fixtures.

All runtime/UI acceptance remains pending unless explicitly recorded in a test report. This is a complete requested-area inventory, not a claim of complete implementation or an up-to-date upstream audit.

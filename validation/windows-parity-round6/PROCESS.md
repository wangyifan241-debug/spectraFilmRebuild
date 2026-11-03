# Process module — validation and manual test guide

Windows baseline: 1bc1066c. Mac reference pinned at 02d82b56cf65f0f8e802a708612edc4e9a4790af.

## Main editor behavior

Open a photo in the existing Qt editor. Scroll the left rail to the printing section. The new **配光 Process** button is beside print exposure and Yellow/Magenta controls. While solving, the controller reports Processing and disables single-flight operations. Process uses the shown immutable decode, calls the existing engine solve(both), then resets the relative Y/M offsets together through the same per-photo save/history/render path used by sliders. Print exposure, film exposure, film/paper identifiers, geometry and post processing remain intact. On the current engine, solve returns database neutral filters rather than mutating the measured material.

The host solve probe is shared desktop boundary code; it releases and lazily restores the interactive GPU session, exactly like scene measurement. There is no engine wire, pipeline, printing model, OutputTransform or Phoenix resource change. build.ps1 accepts an optional CompilerDirectory so this workspace can use its own ASCII-path toolchain without changing another checkout's dependencies.

## Automated evidence

- Application and all Windows test targets built with MinGW 13.1.0 / Qt 6.11.3 in build/round6-native.
- Full CTest: 37/37 passed (279.23 s). This is the configured Windows suite, not macOS parity certification.
- New regression: deliberate removal of Process Y/M reset failed with `Process did not neutralize Y/M`; restored implementation passed (10.32 s).
- Actual Vulkan/controller fixture verifies paired Y/M reset, retained film identity/exposure, one-step undo and redo. Existing remainder verifies fit/refusal, save/reopen, export pixel agreement and immutable decode identity.
- Offscreen empty-editor image generated and inspected. It verifies startup/layout only; Process is below the visible rail portion and this capture does not prove interactive Process operation.
- Real-window input/capture: unavailable. Computer-use returned `native pipe unavailable ... os error 2` on two attempts. No live-window pass is claimed.

## Manual acceptance still required

1. Open owned copies of two photos; set Yellow=.20 and Magenta=-.15 on A, distinct settings on B.
2. On A press Process: observe solving status, both offsets return to zero and preview rerenders. Print brightness and film exposure should remain as set.
3. Undo restores both offsets in one step; redo reapplies the neutral pack. Switch A/B; confirm independent state. Close/reopen and confirm saved values.
4. Export TIFF16 and PNG at native size; compare with the corresponding output transform, and confirm the second photo is unaffected. Repeat after crop, date/border and post processing.
5. Repeat with each Phoenix generation once round-five resources are integrated, and Portra / a positive / a monochrome stock. Positive films have no printing Process button available.

## Remaining work

Process is wired into the editor, but live-pointer/UI verification and dedicated Phoenix integration fixtures remain pending. HDR output/display, native editor preview, export recipes/soft proof, display-color parity and latest-Mac external sidecar synchronization remain separate modules. Current SDR sRGB output must not be described as HDR.

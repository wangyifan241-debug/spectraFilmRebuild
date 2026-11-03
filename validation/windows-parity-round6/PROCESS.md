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
- Native Windows main-window test: the existing application self-test was run using QT_QPA_PLATFORM=windows with the software backend override removed. All 237 report fields passed. Its actual main-window grabWindow captures show the rendered canvas; native export, split/Space, geometry, post processing and isolated persistence passed. See native-report.json and native-canvas.png. This is an application-driven native-window test, not physical pointer input or an OS-level capture. It does not specifically exercise clicking the new Process button.
- External pointer/OS capture: computer-use returned `native pipe unavailable ... os error 2` on two attempts; manual Process interaction remains unverified.
- Deployed official build.ps1 gate: 37/37 passed (321.59 s); source and payload inventories recorded in qt-build.json before packaging.

## Manual acceptance still required

1. Open owned copies of two photos; set Yellow=.20 and Magenta=-.15 on A, distinct settings on B.
2. On A press Process: observe solving status, both offsets return to zero and preview rerenders. Print brightness and film exposure should remain as set.
3. Undo restores both offsets in one step; redo reapplies the neutral pack. Switch A/B; confirm independent state. Close/reopen and confirm saved values.
4. Export TIFF16 and PNG at native size; compare with the corresponding output transform, and confirm the second photo is unaffected. Repeat after crop, date/border and post processing.
5. Repeat with each Phoenix generation once round-five resources are integrated, and Portra / a positive / a monochrome stock. Positive films have no printing Process button available.

## Remaining work

Process is wired into the editor, but live-pointer/UI verification and dedicated Phoenix integration fixtures remain pending. HDR output/display, native editor preview, export recipes/soft proof, display-color parity and latest-Mac external sidecar synchronization remain separate modules. Current SDR sRGB output must not be described as HDR.

## Runnable Preview delivery

Source commit: 493c7e85a8c81e430ba435dc504a2d902f2a36ea. Later validation-only commits do not change the build inputs; the installer embeds this source commit and an exact source inventory.

- EXE: build/round6-native/app/SpektraLabQt.exe (run with its sibling deployed DLLs/resources).
- Installer: build/round6-preview-process/SpektraLab-2026.11.03-round6-process-windows-x64-setup.exe.
- EXE SHA256: 272ba3060640859e26d1ebae963b6304b1b58c7488c91df4dbafa1b6d1910f0c.
- Installer SHA256: 2b884046c95ddb6f4594c304c500a721352140ae7ba04e87f9361ab1cd18df17.
- Package test: 1,721 installed file hashes, isolated installed startup, overwrite refusal, safe uninstall, recipient-owned file preserved and existing uninstall registration unchanged. See installer-report.json.
- Actual limitation: the original deeply nested test installation directory failed with exit 3. An otherwise identical generated test runner using the shorter owned workspace build/i-9b49526c target passed. No installer check was disabled. Deep custom install paths are not supported by this Preview; use the default/short destination. This is not a clean-machine certification.

Operation location: left rail → scroll down to **放大机 · 相纸亮度** → **配光 Process**. The adjacent **亮度** slider controls print exposure; **黄色滤镜** and **洋红滤镜** remain available for relative manual offsets.

This package contains Process only. HDR diagnostics are on independent branch codex/windows-parity-round6-hdr (Draft PR #13); actual HDR output/switch/fallback remain unfinished. No experimental Phoenix resource has been imported, changed or integrated.

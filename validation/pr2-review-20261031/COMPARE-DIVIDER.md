# 分屏线在 100% 时越出预览区域：修复与回归

用户可见触发：导入照片，启用分屏，从“适合”切到“100%”，平移图片。图片比视口大时，分屏线会伸到顶部工具栏和左侧材料列表。

确认根因：`FrameCanvas::imageRect()` 正确返回可能含负坐标、超出视口的图片矩形；图片由 `FrameCanvas` 的 `clip=true` 裁剪。但 Main.qml 中分屏线是 canvas 的兄弟节点，未继承裁剪，且其拖动命中区域也超出画布。

修复仅涉及界面：将分屏线连同 MouseArea 移入 canvas，坐标改为 canvas 局部坐标。保持 1 个逻辑像素宽、图片坐标中的分屏比例，以及原图和成片共用的几何关系。未修改胶片/相纸数据、算法、图像像素或导出处理。

`SelfTest::testComparisonViewport` 覆盖六种状态：适合、100% 居中、平移至左上、平移至右下、分屏比例为 0、分屏比例为 1。每种状态抓取分屏关闭/开启的真实 QML 场景，逐像素检查画布外不被改动。另检查裁剪父节点、固定线宽，并通过 QQuickWindow 鼠标事件验证四个可见位置下拖分屏线只改变分屏比例、不平移图片。测试还继续执行已有原图/空格、裁切、历史、胶片材料及导出流程。

## 实际结果

- 红：仅添加像素回归测试，保留原兄弟节点实现，Windows 测试退出 1：`compare_viewport_1_no_pixels_outside_canvas`。适合视图通过，100% 居中失败。记录见 `compare-divider-red.txt`。
- 绿：修复后原回归通过；加入拖动检查后再次完整 Release 编译和 CTest：**35 通过，0 失败，0 跳过，210.37 秒**。`spk_qt_qml_workflow` 为 75.55 秒。完整测试日志见 `compare-divider-ctest.txt`。
- Windows 11 x64，Qt 6.11.3，MinGW 13.1，NVIDIA RTX 3050 Laptop。QML 场景测试使用 `QT_QPA_PLATFORM=offscreen`、Qt software scene graph 和拥有的合成 Bayer DNG；胶片模拟仍使用 Vulkan。这不是新的真实相机 RAW、可见窗口或物理显示器验收，不替代前次报告的测试范围。
- 原截图是在用户正在运行的旧程序中发现的。本次截图不会覆盖 `screenshots/` 中的用户截图。

## 重现命令与启动路径

```powershell
& C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1 `
  -SourceDirectory C:/Users/Strickland/SpektraLab-work `
  -BuildDirectory C:/Users/Strickland/SpektraLab-work/build/qt-compare-divider `
  -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 `
  -Jobs 6 -RunTests
```

新程序：`build/qt-compare-divider/app/SpektraLabQt.exe`。为保留用户正在使用的旧窗口，本次使用独立输出目录；旧窗口不会自动加载修复。构建清单见 `compare-divider-qt-build.json`。前次 `review.2` 安装包未重新打包，尚不包含此界面修复。

# Windows / macOS 功能核对（2026-10-28）

核对基线：`fd891c6`，保留本机已有的 7 个文件修改。Windows 是 `windows_UI/` 的 Qt Quick 前端，macOS 是 `modern_UI/Spektrafilm/` 的 SwiftUI/AppKit 前端；更新 main 不会自动更新 Windows 界面。

## 结论

当前 Windows 是部分功能移植，不能作为 macOS 完整版验收。构建成功和 CTest 通过不代表界面、功能或整张照片像素与 Mac 一致。

| 功能 | 当前 Windows 状态 | 代码依据 |
|---|---|---|
| 多 RAW 导入、胶片/相纸选择、参数自动更新 | 已有实现 | `src/PreviewController.cpp`、`engine/src/desktop/preview_host.cpp` |
| 颗粒、光晕、眩光、画幅尺度、曝光与滤镜 | 部分参数已接通 | `RenderSettings`、`opening_params`、`changed_params` |
| 裁切、90° 旋转、翻转、拉直 | 已有 Qt 输出实现 | `src/FrameOutput.cpp`、`qml/Main.qml` |
| 设置保存、撤销/重做、同步、批量导出 | 已有实现；完整 RAW 工作流需额外实测 | `src/EditStore.cpp`、`tests/library_workflow.cpp`、`tests/editing_workflow.cpp` |
| Mac 布局、菜单、交互与中英文本地化 | 尚未对齐 | `qml/Main.qml`、`DESIGN_ALIGNMENT.md` |
| 后期调色：白平衡、曲线、色彩平衡等 | Qt 前端未实现 Mac Layer 2 | Mac `Model/Adjustments.swift`、`Canvas/Renderer.swift`；Qt 控制器无对应完整状态/渲染链 |
| 原图比较、分屏、直方图 | Qt 前端未接入 | `qml/Main.qml`、`src/FrameCanvas.cpp` |
| 相机温度/色调、镜头校正 | Qt 界面未接通 | `DESIGN_ALIGNMENT.md`、`PreviewController.hpp` |
| 场景宽容度映射 | Vulkan 明确拒绝启用 | `engine/src/pipeline/windows_unported.hpp` |
| 胶片边框、日期印字、半格双拼 | 关键 Vulkan 功能缺失 | `windows_unported.cpp/.hpp` |
| 黑白胶片与银盐相纸 | 产品 profile 没有通过 Windows CMake 复制，配对/滤镜策略未对齐 | `engine/CMakeLists.txt` 仅复制 `resources/`；Mac `FeatureFlags.swift`、`StockCatalog.swift` |
| EDR/HDR、显示色彩管理 | Qt 当前固定 SDR sRGB，未完成 Mac 同等级验证 | `opening_params`、`README.md` |
| P3/ProPhoto 导出、软打样、配方和命名/EXIF | Qt 输出仅实现部分功能，当前以 sRGB 为主 | `src/FrameOutput.cpp`、`DESIGN_ALIGNMENT.md` |
| Cineon DI | Vulkan 明确拒绝；不同于已有的密度 tap API | `windows_unported.hpp`、`PORT_COVERAGE.md` |
| “不应用胶片模拟”（本次 main 新功能） | 仅 macOS 实现，Qt 没有对应入口和状态 | `git show fd891c6` |
| 局部蒙版、Tone Mask | Mac 当前功能开关也关闭，不能列作已开放 Mac 功能 | Mac `Model/FeatureFlags.swift` |

## 本地兼容修复的实际限制

`preview_host.cpp` 固定 `preview_long_edge=3400`，调用 `live` 渲染，导出直接使用该 Frame 的像素。因此原图大于 3400 像素时，导出也受到该上限约束；裁切只会进一步减少尺寸。`100%` 显示的是当前预览像素，不是相机原始分辨率。分条开关不能证明完整分辨率适合 4 GB 显存。

`PreviewController::metadata()` 的 `sourceWidth/sourceHeight` 来自已渲染 Frame，也不能当作原始 RAW 尺寸。

## 本次构建与验证

- 构建目录：`build/qt-20261028`，Release、Qt 6.11.3、MinGW 13.1.0。
- 完整编译和运行时部署成功；CTest **18/18 通过**，耗时 12.58 秒。
- CTest 覆盖部分核心数学、Vulkan、输出文件、设置存储和拒绝未实现功能的边界；`spk_desktop_preview` 默认仅检查 CPU 转换。
- 完整 RAW 库/编辑工作流测试不在这 18 项默认 CTest 中，需显式提供真实 RAW、拒绝样本和新输出目录。
- 当前未找到上次桌面上的 RAW 样本，未重做真实 RAW 界面导入、整图 Mac 对照或显示器 ICC 验证。

## 建议移植次序

1. 分离预览与原尺寸导出，修正原始尺寸信息；建立 4 GB 显存的大图验收。
2. 对齐 Qt 布局/菜单并接入原图比较、直方图和后期调色，验证预览/缩略图/导出的一致性。
3. 补齐产品胶片目录、黑白配对、相机白平衡和色彩空间导出。
4. 移植场景映射、胶片边框、日期和双拼涉及的 Vulkan 节点，再开放对应 UI。
5. 使用同一 RAW 和参数与 Mac 输出逐图比对，完成显示与导出验收。

## 2026-10-29 增量迁移（不是完整移植验收）

- Windows CMake 现在打包 `resources_product/profiles` 及其独立归属声明，不修改测量数据。
- 目录读取复用引擎 `load_profile`，支持 CineStill 800T 的 `derived_from`，避免把缺省继承的元数据误判为坏配置。
- 四款黑白胶片独立分组；切换胶片时原子选择兼容相纸，拒绝彩色／银盐交叉配对。黑白路径关闭颗粒子层，发送 Multigrade filter 2 的 C/M/Y=0/0/68；不把彩色校色偏移带入银盐相纸。
- 增加当前画面的 sRGB RGB／亮度直方图。统计输入直接来自画布使用的裁切、旋转后图像，使用有限采样网格；它不是场景线性宽容度仪表，也没有解决 Mac／Windows 工作色域的完整对齐。
- 新增 `spk_qt_stock_policy`：真实控制器／已打包目录、四款胶片自动配对、跨工艺拒绝、别名继承，以及通过真实 Vulkan 渲染的四款银盐合成色阶（中性 RGB、非平坦响应）。测试使用独立临时编辑目录，不读写用户设置或 RAW。
- `spk_qt_frame_output` 新增裁切加四分之一转的直方图／导出一致性测试，以及采样上限和空图测试。
- 新增 `--ui-smoke <new.png>`：无需 RAW，初始化实际 QML 窗口并捕获空编辑器；仅验证启动和控件存在，不等同于真实 RAW 或物理显示器验收。已成功运行一次；捕获文件在 `build/qt-20261028/ui-smoke-20261029.png`，没有替换用户的 `screenshots/`。
- 已向用户打开 `build/qt-20261028/app/SpektraLabQt.exe`。用户查看期间不强制关闭程序、不覆盖运行中的二进制。源码后续的 QML 属性重命名（消除 `Canvas.data` 名称冲突警告）尚需下次构建。

原尺寸导出、Layer 2、原图／分屏、胶片边框和其余差异仍未完成；本节不改变前文整体结论。

## 后续增量：CineStill 渲染故障与显影后基础处理

- CineStill 不能选中的渲染故障已通过测试复现为 `Vulkan kernel not ported: spk_highlight_headroom`。将 Metal 原节点移植为 GLSL、登记 Vulkan 绑定并纳入 SPIR-V 打包后，保持默认光晕开启的 CineStill → Portra → CineStill 连续渲染通过。没有改材料数据，也没有关闭光晕绕过错误。
- `spk_vulkan_pointwise` 为该节点增加解析参考与缓冲区边界检查；Mac/Python 整体色彩 parity 尚未在此 Windows 环境运行，不能据此声明整图与 Mac 一致。
- Qt 增加“显影后”面板：色温、色调、曝光、对比度、亮度、饱和度、高光、阴影和黑白场。按 Mac `layer2Tone` 的顺序实现，但暂时运行在现有 sRGB RGBA16 输出上，使用 sRGB 中灰支点；这是前端 CPU 后处理，不是 Vulkan 胶片引擎的 CPU 回退。
- 后处理状态独立于材料参数，接入预览、导航图、直方图、胶片条、裁切预览、单张／批量导出、撤销／重做、同步和每张照片的保存。文档 version 2 增加该状态；version 1 中性读取、保存时迁移，并保留损坏／未来版本保护。
- 新增自有合成 Bayer DNG，使控制器测试实际经过 LibRaw 和 Vulkan，检查选中 CineStill 后不回退、后期不重新生成胶片、撤销／重做、裁切加旋转后的 PNG 和批量输出像素一致、保存及重开一致。照片和编辑记录目录分别放在测试临时目录中，不接触用户 RAW 或编辑记录。
- 完整 Layer 2（ProPhoto 工作色域、曲线、色彩平衡、暗角）、原尺寸大图导出、原图／分屏、边框等仍未完成。
- 增加全部胶片的快速选择下拉框，避免嵌套滚动列表把 CineStill 等型号藏在当前可视范围之外；失败原因同时显示在画布上方，不只藏在右侧状态区。
- 本批验证：Release 构建成功，CTest 19/19（包括合成 DNG 控制器完整流程）；`spk_vulkan_pointwise` 在 RTX 3050 Laptop 上运行 137 个解析参考／边界用例；显影后面板空编辑器 QML 捕获成功、无运行时诊断。它们不是大尺寸真实 RAW、Mac 整图 parity 或显示器 ICC 验收。

## 2026-10-30 原尺寸导出增量

前文 3400 像素导出上限、原始尺寸误报属于审计当时的状态，本轮已改造。Qt 单张/批量导出从不可变解码快照重新生成原尺寸像素；画布仍为 3400 像素预览，不能称为原尺寸显示。真实 Sony 6024×4024 与合成 24/45 MP 输出验证、显存失败历史、最终 CTest 19/19、照片库 147 项和编辑流程 102 项结果记录于 [本轮验证](VALIDATION_2026-10-30.md)。后续执行以 [移植计划](WINDOWS_PORT_PLAN.md) 为准，其余未实现项没有因导出修复而消失。

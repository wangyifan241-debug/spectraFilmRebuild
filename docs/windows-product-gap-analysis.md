# Windows 产品能力与竞争差距审计

审计日期：2026-10-31；代码基线：`0dd07540`。范围：当前工作树的 Qt 桌面程序、共享 C++ 引擎、Vulkan 后端及 macOS 对照实现。只交付审计与实施设计，不修改运行代码。

## 证据口径与结论

- **事实 F**：已直接阅读当前代码，说明存在的实现及调用关系；不等于本轮已运行、跨显卡验证或确认视觉质量。
- **缺口 G**：在列明的实际入口中尚未实现或开放；不因旧移植文档说“缺失”就认定缺失。
- **建议 P**：拟议行为、指标和新增文件，均不是已交付功能。
- **待验证 V**：需要真机、相机文件、显示器或用户研究的数据。本轮未重建程序、执行 CTest、运行 macOS 测试、测速度/显存或拍摄屏幕；既有测试只作为覆盖设计证据。

Windows 已经是有完整基础编辑工作流的产品，而非 RAW 到 TIFF 的薄壳。其竞争短板集中在：可靠的输入与显示色彩契约、预览到原生画面的衔接、CPU 后期交互成本、可分享的创作配方，以及跨平台一致性的验收。依据是 [PreviewController](../windows_UI/src/PreviewController.cpp)、[FrameOutput](../windows_UI/src/FrameOutput.cpp)、[PreviewHost](../engine/src/desktop/preview_host.cpp) 的实际链路。

最值得保留的差异化是光谱负片→印相材料模型、可解释的 Scene Placement、可追溯材料版本；不能为获得讨喜效果修改测量曲线。已有 [RenderSettings](../engine/src/desktop/preview_host.hpp) 与 [Develop](../windows_UI/src/FrameOutput.hpp) 分离，足以作为创作配方的起点。

## 当前 Windows 功能枚举

“已实现”指有代码链路；“部分”指产品边界仍明显不完整。每项均给出入口。

| 功能 | 状态 | 已验证事实与边界 |
|---|---|---|
| RAW 导入、相机信息、方向 | F 已实现 | [PreviewHost::open_raw](../engine/src/desktop/preview_host.cpp) → [raw_decoder.cpp](../engine/src/io/raw_decoder.cpp)，保留线性解码和元数据 |
| RAW 高光余量模式 | F 部分 | `compatible16` 与 `headroom`；后者只接受受支持的常规三色 Bayer 条件，不能承诺 RAF/X-Trans、所有 DNG 都支持；见 [supported_headroom_source](../engine/src/io/raw_decoder.cpp) |
| TIFF/JPEG/PNG 摄影输入 | F/G 部分 | [host/image_io.cpp](../engine/host/image_io.cpp) 有栅格读取；Qt 的 [Main.qml](../windows_UI/qml/Main.qml) 导入筛选是 RAW/双拼，且 [PreviewHost::open_raw](../engine/src/desktop/preview_host.cpp) 直接调用 RAW 解码器。host 的能力不是 Qt 的能力 |
| 胶片、相纸、反转片、黑白配对 | F 已实现 | [PreviewHost](../engine/src/desktop/preview_host.cpp) 读取目录元数据，正片走 `scan_film`，银盐相纸有滤片策略；[stock_policy.cpp](../windows_UI/tests/stock_policy.cpp) 有策略测试 |
| 光谱模拟、颗粒、halation、glare、印相曝光与滤片 | F 已实现 | [RenderSettings](../engine/src/desktop/preview_host.hpp) 的控制进入 C API；[pipeline.cpp](../engine/src/pipeline/pipeline.cpp) 与 [Vulkan 内核构建清单](../engine/CMakeLists.txt) 实现处理。不能把 glare 当成竞品 Bloom |
| 胶片格式与颗粒尺度 | F 已实现 | `film_format_mm`、grain amount/active，见 [preview_host.hpp](../engine/src/desktop/preview_host.hpp)；物理幅面与裁切不是一个概念 |
| Scene Latitude/Placement、场景测量与拟合 | F 已实现 | [SceneLatitude.qml](../windows_UI/qml/SceneLatitude.qml)、[measureScene/fitScene](../windows_UI/src/PreviewController.cpp)、[scene_latitude.cpp](../engine/src/pipeline/scene_latitude.cpp) |
| 片边、漏光/雾化、日期、半格双拼 | F 已实现 | [FilmFrame.qml](../windows_UI/qml/FilmFrame.qml)、[PairProject.hpp](../windows_UI/src/PairProject.hpp)、[overscan.cpp](../engine/src/pipeline/overscan.cpp)；不能依据 `windows_unported.cpp` 文件名认定 Windows 未移植片边 |
| 原图切换、左右分屏、Fit/100%、拖动 | F 已实现 | [FrameCanvas.cpp](../windows_UI/src/FrameCanvas.cpp) 两个纹理节点共享画面矩形；[originalFrame](../windows_UI/src/FrameOutput.cpp) 从保留的线性 decode 生成原图，并处理片边画框。原图指软件解码基线，不是机内 JPEG |
| RGB/亮度直方图 | F 已实现 | [PreviewController::histogram](../windows_UI/src/PreviewController.cpp) 统计最终 `image_`；[displayHistogram](../windows_UI/src/FrameOutput.cpp) 确定性抽样 256 bins。是编码 sRGB 显示读数，不是场景 EV 表 |
| 裁切、比例、旋转、翻转、拉直 | F 已实现 | [Geometry / FrameOutput](../windows_UI/src/FrameOutput.hpp)、[controller](../windows_UI/src/PreviewController.cpp)；启用片边时有入片前几何特例，不能一律称为引擎后处理 |
| 后期白平衡、曝光、对比、亮暗部、黑白点、饱和度、暗角 | F 已实现 | [Develop/applyDevelop](../windows_UI/src/FrameOutput.cpp)、[Main.qml](../windows_UI/qml/Main.qml)，不重新生成胶片颗粒 |
| 曲线与色彩平衡 | F 已实现 | 五组曲线 RGB/luma/R/G/B，master/shadows/midtones/highlights 色彩区；[ToneCurve.qml](../windows_UI/qml/ToneCurve.qml)、[FrameOutput.hpp](../windows_UI/src/FrameOutput.hpp) |
| 按色相分区 HSL、取样器、局部蒙版 | G 缺失 | 当前 `Develop` 是色调区色彩平衡，不是红橙黄等 HSL 通道；[Develop](../windows_UI/src/FrameOutput.hpp) 无局部蒙版状态。macOS 也不应被假定为完整局部修图基线 |
| 照片列表、多选、同步设置、撤销重做、编辑自动保存 | F 已实现 | [PreviewController.hpp](../windows_UI/src/PreviewController.hpp)、[EditStore.cpp](../windows_UI/src/EditStore.cpp)；记录 v8 有材料与全局资源指纹，不是可分享配方格式 |
| 批量导出、取消、原尺寸渲染 | F 已实现 | [exportBatch/exportImage](../windows_UI/src/PreviewController.cpp) 顺序执行，`render_original` 释放交互 session 后渲染 full；取消在照片间检查，不是强制中断正在写文件 |
| TIFF16/PNG8/PNG16/JPEG8、质量和长边 | F 已实现 | [ExportOptions](../windows_UI/src/FrameOutput.hpp)、[Main.qml](../windows_UI/qml/Main.qml)，原子发布且不覆盖现有文件 |
| 输出色域选择、软打样、可保存导出配方 | G 缺失于 Qt | [exportFrame](../windows_UI/src/FrameOutput.cpp) 走 `displaySpace` 后写 sRGB；[ExportOptions](../windows_UI/src/FrameOutput.hpp) 没有目标色域字段；不能将 TIFF16 等同于宽色域输出 |
| 显示器 ICC | F 部分 | [FrameCanvas::refreshMonitorProfile](../windows_UI/src/FrameCanvas.cpp) 调用 `GetICMProfileW`，Qt 转换 sRGB 到显示器空间，跨屏刷新；还需真实桌面合成验证 |
| 自动原生分辨率画布、可调预览尺寸 | G 缺失 | [opening_params](../engine/src/desktop/preview_host.cpp) 固定 3400 长边、交互走 live；full 主要在导出中触发。“100%”是当前纹理像素 1:1，非大图源像素 1:1 |
| 材料导入、禁用、历史版本、来源许可 | F 已实现 | [MaterialLibrary.cpp](../windows_UI/src/MaterialLibrary.cpp)、[MaterialManager.qml](../windows_UI/qml/MaterialManager.qml)，内容 ID/哈希、私有资源视图、历史逻辑移除 |
| 创作配方收藏/分享/同图多缩略图、社区目录 | G 缺失 | 当前 [EditStore](../windows_UI/src/EditStore.hpp) 绑定源照片，材料包只定义材料；[MaterialLibrary](../windows_UI/src/MaterialLibrary.cpp) 不下载远程依赖；无独立 RecipeStore/配方预览服务 |
| Cineon DI、contrast mask | G 明确受限 | [windows_unported.hpp](../engine/src/pipeline/windows_unported.hpp) 拒绝参数；[spk_render_digital_intermediate](../engine/src/pipeline/engine.cpp) 非 Apple 返回错误；不是“有导出符号即可用” |
| 独立创作 Bloom、镜头校正与 RAW 解码白平衡 UI | G/部分 | [RenderSettings](../engine/src/desktop/preview_host.hpp) 未提供这些入口；[host/main.cpp](../engine/host/main.cpp) 有 RAW WB 请求但明确拒绝镜头校正，不能视作 Qt 已开放 |

## macOS / Windows 差异

| 层面 | macOS 真实入口 | Windows 真实入口 | 审计判断 |
|---|---|---|---|
| C ABI | [spk_engine.h](../engine/include/spektrafilm/spk_engine.h)、[EngineClient.swift](../modern_UI/Spektrafilm/Spektrafilm/Service/EngineClient.swift) | 同头文件、[windows_exports.def](../engine/windows_exports.def) | 符号共享，行为要看 `spk_capabilities` 与平台分支；新增 UI 通常不必新增 wire |
| GPU 像素交换 | [metal_gpu.cpp](../engine/src/gpu/metal_gpu.cpp)、[Renderer.swift](../modern_UI/Spektrafilm/Spektrafilm/Canvas/Renderer.swift) | [vulkan_gpu.cpp](../engine/src/gpu/vulkan_gpu.cpp)、[materialise](../engine/src/pipeline/engine.cpp)、[FrameCanvas](../windows_UI/src/FrameCanvas.cpp) | Windows 有 CPU RGBA16 回读、QImage 处理与 Qt 纹理上传；不是 Metal 零拷贝 |
| 后期 | [Adjustments.swift](../modern_UI/Spektrafilm/Spektrafilm/Model/Adjustments.swift)、[Shaders.metal](../modern_UI/Spektrafilm/Spektrafilm/Canvas/Shaders.metal) | [FrameOutput.cpp](../windows_UI/src/FrameOutput.cpp) | 数学可移植，GPU 执行方式与线程模型不可照搬；Qt 后期目前会在 setter 中同步生成可见图 |
| 解码 | [ImageDecoder.swift](../modern_UI/Spektrafilm/Spektrafilm/Import/ImageDecoder.swift) Core Image | [raw_decoder.cpp](../engine/src/io/raw_decoder.cpp) LibRaw；另有 [image_io.cpp](../engine/host/image_io.cpp) | 不同 RAW 场景估计不能要求最终像素相同；必须拆开解码一致性与引擎一致性 |
| 工作与显示空间 | [ColourManagement.swift](../modern_UI/Spektrafilm/Spektrafilm/Canvas/ColourManagement.swift)、tagged Metal layer | [PreviewController](../windows_UI/src/PreviewController.cpp) 使用 ProPhoto host，最终 sRGB QImage 再转监视器 ICC | Windows 先压到 sRGB，显示器更广色域不能恢复已经压缩的颜色 |
| 调度 | [RenderScheduler.swift](../modern_UI/Spektrafilm/Spektrafilm/Service/RenderScheduler.swift)、[Session.swift](../modern_UI/Spektrafilm/Spektrafilm/Model/Session.swift) | [PreviewController.cpp](../windows_UI/src/PreviewController.cpp)、[PreviewHost](../engine/src/desktop/preview_host.cpp) | Windows 已有串行 worker、180ms 合并、generation/revision 拒绝过时结果；缺少对应 macOS 的闲置 full 画布策略 |
| 导出 | [ExportRecipe.swift](../modern_UI/Spektrafilm/Spektrafilm/Export/ExportRecipe.swift)、[Exporter.swift](../modern_UI/Spektrafilm/Spektrafilm/Export/Exporter.swift) | [FrameOutput](../windows_UI/src/FrameOutput.cpp)、[exportBatch](../windows_UI/src/PreviewController.cpp) | 可移植目标变换、快照、命名和软打样语义，不移植 ImageIO 写入代码 |
| 所有显示表面 | [SurfaceAgreementTests.swift](../modern_UI/Spektrafilm/SpektrafilmTests/SurfaceAgreementTests.swift) | [frame_output.cpp](../windows_UI/tests/frame_output.cpp)、[qml_workflow.cpp](../windows_UI/tests/qml_workflow.cpp) | macOS 文档记录直方图裁切缺陷，不能机械复刻；Windows 已统计可见图，应保持自身正确链路 |

EDR 也需分层判断：[engine/CMakeLists.txt](../engine/CMakeLists.txt) 已构建 `spk_edr`，但 [PreviewHost](../engine/src/desktop/preview_host.cpp) 设置 `extended_dynamic_range=false`，Qt 最终显示为 8 位 sRGB。不能据此宣称 HDR 产品支持。

## 竞品参考的有效边界

以下只依据公开产品行为，不推断算法、精度、光谱测量方式或成片优劣；资料检索于 2026-10-31。各产品不是同一类别，手机拍摄、桌面照片编辑、视频插件不能以功能个数直接排名。

| 参考对象 | 公开证据 | 对 SpektraLab 的建议及本地入口 |
|---|---|---|
| Gloam | [官方用户协议](https://gloam-image.com/gloam-camera/terms-of-use?lang=zh-Hans) 确认拍摄、编辑、相册与导出；[App Store](https://apps.apple.com/cn/app/gloam-%E7%9B%B8%E6%9C%BA/id6753741715) 有相机/色彩配置更新。低门槛是本次用户给定的参考目标，尚非可量化结论 | P 在 [Main.qml](../windows_UI/qml/Main.qml) 提供“选配方→少量调整→导出”，高级物理参数仍可展开；不复制其相机资源或颜色配置 |
| Dehancer | [官方桌面手册](https://www.dehancer.com/learn/article/desktop-manual) 公开 film、grain、halation、Bloom 等工具 | P 将材料能力、物理效应与创作工具明确分组；当前 [RenderSettings](../engine/src/desktop/preview_host.hpp) 有前三类，Bloom 需独立定义，不能重命名 glare 冒充 |
| RNI Films | [官方移动站](https://www.mobile.reallyniceimages.com/) 可支持胶片摄影编辑定位；本轮官方证据不足以确认用户列出的配方分享、曲线/HSL 的具体版本行为 | P 将这些作为 SpektraLab 自身工作流目标，而非已证实竞品功能；曲线已有 [ToneCurve.qml](../windows_UI/qml/ToneCurve.qml)，HSL 与分享需要扩展 |
| Filmbox | [官方用户指南](https://videovillage.com/images/filmbox/features/Filmbox%20User%20Guide.pdf) 讨论输入/显示处理与色彩管理；[Looks 快速指南](https://videovillage.com/learn/filmbox-looks/getting-started/resolve) 明确 Looks 输出 Rec.709 Gamma 2.4，不能混同 Pro | P 学习显式输入/输出契约；以 [spk_output_transform](../engine/src/pipeline/engine.cpp) 建立可验证变换。不能据此声称跨平台像素已一致 |
| SpektraLab macOS | 当前仓库的 [ARCHITECTURE §7](../ARCHITECTURE.md)、上表 Swift/Metal 实现 | P 优先移植已经有数学和验收语义的功能，不将 macOS 历史性能数字写成 Windows 承诺 |

## 用户价值与产品定位建议

P 推荐定位为“可追溯物理胶片与印相模拟，加可复用创作配方的摄影工作台”。基础模式围绕挑选照片、尝试配方、原图对比、批量交付；专业模式再开放胶片曝光、场景放置与印相控制。理由：这些环节分别已经有 [libraryItems/批量同步](../windows_UI/src/PreviewController.cpp)、[FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[Develop](../windows_UI/src/FrameOutput.hpp) 与材料版本能力，而不是需要从零开发一个 RAW 编辑器。

V 尚不能宣称“物理最准”“比竞品更好看”“专业跨平台一致”。需独立建立中性阶梯/色卡、肤色、逆光、夜景、高饱和、黑白、片边、不同 RAW 相机样本；先用同一线性 ProPhoto 数组隔离引擎，再单独看 RAW 解码。随机阶段关闭后比较数值，颗粒开启时比较分布和 1:1 视觉；引用 [AGENTS traps 1/11/23/26/33](../AGENTS.md)、[parity_render.py](../engine/tests/parity_render.py)。具体优先级和验收在 [路线图](windows-product-roadmap.md)，实现设计在 [架构改进](windows-architecture-improvement.md)。

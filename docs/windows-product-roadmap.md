# Windows 产品改进优先级与实施路线图

日期 2026-10-31，代码基线 `0dd07540`。本文件全部排期、工作量和验收阈值是建议；当前事实与代码依据见 [产品审计](windows-product-gap-analysis.md)、[架构设计](windows-architecture-improvement.md)。本轮只写三份文档，不实施功能或承诺交付日期。

## 1. 排序方法与改进清单

价值 V：1 低至 5 高；成本 C：1=约 1–3 人日、2=4–8、3=9–15、4=16–30、5=超过 30；风险 R：1=局部可验证，5=跨驱动/色彩/资源同步。均为单熟悉代码工程师的粗估，包含实现与验证，不含硬件采购、资料授权、用户研究等待；不是实测效率。优先级先按正确性依赖和用户交付链排序，再参考 `V/(C+R)`，不会因低成本让装饰优先于色彩正确。

| 顺序/阶段 | 改进项 | V/C/R | 用户收益与排序依据 | 修改入口 |
|---|---|---|---|---|
| 1 / P0 | 输入、显示、导出色彩契约和可见诊断 | 5/2/3 | 防止错误输入或未校准显示被误认为胶片效果，后续宽色域工作的前置条件 | [FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[image_io](../engine/host/image_io.cpp)、[FrameOutput](../windows_UI/src/FrameOutput.cpp) |
| 2 / P0 | 原图、裁切、直方图、导出与版本的一致性门 | 5/2/2 | 现有功能可信，比重复开发已有对比功能更有价值 | [frame_output 测试](../windows_UI/tests/frame_output.cpp)、[main SelfTest](../windows_UI/src/main.cpp)、[EditStore](../windows_UI/src/EditStore.cpp) |
| 3 / P1 | 后期异步、最新结果优先和原图缓存 | 5/3/3 | 曲线和滑块不占 GUI，改善每次编辑 | [PreviewController](../windows_UI/src/PreviewController.cpp)、[FrameOutput](../windows_UI/src/FrameOutput.cpp) |
| 4 / P1 | P3/ProPhoto 导出、可保存导出设置和软打样 | 5/3/3 | 摄影后续调色不必先损失到 sRGB，文件结果可预期 | [ExportOptions](../windows_UI/src/FrameOutput.hpp)、[output transform](../engine/host/colour_transform.cpp)、[Main.qml](../windows_UI/qml/Main.qml) |
| 5 / P2 | 创作配方保存、导入导出、收藏与一键套用 | 5/3/2 | 让已有物理模拟与后期形成可重复结果 | [EditStore](../windows_UI/src/EditStore.hpp)、[RenderSettings](../engine/src/desktop/preview_host.hpp)、[MaterialLibrary](../windows_UI/src/MaterialLibrary.cpp) |
| 6 / P2 | 同图配方缩略图、简易模式和 A/B 快照 | 5/3/3 | 降低试片与参数学习成本，不复制竞品资源 | [Main.qml](../windows_UI/qml/Main.qml)、[FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[PreviewHost](../engine/src/desktop/preview_host.cpp) |
| 7 / P3 | 显存预算、预览尺寸和原生画布 | 5/4/4 | 看真实颗粒、稳定处理大图；由内存接纳约束 full | [vulkan_gpu](../engine/src/gpu/vulkan_gpu.cpp)、[render_original](../engine/src/desktop/preview_host.cpp) |
| 8 / P3 | Qt 栅格输入、相机 WB、headroom 支持提示 | 4/3/3 | 接入其他 RAW 开发器和更多摄影交接 | [image_io](../engine/host/image_io.cpp)、[white_balance](../engine/host/white_balance.cpp)、[raw_decoder](../engine/src/io/raw_decoder.cpp) |
| 9 / P4 | HSL 与独立 Bloom | 4/3/3 | 补常见成片控制，但先复用已存在曲线/色彩平衡 | [Develop](../windows_UI/src/FrameOutput.hpp)、[ToneCurve.qml](../windows_UI/qml/ToneCurve.qml) |
| 10 / P4 | 材料质量标签、依赖归档和版本恢复 | 4/3/3 | 数据升级后旧照片有清晰恢复路径 | [MaterialLibrary](../windows_UI/src/MaterialLibrary.cpp)、[EditStore](../windows_UI/src/EditStore.cpp)、[material_fit](../windows_UI/material_fit/README.md) |
| 11 / P5 | 完整 ICC 引擎与宽色域显示 | 4/4/4 | 专业显示与复杂输入覆盖；须真机验证合成链 | [FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[image_io](../engine/host/image_io.cpp) |
| 12 / P5 | contrast mask / Cineon DI 的 Vulkan 移植 | 3/4/4 | 专业流程补齐，不能阻塞基础照片工作流 | [contrast_mask.cpp](../engine/src/pipeline/contrast_mask.cpp)、[windows_unported](../engine/src/pipeline/windows_unported.cpp)、[engine.cpp](../engine/src/pipeline/engine.cpp) |
| 13 / P5 | 可选社区目录 | 3/4/4 | 在文件分享已稳定后扩大生态 | [MaterialLibrary](../windows_UI/src/MaterialLibrary.cpp)、[MaterialManager.qml](../windows_UI/qml/MaterialManager.qml) |
| 14 / 条件项 | Qt/Vulkan 零拷贝、异步 dispatch、HDR、镜头校正 | 3/5/5 | 需 profiling、平台与依赖研究，暂不作为首轮承诺 | [FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[vulkan_gpu](../engine/src/gpu/vulkan_gpu.cpp)、[raw_decoder](../engine/src/io/raw_decoder.cpp) |

## 2. 分阶段开发计划

以下路径标记“新增”的尚不存在。CMake 项目新增源文件先维护 Windows/engine CMake；若新增共享 engine `.cpp` 被 Xcode 编译，还须运行 `modern_UI/Spektrafilm/Tools/gen-project.py`。不为仅新增 docs 运行生成器。

### P0：建立可信基线（约 5–8 人日）

**修改**：`windows_UI/src/FrameCanvas.cpp/.hpp`、`windows_UI/qml/Main.qml` 的色彩/分辨率说明；`windows_UI/tests/frame_output.cpp`、`qml_workflow.cpp`、`editing_workflow.cpp`；新增 `windows_UI/tests/surface_agreement.cpp` 与 `windows_UI/CMakeLists.txt` 登记；必要时调整 `engine/host/image_io.cpp` 的错误策略和对应测试。

**依赖**：拥有可复制测试夹具、至少一个实际 Windows GUI、SDR 显示器；已有测试参考入口 [OriginalReference.hpp](../windows_UI/tests/OriginalReference.hpp)、[SyntheticDng.hpp](../windows_UI/tests/SyntheticDng.hpp)。先冻结当前 v8 编辑读取语义。

**验收**：

- 同一彩色标记图裁切、90°、翻转、拉直后，画布、原图、可见直方图及同尺寸导出对齐；有/无片边、半格双拼各有案例，filmstrip 的整图加裁切遮罩语义单独断言。
- 标明原图是软件解码，100% 对应 preview/native 的哪一种；ICC 失败原因能读到；不将 fallback 显示成“已校准”。
- 材料 hash 不符、损坏记录、缺 fixture 明确失败/阻断；记录测试执行数、skip 数、fixture 列表和产物路径，不仅写“0 failures”。
- 在真实窗口验证对比键、拖线、跨屏；offscreen 成功不能替代 live surface。

**退出条件**：形成可重复基线报告和自有 fixture；未完成显示器实测的项仍写 V，不包装成已验证色准。

### P1：高频编辑与宽色域交付（约 15–25 人日）

**修改**：`PreviewController.cpp/.hpp`、`FrameOutput.cpp/.hpp`、`Main.qml`；新增 `PresentationScheduler.hpp/.cpp`、`ColourPipeline.hpp/.cpp`、`ExportPreset.hpp/.cpp`；扩展 `tests/responsiveness.cpp`、`frame_output.cpp`、`editing_workflow.cpp`，登记 CMake。复用 `engine/host/colour_transform.cpp`、`output.cpp`，不重写 CAM16。

**依赖**：P0；P3/ProPhoto ICC 生成/读取的确定性测试，当前 `spk_output_transform` 支持的色域契约。输出预览与最终文件共享函数。

**验收**：

- 后期、几何与原图准备不在 GUI 做整图循环；快速修改、切片、关闭时旧任务不能覆盖当前帧。交互任务最多一个执行、一个最新待处理；导出不被悬停预览抢占。
- 保持现有旧 `legacySrgb` 编辑外观解释；五组曲线、四组色彩区与中性设置回归通过。预览与导出按相同工作尺寸和变换比较，不拿 3400 图与 full 粒子逐像素比较。
- 导出 sRGB/P3/ProPhoto 的像素与 ICC 匹配；TIFF16/PNG16 保留高位深，JPEG8 明确量化；不改变已存在文件、不留部分文件；EXIF/尺寸元数据检查。
- 软打样与导出采用同一目标参数；缩略软打样标为估计，提供原生尺寸验证。导出时使用不可变 snapshot，批量取消在文件间生效。
- 建议交互指标：固定硬件、3400 长边，GUI 事件延迟 p95 <50ms，后期可见结果 p95 <100ms；先测基线，不达标则记录瓶颈而非降低色彩精度。阈值是目标，不是已测数字。

### P2：创作配方与低门槛体验（约 15–25 人日）

**新增**：`Recipe.hpp/.cpp`、`RecipeStore.hpp/.cpp`、`RecipePreviewService.hpp/.cpp`、`qml/RecipeBrowser.qml`；新增 `tests/recipe_store.cpp`、`recipe_workflow.cpp`、`recipe_preview.cpp`。**修改**：`PreviewController`、`Main.qml`、`EditStore` 的事务接入、`windows_UI/CMakeLists.txt`。

**依赖**：P1 最新结果调度与颜色目的地；材料依赖采用当前 `MaterialLibrary` 内容版本，不另造 profile 加载器。schema/evaluator 版本先文档化。

**验收**：

- 自有 6–12 个示例配方覆盖肤色、暖调、冷调、夜景和黑白；每个配方记录作者、许可、材料依赖和调整值，不分发竞品 LUT/测量资源。数量是内容目标，质量需摄影师盲评。
- 保存→导出→另一独立编辑目录导入→同一输入重现；不含绝对路径、EXIF 或默认裁切；未知字段/版本、非有限数、缺材料、错误 hash 均失败且不修改当前编辑。
- 悬停/滚动预览不改 dirty/undo/sidecar；应用只入一次撤销历史；可选择只套后期或整套物理+后期。
- 缩略图严格有尺寸、缓存字节和队列上限；建议初始最多 12 张 512px 缩略、1 个低优先任务，实测后调整；卡片退出可见区撤销未开始任务，不为每张卡创建 GPU engine。
- 简易/高级模式显示同一状态；简单模式不偷偷替换材料，也不使用“自动变好看”掩盖物理曝光与印相曝光差异。

### P3：原生细节、大图稳定与输入扩展（约 20–35 人日）

**修改**：`engine/src/gpu/gpu.hpp`、`vulkan_gpu.cpp`、`engine/src/desktop/preview_host.cpp/.hpp`、`PreviewController`、`FrameCanvas`；新增 `MemoryBudget.hpp/.cpp`；栅格输入接 `engine/host/image_io.cpp`，RAW WB 接 `engine/host/white_balance.cpp` / `engine/src/io/raw_decoder.cpp`；编辑缓存键与 `EditStore` 必要的版本迁移；新增预算/调度/输入测试，登记两级 CMake。

**依赖**：P1 调度；P2 缩略图必须服从同一预算。明确 full 仅一个在途；扩展栅格输入前先完成 ICC 不支持的拒绝策略。WB 改动会使 decode 失效，不能仅重印负片。

**验收**：

- 预览尺寸设置、停止编辑后的可选 full、preview/native 标记一致；1:1 真正对应 native 才标源像素。full 过时后丢弃，切图/退出无悬空纹理。
- 覆盖标准 1MP、24MP、45MP、大于 45MP；4GB 等低显存环境测试片边/双拼峰值，无法容纳 full 则明确报错，不偷偷用预览导出。反复打开/导出后 pool 与 CPU 缓存回到预算内。
- OOM 注入、host-backed 存储、device lost 错误分类可区分；只释放 free 且 idle 的 buffer；不删除节点 flush。
- 同一场景不同编码输入保持结果；浮点/整数、sRGB/P3/ProPhoto、有 ICC/无 ICC/坏 ICC 分开测试。headroom 不支持原因可见；不对不同 RAW 解码器承诺像素相同。
- 所有数据记录 CPU/GPU/驱动/Qt 后端、输入哈希、render 参数、资源哈希、skip 数。对照实验交错执行，不能顺序测两组再断言性能提升。

### P4：创作工具与可复现材料（约 15–30 人日）

**修改/新增**：`FrameOutput` 与新 `CreativeAdjustments.hpp/.cpp`、`qml/HslEditor.qml`，Recipe evaluator 新版本；`MaterialLibrary`、`EditStore`、`MaterialManager.qml` 增加质量/依赖说明与旧资源恢复；新增中性、色相环、亮部扩散、旧版本及缺依赖测试。若新增 GPU 内核，改 `engine/src/shaders/vulkan/` 与 `engine/CMakeLists.txt`，保留 CPU 参考。

**依赖**：P2 配方版本、P3 预算；Bloom 先完成自有定义和合法素材；材料归档容量策略先评审。

**验收**：HSL 环首尾连续、灰色无异常色相、关闭恒等；Bloom 不改实测 profile、默认关闭、不同预览尺度的扩散尺度定义一致；旧配方保持旧评估顺序。材料更新不覆盖旧引用，禁用不破坏旧照片，恢复所需数据可定位；结构校验与实测认证分开显示。Status M 拟合报告不能越过 `profile_ready=false` 的门。

### P5：专业与生态扩展（每个子项目单独估算）

**完整 ICC/宽色域显示**：修改 `ColourPipeline/FrameCanvas` 与输入解析适配；依赖 WCS/LittleCMS 选型、授权、Qt/Windows 合成验证。验收 LUT ICC、监视器配置切换、SDR/Advanced Color 的单次变换；HDR 另立规范。

**contrast mask/Cineon DI**：移植 `engine/src/pipeline/contrast_mask.cpp` 与相关 Metal 内核到 Vulkan，调整 `windows_unported` 平台拒绝、`engine.cpp` 能力宣告和测试。`spk_export_di` 的旧归一化包与 `spk_render_digital_intermediate` 的 Cineon 路径必须分开验收，不以同一个 DI 名称判断。改变 wire 时同步改 `API-SPEC-callable-render-service.md`。同一线性输入跑引擎 parity 与 C ABI 拒绝/成功边界测试，再开放 Qt UI。

**社区目录**：新增 `CommunityCatalogClient` 和浏览 UI，复用 MaterialLibrary 导入；依赖 P2/P4、签名/撤销/缓存/许可政策。验收离线可用、坏签名/超限/路径逃逸拒绝、无可执行下载、许可证与作者可读、撤销不删除用户仍引用的版本。首轮不必有账号、云编辑或商店。

**原生纹理互操作/异步 Vulkan**：仅 profiling 确认瓶颈后立项。修改 `vulkan_gpu`、`FrameCanvas` 和资源所有权接口；验收 fence/布局/设备丢失/关闭与 in-flight 生命周期；不能以减少 wait 数量替代正确性。

## 3. 测试门与交付要求

F 测试登记的权威是 [windows_UI/CMakeLists.txt](../windows_UI/CMakeLists.txt) 与 [engine/CMakeLists.txt](../engine/CMakeLists.txt)。其中 `spk_qt_editing_workflow_test` 是要求显式 fixture/output 参数的可执行程序，不应想当然认为普通 CTest 已运行它。既有 `VALIDATION_2026-10-30.md` 是历史记录，不代表本次工作树已验证。

P 每次实施使用新鲜或确认已重建的构建目录，先列出测试：

```powershell
ctest --test-dir <fresh-build> -N
ctest --test-dir <fresh-build> -R 'spk_qt_(frame_output|edit_store|responsiveness|qml_workflow|material_library|material_workflow)' --output-on-failure
ctest --test-dir <fresh-build> -R 'spk_windows_(feature_boundaries|film_edge|c_abi)' --output-on-failure
```

上面是计划命令，不是本轮执行记录；真实名称和环境以新构建的 CTest 清单为准。Qt 测试 DLL PATH 按 CMake 环境配置；不要依赖旧 shell PATH。新回归测试先证明旧/故意破坏行为能失败，再验证修复。

P UI-only 改动运行受影响 Qt 门，不额外跑 engine parity；engine 输入/内核/资源/边界变动才跑对应 Vulkan/数学/C ABI 与同输入跨后端 parity。一个完整阶段或合并前跑 Windows 完整门一次；涉及共享代码，在 macOS 运行受影响类并在集成边界跑完整 `SpektrafilmTests`，明确 fixture-dependent skip，不将本机 Windows 无法执行的 macOS 检查填成通过。

P 每阶段产物包括：变更文件/构建 ID、验收矩阵、测试数与跳过原因、live 窗口截图及环境、色彩空间与哈希、性能原始数据、未解决问题。性能与视觉质量分开结论；截图不能证明数值 parity，unit green 不能证明画布，随机关闭的 parity 不能证明颗粒分布。

## 4. 首轮范围与不做事项

建议首轮落实 P0→P1→P2，随后根据大图用户比例和实测决定 P3 的 full 画布与输入扩展先后。保留已有材料库、曲线、原图对比和批导实现，避免把审计误变为重写。P4/P5 需前序验收后再进入。

本次没有调整测量材料数据、波长轴、特征曲线、工作精度或随机模型；没有引入 Python 运行时、竞品专有素材/算法假设，也没有提交或推送。尚未测量的颜色准确性、交互时间、显存峰值与竞品主观效果，均是未来验收任务而非本次结论。

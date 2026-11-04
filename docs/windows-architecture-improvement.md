# Windows 架构改进设计

日期 2026-10-31，基于 `0dd07540`。F=代码已核对，P=建议，V=待实测。本文的新增类与文件名均为建议，不是当前实现；详见 [能力审计](windows-product-gap-analysis.md)。

## 1. 以现有边界演进

F 当前实际链路：

```text
Main.qml → PreviewController → 串行 PreviewHost
  → LibRaw / 线性 ProPhoto float → spk_open / render(live)
  → Vulkan → 独立 CPU RGBA16 编码 ProPhoto
  → FrameOutput：Develop → 几何 → sRGB 输出变换
  → QImage ARGB32 → FrameCanvas：监视器 ICC → Qt 纹理
导出：不可变 Frame → render_original(full) → Develop/几何 → sRGB → 文件
```

依据：[PreviewController.cpp](../windows_UI/src/PreviewController.cpp)、[preview_host.cpp](../engine/src/desktop/preview_host.cpp)、[engine.cpp::materialise](../engine/src/pipeline/engine.cpp)、[FrameOutput.cpp](../windows_UI/src/FrameOutput.cpp)、[FrameCanvas.cpp](../windows_UI/src/FrameCanvas.cpp)。引擎不写文件；桌面 host 与独立的 [stdio host](../engine/host/main.cpp) 是不同入口，不应把后者全部功能记到 Qt 名下。

P 保持三个稳定边界：材料只描述材料；RenderSettings 描述物理过程和入片前场景放置；Develop/创作效果描述扫描后的风格。Recipe 是上述状态的声明式组合，不是修改 Profile 的脚本。先补契约和任务编排，再按实测决定是否替换像素传输架构。

## 2. 色彩、ICC 与输入风险

### 已验证风险

| 风险 | 真实代码依据 | 后果与建议 |
|---|---|---|
| 嵌入 ICC 不受支持后的回退 | [host/image_io.cpp::parse_icc/space_for](../engine/host/image_io.cpp)：支持 RGB matrix/TRC；无 matrix colorants 的 LUT ICC 不支持，产生 ignored/assumed sRGB 提示 | F 在 host 栅格输入上存在；Qt 当前 RAW 链路不走这里。P Qt 开放 JPEG/TIFF 前先补严格策略，禁止将明确有但不支持的 ICC 静默当 sRGB |
| 无标记深位 TIFF 的假设 | [image_io.cpp](../engine/host/image_io.cpp) 将无 ICC deep TIFF 假设为线性 ProPhoto | P 导入时显示解释并允许显式指定；位深不证明色域/传递函数；保存解释到编辑状态 |
| 显示先压缩到 sRGB | [FrameOutput::displayFrame/exportFrame](../windows_UI/src/FrameOutput.cpp) 与 [colour_transform.cpp](../engine/host/colour_transform.cpp) | F 广色域在进入画布 ICC 前已经受 sRGB 目标约束。P 先提供宽色域文件输出，再做宽色域显示，避免两项混淆 |
| ICC 转换不等于验证桌面合成 | [FrameCanvas](../windows_UI/src/FrameCanvas.cpp) 在 `updatePaintNode` CPU 转换并上传 | V 要确认 Qt 后端、Windows SDR/Advanced Color 路径是否再次处理；不能仅凭 GetICMProfileW 成功宣布色彩正确 |
| ICC 更新检测有限 | [FrameCanvas::refreshMonitorProfile](../windows_UI/src/FrameCanvas.cpp) 连接 window/screenChanged | P 处理同屏配置文件替换、显示设置变更与手工刷新；将 profile hash 纳入纹理缓存键 |
| 监视器空间输入能力差异 | 同文件使用 `QColorSpace::fromIccProfile`，无效时 sRGB fallback | V 用矩阵、LUT、损坏与缺失 ICC 实测 Qt 接受度；P 暴露降级原因，不将输入解析器限制直接套给 Qt 显示解析器 |

P 建议新增 `windows_UI/src/ColourPipeline.hpp/.cpp`，将源空间、编码、目标、渲染意图、变换版本、ICC hash、降级原因集中描述；显示和导出各有目的地，不能复用“已转换到监视器”的像素写文件。默认保持现有 SDR 行为，先在 `FrameOutput::ExportOptions` 加 sRGB/P3/ProPhoto 目标，再让 `exportFrame` 使用同一个目标变换创建像素并嵌入匹配 ICC。[spk_output_transform](../engine/include/spektrafilm/spk_engine.h)、[host/output.hpp](../engine/host/output.hpp)、[colour_transform.cpp](../engine/host/colour_transform.cpp) 已可复用，不需要从零写颜色科学。

P 若要支持完整 ICC，先评估 Windows WCS 或 LittleCMS 的覆盖、依赖与 GPL 兼容性，再通过统一接口接入；未选型前不承诺任意 ICC。输入策略至少分为明确可转换、无标记需解释、存在但不支持应拒绝/要求确认。HDR/EDR 独立立项，不能把 `spk_edr` 内核存在当成 Windows HDR 输出支持；[PreviewHost](../engine/src/desktop/preview_host.cpp) 当前明确关闭 EDR。

P 色彩验收分三层：同一线性输入的 Metal/Vulkan 引擎比较；编码 sRGB/P3/ProPhoto 对同一场景的导入不变性；真实显示器的单次变换与跨屏测试。不得用截图证明显示器物理色准；记录桌面模式、Qt 后端、ICC 哈希和必要的仪器读数。

外部 API 依据：[Microsoft GetICMProfileW](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-geticmprofilew) 描述获取设备上下文关联的配置文件，并不提供画布已正确转换的证明；[Qt QColorSpace](https://doc.qt.io/QT-6/qcolorspace.html) 明确并非支持所有 ICC，不支持时返回无效空间。该在线 Qt 文档不能代替仓库实际固定 SDK 的验证，测试须记录 Qt 版本。

## 3. RAW 与预览调度

F [raw_decoder.cpp](../engine/src/io/raw_decoder.cpp) 的 compatible16 路径输出线性 ProPhoto，但 uint16 处理阶段不能视为无裁切浮点 RAW；headroom 保留浮点转换余量，同时在 `supported_headroom_source` 有明确相机/传感器约束。[white_balance.cpp](../engine/host/white_balance.cpp) 是独立 host 的温度/色调辅助，不等于 Qt 开放了相机白平衡。镜头校正在 [host/main.cpp](../engine/host/main.cpp) 明确拒绝。

P 新增 `InputSettings` 时将 decode mode、camera WB、输入解释、解码器版本、镜头校正版本与 RenderSettings/Develop 分开。解码缓存键必须包括它们；headroom 不支持时提示原因，兼容模式应由用户选择而非偷偷降级。高光恢复算法与扩大 float 动态范围不是同一功能，不承诺前者。测试覆盖 Bayer/X-Trans、不同压缩 RAW、旋转、无 WB/损坏文件；参考 [raw_headroom.cpp 测试](../engine/tests/raw_headroom.cpp) 与 [windows_raw_unsupported.py](../engine/tools/windows_raw_unsupported.py)。

F [PreviewController](../windows_UI/src/PreviewController.cpp) 已有串行 worker、180ms auto apply、generation 与 settings revision；`deliver` 在 worker 准备显示图，但 `setPostAdjustment`、几何修改和 `originalImage` getter 仍可能同步进行 QImage 生成。已存在的 [responsiveness.cpp](../windows_UI/tests/responsiveness.cpp) 证明的是注入图像转换的线程归属设计，不能证明所有后期滑块或真实窗口都不卡。

P 新增 `PresentationScheduler.hpp/.cpp`：

1. 不可变请求含 frame/source identity、decode revision、physics revision、develop revision、geometry revision、目标尺寸、目的地与色彩配置版本。
2. 交互后期只保留最新待处理项，CPU 结果在 worker 生成，GUI 只发布；原图也异步缓存，getter 不做重任务。
3. 交互物理渲染优先；idle full、配方缩略图低优先；同一 engine 不并发使用，旧结果按完整键丢弃。
4. 初期 full 用现有 `render_original` 的快照语义，但它会释放交互 session；若把它用于画布，要显式接受下次编辑重建成本，并提供取消与资源预算。
5. 设置预览长边，默认可先沿用 3400；idle full 以配置开关渐进上线，显示 native/preview 标记。“100%”同时显示对应的源分辨率，避免误以为在看原片颗粒。

参考 [macOS RenderScheduler](../modern_UI/Spektrafilm/Spektrafilm/Service/RenderScheduler.swift) 的 sent/wanted 合并语义，复用规则而非 Swift actor。现阶段不改同步 Vulkan dispatch：在 [vulkan_gpu.cpp](../engine/src/gpu/vulkan_gpu.cpp) 改成异步命令之前，必须重做 in-flight buffer 生命周期、fence 与池复用证明，不能删除 flush 假装优化。

## 4. GPU 显存与内存预算

F [Vulkan 后端](../engine/src/gpu/vulkan_gpu.cpp) 已有内存池、staging 复用、实际 allocation 记账，以及回收空闲池后重试、特定 device OOM 时 host-backed 分配路径。不能说“完全没有 OOM 处理”，也不能将 host-backed 存储称为 CPU 渲染 fallback：计算仍是 Vulkan。

F [opening_params](../engine/src/desktop/preview_host.cpp) 设置 `striped=true`、128 行，但片边路径会关闭 stripes，部分空间算子也保留全平面。3400 预览是约束之一，不是 full 导出显存上限保证。[render_original](../engine/src/desktop/preview_host.cpp) 已主动释放交互 GPU session；CPU decode/FramePtr/QImage 则仍可同时保留。

P 预算分开统计 GPU device-local、host-backed GPU buffers、staging、CPU decode、工作 QImage、显示纹理、待导出图；不将 `spk_memory_report` 当成进程总内存。45MP 的 RGB float32 单平面约 540 MB、RGBA16 约 360 MB、BGRA8 约 180 MB（十进制理论尺寸，不是本机实测）；多个平面和双拼会叠加，必须记录 actual allocation。

P 在 [gpu.hpp](../engine/src/gpu/gpu.hpp) / [vulkan_gpu.cpp](../engine/src/gpu/vulkan_gpu.cpp) 增加设备预算查询能力，`VK_EXT_memory_budget` 可用则用、不可用则保守上限；新增桌面 `MemoryBudget` 负责接纳任务。preview/配方图可自动降尺寸，最终导出不得静默降分辨率；不足时明确拒绝并保留编辑。不可宣称任意 tiling：halation、IIR、片边与全图统计必须单独证明分块等价和 halo。

V 需要 Intel/AMD/NVIDIA、集显/独显、4/6/8GB 级别的覆盖，完整 RAW、片边、双拼、批导、反复打开、OOM 注入与设备丢失。记录峰值、每类池持有、回收后稳态、首帧及交互 p95；不沿用 Metal/Apple 统一内存数字作为 Windows 验收。

外部设计参考：[Khronos memory budget 示例](https://docs.vulkan.org/samples/latest/samples/extensions/memory_budget/README.html) 展示按 heap 查询预算与消耗。P 应持续采样预算并保留余量，而不是把一次读到的数值当成永远可分配的额度；仍须保留当前分配失败恢复路径。

## 5. 不改变测量数据的创作配方

F 已有分层：`RenderSettings` 在 [preview_host.hpp](../engine/src/desktop/preview_host.hpp)，`Develop/Geometry` 在 [FrameOutput.hpp](../windows_UI/src/FrameOutput.hpp)，照片状态在 [EditStore](../windows_UI/src/EditStore.hpp)。配方不可直接导出照片 sidecar：它包含源身份、几何和照片专属状态。

P 新增 `Recipe.hpp/.cpp` 与 `RecipeStore.hpp/.cpp`，建议 v1 声明：

```json
{
  "format": "SpektraLab.CreativeRecipe",
  "version": 1,
  "id": "author.recipe",
  "revision": 1,
  "name": "Warm print",
  "author": "author",
  "license": "declared-license",
  "requires": {"recipeEvaluator": 1, "features": []},
  "materials": {"film": {"id": "...", "sha256": "..."}, "paper": {"id": "...", "sha256": "..."}, "resourcesSha256": "..."},
  "process": {},
  "develop": {},
  "creative": {"version": 1},
  "applyPolicy": {"decode": "preserve", "geometry": "preserve"}
}
```

这只是结构示意，`process/develop` 必须在实现时定义严格白名单、有限值及范围，不能任意转发 JSON 给引擎。默认不携带源路径、EXIF、照片裁切、双拼源文件、日期私有文本、camera WB。允许显式选择只套后期或物理+后期；应用前解析依赖，形成一次可撤销事务，缺材料/版本不符先拒绝，不替换为相近材料。

P 层次顺序：输入解释/解码 → 入片前 Scene Placement → 胶片/印相 → 扫描后 Develop → 独立创作效果 → 几何 → 目的地变换。遵循现有几何在片边下的入片特例，不能为配方统一顺序而改变旧片边效果。色调区与 HSL 的顺序一旦确定写入 evaluator version，旧配方固定旧规则。

P 强度滑块只在明确定义的创作参数间插值：曝光 EV、曲线控制点/采样表、色彩向量；不对胶片 ID、物理密度曲线、波长响应插值。若要混合成片，只允许同工作空间的前后期结果，明确它是创作混合、不是“半份乳剂”。物理过程设置可直接保存，但不能给全部设置一个含义不明的强度旋钮。

P 新增 HSL 需要色相环连续插值、低饱和保护、色相接缝测试；可放进新 `CreativeAdjustments`，避免更改旧 Develop 的评估含义。独立 Bloom 必须写明作用于何种线性光、阈值/扩散/合成顺序，默认关闭，使用自有算法与合法测试素材；不能修改 profile 或借用竞品 LUT。首版先做已有曲线/色彩平衡配方，HSL/Bloom 后置。

P `RecipePreviewService.hpp/.cpp` 以当前照片生成缩略图：后期配方复用同一工作图，物理配方经串行 host 渲染；缓存键包含源/解码/物理/配方 evaluator/材料/几何/输出变换/尺寸/随机策略。悬停不写 EditStore，点击确认才提交。缩略图不烘入监视器 ICC；显示时统一转换。控制可见卡片队列和字节上限，禁止每张卡保留 full decode 或创建一个 engine。

## 6. 材料库与社区扩展

F [MaterialLibrary.cpp](../windows_UI/src/MaterialLibrary.cpp) 已限制单 profile 4MiB、pack 1–32 个/32MiB，检查 hash/大小/目录越界/符号链接，调用 native parser，验证来源与许可声明，命名空间化 stock、不可变版本保存、registry 原子发布、QLockFile 保护。它不接受任意 ZIP/插件代码，也不自动转换全局光谱表。已具备社区文件交换底座，不应重写。

F [EditStore.cpp](../windows_UI/src/EditStore.cpp) v8 锁定 film/paper SHA256 和全局资源联合指纹；旧版缺指纹，不能保证重现最初数据。材料库的 `resourceIdentity` 只覆盖 constants/filter/LUT 等指定文件，不等于 shader、二进制和全部运行上下文哈希。故目前“数据版本匹配”不等于“所有软件版本像素完全复现”。

P 分三个层次扩展：

1. **本地共享先行**：Recipe pack 引用材料哈希，按许可显式选择是否附带材料；分别保留作者、来源、许可与修改说明。哈希证明内容身份，不证明作者或许可真实性。
2. **版本恢复**：增加 evaluator/引擎构建/全局资源归档与兼容性清单；更新并行安装旧资源，不覆盖旧版；按引用计数显示可清理内容，正在被编辑引用的版本不自动删除。材料版本与配方版本分开升级。
3. **可选社区目录**：后续新增 `CommunityCatalogClient`，仅下载声明式 JSON/素材；签名清单、大小上限、撤销列表、离线缓存、明确下载动作、许可证可读。签名是来源信任，不是光谱准确性认证；不执行网络材料中的脚本或 DLL。

P 将 `schema checked / measured / reconstructed / assumed / alias` 等质量标签与测量条件、仪器、参考扫描和误差分开；[material_fit/README.md](../windows_UI/material_fit/README.md) 与 [material_audit/REPORT](../windows_UI/material_audit/REPORT_2026-10-30.md) 已明确仪器密度拟合不等于乳剂层数据。保持 `profile_ready=false` 的阻断，不能为扩库把拟合报告直接包装成完整实测胶片。新材料的颗粒/halation 默认值与专用标定要单列。

## 7. 四个明确实现入口

| 需求 | 当前入口 | 下一步具体改动（P） | 验收 |
|---|---|---|---|
| 原图对比 | [FrameCanvas](../windows_UI/src/FrameCanvas.cpp)、[originalFrame](../windows_UI/src/FrameOutput.cpp)、[originalImage](../windows_UI/src/PreviewController.cpp) | 对比模式标明“解码原图/渲染结果”；后台缓存原图；支持保存 A/B 编辑快照，所有表面共享 source/geometry revision | 裁切+90°+翻转+拉直+片边+双拼对齐；连切照片不显示旧原图；拖分割线不触发引擎 |
| 直方图 | [displayHistogram](../windows_UI/src/FrameOutput.cpp)、[Main.qml](../windows_UI/qml/Main.qml) | 保留 sRGB 显示直方图，缓存统计并放后台；可增加独立工作空间或导出预览读数，并标注空间与抽样策略 | 当前可见输出的像素总数/抽样总数正确；不统计裁掉区域；不能用显示直方图冒充场景纬度 |
| 配方预览 | [RenderSettings](../engine/src/desktop/preview_host.hpp)、[EditStore](../windows_UI/src/EditStore.hpp)、[Main.qml](../windows_UI/qml/Main.qml) | 新 Recipe/RecipeStore/RecipePreviewService、`RecipeBrowser.qml`；预览不写记录，确认一次事务 | 取消不留 dirty/undo；缺材料拒绝；快速滚动不积压 full 渲染；同图不同配方可比较 |
| 后期调色 | [setPostAdjustment/setCurve](../windows_UI/src/PreviewController.cpp)、[applyDevelop](../windows_UI/src/FrameOutput.cpp)、[ToneCurve.qml](../windows_UI/qml/ToneCurve.qml) | 先加异步后期与帧键；再独立 HSL；验证 CPU 参考后才加 GPU 版本 | 中性恒等、旧 v2 legacy sRGB 保持解释、曲线接缝/有限值、预览与同尺寸导出一致；单次拖动只入一次历史 |

## 8. 可直接移植与需新增架构的区分

P 可直接复用：共享 C++ 光谱 setup、物理参数语义、输出变换、曲线数学、同一线性输入的 parity 案例、macOS 导出配方字段与快照/表面一致性规则。入口分别见 [core](../engine/src/core)、[params.hpp](../engine/src/core/params.hpp)、[colour_transform.cpp](../engine/host/colour_transform.cpp)、[Adjustments.swift](../modern_UI/Spektrafilm/Spektrafilm/Model/Adjustments.swift)、[ExportRecipe.swift](../modern_UI/Spektrafilm/Spektrafilm/Export/ExportRecipe.swift)。

P 需平台适配而非复制：Core Image→LibRaw/WCS、Metal→Vulkan shader/Qt 场景图、ColorSync→Windows 显示契约、ImageIO→现有 Windows writer、Swift actor→C++ worker。这些都要验证实际像素与资源生命周期。

P 需新增架构：完整配方 schema/evaluator、配方缩略图任务服务、全局任务优先级和显存接纳、运行上下文版本恢复、社区索引信任。可延后：Vulkan 与 Qt 原生纹理互操作和异步 dispatch；先证实 CPU 回读/上传为主要瓶颈，避免提前引入跨 API fence/设备/布局同步复杂度。

所有涉及 wire 的真实新增字段，实施时须同时更新 [API-SPEC](../API-SPEC-callable-render-service.md)、schema/Params 与测试；本次方案不修改 wire、测量数组或物理常量。

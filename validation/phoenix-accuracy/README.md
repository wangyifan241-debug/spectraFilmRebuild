# Phoenix 精度验证基础设施 v1

冻结父版本：Draft PR #4，`ba076b2befe6586ad3095f897ebf40ea7e9ffd6c`。
本目录不修改材料、引擎、Windows UI、输出链路或已有图片。差距分析先于实现提交，见
[GAP_ANALYSIS.md](GAP_ANALYSIS.md)。当前真实参考样本数为 **0**，摄影还原度为 **Not Verified**。

## 执行与证据

仅需 Node 20+ 和含父提交的 Git 历史，无 npm 包、Python、联网或应用运行时依赖。
从仓库根目录执行：

```sh
node --test tools/phoenix-validation/tests.cjs
node tools/phoenix-validation/cli.cjs verify
node tools/phoenix-validation/cli.cjs split
node tools/phoenix-validation/cli.cjs report --run review-001
node tools/phoenix-validation/cli.cjs report --run photo-001 --gate photographic
```

退出码：0 = 所选门槛 Pass；1 = Fail / 输入无效；3 = Not Verified。
报告默认写入 `build/phoenix-validation-reports/<run>/`，已有目录禁止覆盖。
HTML 可离线打开，JSON 有独立格式与版本号。报告展示固定输入图片、字节一致性、
编码 RGB 差异及配对样本误差；固定输入差异始终标注 engineering-only。

`baseline-lock.v1.json` 从父提交 Git 对象生成，冻结两份材料、整个既有 baked
资源库、Phoenix 研究数据/图表/脚本/来源记录、印相滤色表、四张 PNG 与历史测试日志。
外部来源 URL 和已记录哈希同时保留；这不表示未随仓库提供的原始 PDF 已被重新取得。
锁中 SHA256 为 Git blob 字节。Windows 文本 CRLF 与 LF 的唯一差别可单独识别并列出
工作区字节哈希；没有 JSON 重排、浮点舍入、缺失数据填补等归一化。

`fixed-recipe.v1.json` 的显式控制值来自父版本 `phoenix_material.cpp`；它是重建记录，
**不是**历史完整参数转储。历史 PNG 以原始哈希为准，印相表不重新求解。

工程门槛复用 `verify-phoenix.cjs` 和父版本已发布的七项 native 测试凭据。
报告明确将后者标为 historical，不声称本轮重跑了整个 Windows/macOS 测试套件。
候选模型须提供独立的、经过审阅的 native 凭据，不能仅凭一张渲染图片取得工程 Pass。
真实胶片门槛另需完整物理来源、独立配对观测、每款至少三个独立分组，以及灰阶、颗粒、
光晕证据。`acceptance.v1.json` 的阈值目前是 **未审阅的项目建议**，因此不会授予摄影 Pass。

## 独立样本清单与防泄漏

`references.v1.json` 是唯一生效的参考清单，当前为空。`reference.example.json`
是未激活的条件模板，所有未知条件保留 null，不应填入猜测来解锁指标。
每项记录 film/product/format、源网址/作者/许可、negative/roll/scene/raw_capture
身份、曝光、冲洗、扫描设备与处理、ICC 变换、配对审阅人/方法及证据文件。
`files` 的 raw/scan/icc/reference_observations/pairing_evidence 均使用
`{ "path": "相对于 data_root 的路径", "sha256": "64位小写摘要" }`。
RAW 与扫描的配对是外部审阅事实，工具不能证明一张下载的 JPEG 属于某张底片。

按 negative、整卷 roll、scene、RAW 身份和 RAW/扫描内容哈希做**传递闭包分组**。
同组共享 split；冲突的 train/validation/test 声明直接拒绝。缺省按分组哈希分为
70/15/15；真实数据入库后应将审阅过的分组 split 明确写回清单，冻结清单与报告。
增加扫描或发现关联可能合并分组，须发布新 dataset_version、检查全部 assignments，
不得悄悄沿用旧分数。未知身份不能以每扫描一个新 ID 冒充独立底片。

训练集用于假设/拟合，validation 用于选择，test 仅能通过
`--split test --purpose final_evaluation` 查看，报告不会将 test 标记为可选模型依据。
这是一项工作流约束，不是对反复人工读取 test 的技术保密机制。

## 配对观测与颜色指标

观测包：`format: SpektraLab.PhoenixObservations`、`version: 1`、sample_id、
origin_kind（physical_film/model_render/synthetic_engineering）、源图片 SHA256、
extraction_method，colorimetry 必须是 CIELAB / D50 / 2deg / relative_y_scale=1，
并附 ICC SHA256。patches 每项含唯一 id、kind(gray/color)、Lab 三元组、相对 Y；
gray 另含 exposure_ev。两包须具有完全相同的 ROI ID、类型和曝光坐标。
模型输出还必须绑定 stock、material_sha256、RAW 输入、recipe、renderer 和图片哈希。
未配对、未知处理条件、缺失文件/ICC、错误代际、合成观测均返回 Not Verified / metrics:null。

ΔE00 使用 kL=kC=kH=1，报告 mean/p95/max；相对 Y 报告 MAE/RMSE；至少三点灰阶
报告 L* RMSE、每 EV 的斜率误差与逐点曲线。没有自动白平衡、直方图匹配或曝光归一化。
扫描色差是在申明扫描/显影/ICC 条件下的端到端差异，不能单独归因于胶片乳剂。

```sh
node tools/phoenix-validation/cli.cjs extract --input scan.png --spec scan-rois.json --out observations.json
```

可选提取器只支持非交错 PNG8/16、无透明、**人工审阅为标准 sRGB 的嵌入 ICC**。
spec 需 sample_id/origin_kind/color_model=sRGB_IEC61966_2_1、icc_sha256、
icc_interpretation_reviewed_by，以及 regions:[{id,kind,exposure_ev,rect:[x,y,w,h]}]。
先解 sRGB 传递函数，再逐像素转线性 XYZ、求 ROI 均值、转 D50 Lab。
提取器使用明确固定的 Bradford D50 矩阵与白点 [0.96422,1,0.82521]；
这是统一的分析约定，不是任意 ICC 的 CMM。导入的独立观测须使用同一约定，
在 scan.icc_transform / observation_reviewed_by 中记录实际转换与审阅。
非 sRGB、RAW、TIFF、未配准扫描须先由可审计外部工具处理并记录转换，不能改扩展名或假定 sRGB。

## 高分辨率扫描统计接口

观测包可含 spatial.grain / spatial.halation：width/height 至少 32、最多 1M 像素，
pixel_pitch_um (0,10]、source_bit_depth>=16、basis=linear_relative_luminance、samples
数组，以及 processing:{denoise:false,sharpen:false,tone_curve:"linear"}。
PNG 提取 spec 的 spatial 同名项用 rect 代替 samples/width/height。
颗粒输出去平面趋势的 Y RMS、偏度、x 方向 lag1；光晕需 center_px、core_radius_px、
outer_radius_px、独立测量 background_Y，输出径向轮廓及正背景扣除的 halo/core 能量。
截断光晕、分辨率不足、处理不受控或像素尺度不匹配均为 Not Verified。
这些是**扫描 ROI 统计**，包含扫描仪/镜头/锐化历史影响，不是标准密度计 RMS，也不是孤立胶片 PSF。
颜色固定输入配方关闭颗粒/光晕；空间统计必须另建开启效果、空间标定一致的冻结配方，
父/候选使用同一新配方，不能拿关闭效果的历史 PNG 验证真实颗粒。

## 一次一个假设的候选流程

1. 保留 v1 字节；复制材料到新路径、新 stock ID 和更高语义版本，添加
   metadata.parent_material_sha256、完整 license/datasource/source_sha256 与不确定性说明。
2. 复制 `candidate.example.json`，指定一类 hypothesis：spectral_response、exposure_response、
   dye_density、base_density、dir_couplers、grain 或 halation。工具比较物理字段，
   忽略版本/出处/显示名，拒绝未知字段及跨类修改。它不替代原有 native 材料校验器。
3. `node tools/phoenix-validation/cli.cjs candidate --plan plan.json` 审查差异。
4. 可加 `--render-out build/候选唯一目录`。指定**现有渲染器/适配器**的直接可执行文件、
   SHA256 与 argv，占位符为 {resources,input,output,stock,params,recipe}。
   工具不另写渲染引擎；渲染器须读取给定资源、参数与输入，输出无透明 PNG。
   固定 exe/recipe/resources/input 后，在私有资源副本中添加候选，复制父版本印相包到新 ID，
   同时运行旧/新材料，自动产生 render-run.json、comparison.json、HTML 及并列图片。
   输出与计划都在本仓库内时，还自动生成 models.json 供配对报告直接使用。
   解释器加载的脚本等静态 argv 文件须另列在 renderer.assets 中并绑定哈希。
   原材料/印相表不写回，输出目录不覆盖。不提供渲染器时仅校验计划，不声称已经渲染候选。
5. 扩展新的 models JSON（不要覆盖 models.v1）：首项仍为 frozen experimental-v1，
   之后的候选项带 candidate_plan 文件绑定、材料绑定含 parent_stock、新渲染绑定及独立
   engineering_receipt。所有模型 renderer_commit/recipe_sha256 必须相同。
   实拍 sample.model_outputs 以 model ID 索引，记录输出和观测绑定。
6. 用 `report --models 新索引 --references 新清单 --run 新版本` 自动比较同样本的
   ΔE00/亮度误差差值，并输出固定输入图片。缺少配对参考则不计算摄影排名。

native 凭据格式为 SpektraLab.PhoenixEngineeringReceipt/version1，含 reviewed_by、
renderer_commit、recipe_sha256、material_sha256 数组；tests 每项记录原有测试名、
完整 command、exit_code:0、status:Pass、log 文件绑定。所需七项见 receipts.cjs。
凭据是可审阅执行记录，哈希不会证明实验条件或人类声明真实。

## 数学来源

ΔE00 的独立实现及边界测试依据
[Sharma、Wu、Dalal 的实现说明与补充数值](https://hajim.rochester.edu/ece/sites/gsharma/ciede2000/)。
颜色约定参照 [ICC sRGB 说明](https://registry.color.org/rgb-registry/srgb) 的
传递函数与 D50 色适应原则；不实现任意配置文件、黑点补偿或外观模型。
新增测试中的合成观测仅测试数学与拒绝逻辑，不属于本项目的真实参考数据。

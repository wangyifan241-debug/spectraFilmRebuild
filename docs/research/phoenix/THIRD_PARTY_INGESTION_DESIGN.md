# 第三方渲染结果接入设计 v1（待实现）

日期：2026-11-02。只设计、不执行导入、不实现模块；不修改第二轮 PR、PR4 材料或 PR6 基线。研究正文 THIRD_PARTY_BENCHMARK.md 按用户已保存文件纳入；其中历史范围声明仅描述该报告原始写作过程，不限制本次已授权的 Git 提交。产品支持与许可结论须在实际运行时重新核实。

## 1. 数据流与统一输入

Sony ARW → 固定版本解码器 → 线性 RGB 主文件 → 带 ICC 的 16-bit RGB TIFF → 授权第三方软件人工渲染 → 原始输出 TIFF → 身份/参数审阅 → 显式 ICC 转换 → ROI 观测 → 独立第三方一致性报告。

固定 ARW 字节 SHA256、字节数、相机/镜头/方向、黑白电平、decoder 名称/版本/二进制 SHA256、解码参数文件 SHA256。白平衡采用一次确定的 as-shot 或灰卡系数并记录数值；所有软件共用该解码结果。禁止各软件分别开发 RAW 后声称输入完全一致。

主文件建议 float32 线性 ProPhoto RGB/D50；交付输入为三通道 uint16 TIFF、linear transfer、嵌入真实线性 ProPhoto ICC，记录 ICC 字节 SHA256。不能给线性像素贴普通 gamma=1.8 ProPhoto 标签。量化规则 round(clamp(x,0,1)*65535)，报告负数、>1 和截断比例；量化丢失的 HDR 值不能在后续恢复。必要时另建共同且预登记曝光缩放的 HDR 兼容实验，不能逐软件自动压高光。

如果某软件不接受线性 ProPhoto TIFF：由同一主文件通过固定 CMM 转成已确认支持的 tagged sRGB/Adobe RGB 16-bit TIFF；记录 CMM 版本、输入/输出 ICC、rendering intent、BPC、传递函数、gamut/clipping 与转换后像素哈希。SpektraLab 同时读取该相同兼容文件。该实验单独命名，不与线性 ProPhoto 输入组合排名。禁止仅修改 ICC 标签；未知 ICC 或默认解释未知时不可比。

测试集包含同一 RAW 的合法照片，以及独立灰阶、明确来源的24色块、肤色、天空、植物和夜景高光。缺少某类素材则 coverage=missing，不以近似 RGB 色块冒充反射谱或真实胶片。曝光序列在模拟前的线性输入统一施加 2^EV，EV=-6..+6、步长1；关闭第三方自动曝光与独立 tone matching，记录 TIFF 截断。每个 EV 是独立输入绑定。

## 2. Manifest 草案（非生效文件）

下例 null 均表示缺失，不是已完成记录。摘要必须是文件实际字节的64位小写 SHA256；示例不会通过可比性检查。JSON schema 后续单独实现，目前仅在本文中定义。

```json
{
  "format": "SpektraLab.PhoenixThirdPartyBenchmark",
  "version": 1,
  "dataset_id": "phoenix-third-party-pilot",
  "dataset_version": 1,
  "classification": "third_party_simulation_not_real_film",
  "runs": [{
    "id": "dxo-phoenixI-ev0-001",
    "film_generation": "phoenix_200",
    "fixture_id": "sony-fixed-ev0",
    "exposure_ev": 0,
    "raw": {"path": null, "sha256": null, "bytes": null},
    "decode_record": {"path": null, "sha256": null},
    "input_tiff": {"path": null, "sha256": null, "bytes": null},
    "software": {"vendor": "DxO", "name": "FilmPack", "version": null, "build": null, "platform": null, "binary_sha256": null},
    "preset": {"display_name": "Harman Phoenix 200", "generation_review": "unreviewed", "revision": null},
    "settings_record": {"path": null, "sha256": null},
    "output_tiff": {"path": null, "sha256": null, "bytes": null},
    "input_encoding": {"space": null, "icc_sha256": null, "transfer": null, "bit_depth": 16, "sample_format": "uint", "channels": 3},
    "output_encoding": {"space": null, "icc_sha256": null, "transfer": null, "bit_depth": null, "sample_format": null, "channels": null},
    "white_balance": {"mode": null, "rgb_multipliers": null, "temperature_K": null, "tint": null},
    "effects": {"grain": null, "halation": null, "bloom": null, "sharpening": null, "denoise": null, "vignette": null, "auto_exposure": null, "auto_contrast": null},
    "conversion_record": {"path": null, "sha256": null},
    "registration_record": {"path": null, "sha256": null},
    "rights": {"status": "not_verified", "use": "private_comparison", "eula_version": null, "evidence": null, "redistribution_allowed": false},
    "review": {"operator": null, "reviewer": null, "reviewed_at": null, "status": "unreviewed"}
  }]
}
```

生成型号枚举 phoenix_200 / phoenix_ii；不按相似商品名推定代际。提供者枚举 DxO、Ashigrain、The Grain Archive；后者还需 host 名称/版本、style显示名/修订与授权凭据，不能只记录预设作者。设置文件记录曝光/对比/曲线/输出 resize/裁切、所有效果的开关和数值及扫描/印相模式。未知值必须 null，不能按关闭处理。

配对记录引用 run_id 左右各一项，绑定相同 fixture_id、input TIFF SHA256、EV、代际、recipe比较类别。SpektraLab 一侧另绑定材料 ID/版本/SHA256、renderer SHA256、source commit 和完整 resolved 参数。批次/运行时间/操作者另记录；文件改动一字节即新身份，不接受文件名或时间戳代替哈希。

## 3. 人工 TIFF 导入与可比性门槛

操作者通过授权软件手工导出 TIFF；不读取程序安装目录、缓存数据库或商业 LUT。未来导入器只接收人工指定的输出和侧车记录，校验路径在 data_root 内、拒绝 symlink逃逸/重复ID/过大或损坏文件，读文件计算哈希并核对 manifest。源 TIFF 保持只读；派生转换输出新文件、新 SHA256，保留完整转换链。

检查 TIFF 实际尺寸、orientation、通道、位深、sample format、ICC和 alpha；不能信扩展名或侧车声明。缺失/冲突 ICC、未知 transfer、代际不明、未经许可、输入哈希不同、自动曝光未知或 geometry/ROI 不对应：status=Not Verified，metrics=null，列出 reasons。明确不同 gamut/模式但已记录的转换可标 limited_comparability，不进入严格相同条件聚合；不得自动校正曝光、WB、饱和度或直方图来通过门槛。

分别建 color_only 与 spatial_full_look 两种合同。前者附加颗粒、光晕、bloom、锐化、降噪关闭并经人工确认；关不掉则记录残留，不能冒充隔离颜色实验。后者固定像素采样/输出尺寸、有效胶片格式和ROI，比较随机效果必须重复渲染至少5次，报告均值/方差而非逐像素单次“误差”。水印不移除；受影响 ROI 排除并降级覆盖，无法取得干净合法输出就不执行该比较。

## 4. PR6 复用与严格隔离

已审阅 PR6 00405ee8ad5a448b09e64b1bf9195f1bb429f9a9 的 observations.cjs、references.cjs、metrics.cjs。现有 PNG 提取器只接受经审阅嵌入 ICC 的标准 sRGB PNG8/16，不原生接受任意 TIFF/ICC；physical manifest 只有 physical/synthetic_engineering 分类，不能把第三方输出塞进其中，也不能伪造真实 RAW/扫描配对证据。

未来外部、可审计 TIFF/ICC 适配器先读三通道16位数据并转换为 D50 XYZ（保留负值/越界QC），ROI 在线性 XYZ 求均值后调用现有 xyzToLab。可选落地标准sRGB PNG16再使用现有提取器，但必须报告gamut clipping并保留源TIFF；不得将无嵌入ICC的PNG视为合格。

复用 deltaE00 算法（kL=kC=kH=1）、Lab/灰阶/相对Y和斜率统计；pairedMetrics 的输入约束由独立适配层验证，不调用 photographic evaluateSample。候选模型的材料身份/配方/固定输入匹配逻辑可复用为前置检查，第三方不伪装成 PhoenixCandidate 新材料。

独立报告 format=SpektraLab.PhoenixThirdPartyReport/version=1，字段 software_consistency、comparability、coverage、rights_review、metrics；physical_fidelity 固定 status=Not Verified/metrics=null，除非未来独立真实胶片流程提供证据，且不能由本模块更改。没有输出时整体 status=Not Verified、metrics=null、reasons=[missing_authorized_outputs]；执行状态可另为 Not Run。三种 pair_kind：third_party_vs_third_party、spektralab_vs_third_party、physical_film_validation；最后一项禁止本适配器生成。第三方分数不得进入真实胶片排名、门槛或训练集。

色块逐块输出RGB、D50 Lab、Y与模型间ΔE00（mean/p95/max）；灰阶L*、ΔY、每EV斜率；肤色/绿/蓝输出C*=sqrt(a²+b²)、h=atan2(b,a)、环形Δh，低C不报告不稳定色相。全图对齐后RGB MAE/RMSE、差异图并显示clipping比例；不把SSIM或相互接近当作真实性。曝光稳定性按相同patch跨EV逐项报告。空间ROI复用去平面Y RMS/偏度/lag1和背景扣除halo/core能量；PSD需新增明确方法，不能宣称现有模块已有。光晕输入高光clip、ROI截断或有效像素间距不同则不比较。

## 5. 后续实现文件清单（仅建议，本次不创建）

| 未来文件 | 用途与必要验证 |
|---|---|
| tools/phoenix-third-party/manifest.cjs | 新格式、路径/哈希/身份/许可校验；缺失和冲突拒绝测试 |
| tools/phoenix-third-party/tiff-adapter.cjs | TIFF实际位深、ICC/CMM与转换链；独立像素/ICC测试向量 |
| tools/phoenix-third-party/comparability.cjs | 相同输入、EV、WB、generation、效果、空间采样合同；未知不默认通过 |
| tools/phoenix-third-party/observations.cjs | 独立ROI观测适配，复用PR6数学；禁止physical来源伪装 |
| tools/phoenix-third-party/report.cjs | 独立JSON/HTML、三类差异标签、缺数据metrics=null |
| tools/phoenix-third-party/cli.cjs | 手工导入/校验/报告命令，不调用商业程序或提取模型 |
| tools/phoenix-third-party/tests.cjs | 已知色差向量、16位精度、错误ICC、缺授权、代际混淆、水印、隔离photographic gate |
| validation/phoenix-third-party/*.schema.json | fixture/run/pair/report版本化schema，未知字段策略与迁移 |
| validation/phoenix-third-party/examples/*.json | 明确未激活、null值模板，无假输出/假SHA |
| validation/phoenix-third-party/README.md | 本地data_root、合法导出操作、CMM固定和审阅清单 |

数据文件默认位于仓库外的人工选定 data_root；公共CI仅用自制合成测试向量，不上传商业素材/用户RAW或预设。输出、截图、许可合同能否公开分开审阅；私有比较授权不等于公开报告图片授权。没有获得许可时仅公开方法与缺失状态。

## 6. 当前状态和后续顺序

当前授权输出数量0；无软件安装/试用/购买，无第三方指标。报告现有产品信息不是已运行证据；原报告“Phoenix II跨所有host”等范围表述也需按实际版本逐host复核，不能作为导入器的支持保证。

先完成第二轮独立审查，再固定输入与rights记录；确认DxO一代、Grain Archive二代的合法软件/host与输出控制；做一张相同输入的色彩管理pilot；随后24色块/灰阶与EV序列；最后空间效果重复试验。Ashigrain需要可用Mac/host和授权输出，水印试用不能靠去水印处理。每阶段先审阅条件再执行，第三方一致性无论多高都不代替未来第四轮真实胶片配对验证。不因第三方风格建议改第二轮材料。

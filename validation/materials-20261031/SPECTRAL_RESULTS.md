# 扫描数据光谱分离：实现与验证

2026-10-31，Windows；基于 `86f337c` 继续。此阶段交付离线工具，不是 Pro Image 100 可用 Profile，也不是全部材料标定完成。

## 已实现

- `windows_UI/material_fit/SpectralLift.*`：逐波长 Beer–Lambert 前向积分、稳定 log-sum-exp、解析 Jacobian、多起点有界层量反演和局部条件数。
- CLI `spk_material_fit --spectral`：显式曲线、染料、仪器响应、感光度四输入；有限/尺寸/网格/单位/支持校验；新文件专用输出；每份输入来源与 SHA256；失败仍写诊断，退出 3。
- 保留原始负代理染料样本、所有曝光点和 null 感光度。没有自动填尾部、裁负数、改曝光轴、改基底或强制注册材料。
- 默认一维 CDF CLI 的 v1 报告逐字节对照旧存档；新模式不改变旧报告。
- 新回归包含独立 long-double 透射 oracle、64 组已知层量、有限差分 Jacobian、秩亏/边界、仪器增益不变性、负样本、积分面积、感光度缺测、错误输入和 CLI 拒绝覆盖。

没有修改 engine 核心算法、Vulkan kernel、C ABI、Qt 编辑界面或既有 Profile。与上一版冻结清单比较，运行时 **37/37 份 Profile SHA256 相同**，`git diff HEAD -- engine` 为空。没有声称对所有胶片做跨旧二进制逐像素穷举证明。

## 输入来源与权限边界

胶片三份输入为 OwlMightyCh / Film Scan Calibration，固定 commit `8d03a0e0c111625b8a88fb8647a35d95b491249d`，数字化/派生数据声明 CC BY 4.0；完整归属、原始字节、哈希和不确定性在 `engine/resources_research/scan/NOTICE.md`。原厂数据表权利与代理模型的不确定性保留。

外部仪器响应来自同一固定快照的 `data/standards/StatusM_ISO5-3.json`，SHA256：

`b296756ebe973d523de62ba2c5d2801f1ecb7f2394d95cf53961746d504c2a32`

该文件自述来源为 ArgyllCMS/ISO 5-3:1995。本工具不认证它为标准、不重新授权；**没有把该表、标准正文或其原始权重复制到 Git/应用包**，也没有宣称它受胶片数字化文件的 CC BY 4.0 覆盖。它只是调用者显式提供、哈希固定的本地研究输入。回归的合成响应为本项目自有解析函数，明确不是 ISO 表。

## 真实数据结果：未过门

积分为 400–700 nm、1 nm 步长、包含端点的均匀求和；仪器响应在已供支持内线性插值。被排除的红响应权重为 **0.4875082906%**，绿/蓝为零。这与上游引擎 1 nm 口径一致；10 nm 原表直接求和约 0.28%，不是同一个采样口径。

| 项目 | R | G | B |
|---|---:|---:|---:|
| 模型基底仪器密度 D | 0.27350494 | 0.67705083 | 0.91990375 |
| 全部 201 点 RMSE D | 0.00710643 | 0.000458719 | 0.0000407109 |
| 最大绝对重建误差 D | **0.02874505** | 0.00184634 | 0.000164917 |
| 目标低于模型基底的点数 | 25 | 0 | 0 |

门为每行最大绝对误差 ≤0.02 D、局部 Jacobian 条件数 ≤10000。**12/201 行失败，0 行局部秩亏**。失败曝光为 −3.14 至 −2.92 log lux-seconds，cyan 层量均命中零下界。它说明当前非负层量、代理染料/基底和数字化目标无法由本次有界反演完全协调；不能以删除暗部、放宽误差或允许负层量宣称材料已完成。

感光度支持：cyan 531–660 nm（130 有效点；引擎 81 点网格上 26 点）、magenta 441–590 nm（150 / 30）、yellow 370–510 nm（141 / 27）。缺测尾部保留未知；`exposure_anchor_calibrated=false`。代理染料 magenta 含 51 个负样本，原样参与前向积分。

`mathematical_fit_passed=false`、`profile_ready=false`，报告没有 Profile 的 info/data，材料管理器拒绝导入。这是**数据质量门失败，非程序崩溃或忽略断言**。即使未来数值门通过，也仍需处理完整光谱支持、真实染料/基底不确定性、绝对曝光锚点和独立实拍验证。

## 实际测试与构建证据

Release：Qt 6.11.3 / MinGW-w64 13.1.0，完整 **30/30 CTest 通过，188.50 秒，退出 0**。无缺夹具跳过；实际 Vulkan 设备 NVIDIA GeForce RTX 3050 Laptop GPU。耗时仅为本次运行观察，不是性能基准。

窄门 `spk_spectral_lift` 为 511 项、`spk_scan_curve_fit` 为 125 项，2/2 通过。先以 `--linear-counterfactual` 故意使用初始线性 Jacobian 矩阵替代非线性层量恢复，实际退出 1，错误为 `Known CMY recovery failed (linear matrix is not spectral inversion)`，恢复非线性实现后转绿。完整门还包括 191 字段 QML 工作流、9368 项开放材料、100 项材料库、37 项材料工作流、393 项编辑记录、边框/日期/半格及 Vulkan 系列测试。

程序在 `D:/学习资料/SpektraLab/build/qt-20261031-spectral/app/`：GUI `SpektraLabQt.exe`，离线 CLI `spk_material_fit.exe`，保留整个 app 目录的 DLL/资源。GUI 仍使用原有 37 份运行时材料；本阶段没有新增 UI 控件或可选 Pro Image 材料。

| 身份 | SHA256 |
|---|---|
| GUI EXE | de9b804646bec213551f7ee84b0604674617eb6fc5b61a4b5a9ac04ea94c5df7 |
| 拟合 CLI EXE | 4207e7f8af31504b88b7eddc9ba93c4101e390c48c6563312b5ff5cb752312c2 |
| 冻结源码/资源输入清单摘要 | eb0c44935d70448e7af603177a1facfcad80efd456a3b20fe5cef75883ec028e |
| 完整 CTest 日志 | ab18ec68bc65ad51c4aaf387d351b5163ca3b059e1320bc1eb48f6d81b5478d5 |
| 光谱真实数据报告 | 59cc7095c750463b75e73e08a6c99edf6aa486e969aeb13c8b2bf326e2a70b93 |
| 旧一维报告（字节未变） | 79da47609407b5f0c569855d2e067999ba25689d2d3e5dc7057821aa1c3fb0db |

本目录保存 `spectral-build.json`、`spectral-ctest.txt`、`spectral-narrow-tests.txt`、`spectral-counterfactual-red.txt`、`proimage100-spectral-lift.json`、`spectral-diagnostic-verification.json`。真实数据连续两次生成报告哈希一致，均退出 3。构建目录另有 `qt-build.json`、`build-source-files.json`，构建后再算输入清单仍相等；完整构建控制台在 `build/spectral-release-final.log`。事后验证目录不属于既有编译输入清单，没有修改 inventory 规则。

真实数据复核单独运行，不把第三方标准表塞进 CTest：

```powershell
& '<repo>/validation/materials-20261031/verify-spectral.ps1' `
  -BuildDirectory '<repo>/build/qt-20261031-spectral' `
  -Responsivity '<自行取得的固定 SHA256 外部仪器响应 JSON>'
```

脚本为每次运行建立新的结果目录，不覆盖文件。实际运行已通过：检查输入固定哈希、CLI 返回 3、201 行/12 个拒绝、完整报告哈希及旧一维报告哈希。`diagnostic_regression_passed=true` **仅表示正确复现拒绝行为**，`research_quality_gate_passed=false`，并非把材料质量失败改记成功。

开发期间首次窄门编译因一次 `auto` 声明混合两种类型失败，拆开声明修复；失败记录在 `build/spectral-build-1.log`，随后窄门及新的完整 Release 已重跑，不采用失败的构建。

本次仅离线工具变更，不新增人工鼠标/显示器校准或 macOS Metal parity 声明。前一阶段真实 Windows/NVIDIA/ARW 窗口证据保留在 `2302_SCAN_RESULTS.md`，不把旧截图当作本阶段新窗口验收。

## 后续完成条件

1. 取得相互一致的暗部特性曲线、基底与染料实测/授权数据，或由用户明确接受一个单独、带假设标签的研究模型；不能覆盖当前原始数据。
2. 独立明确感光度定义、照明与能量/曝光单位，建立可检验的绝对曝光锚点；不能把 R/G/B 仪器曲线直接当 CMY 本征层曲线。
3. 新材料以版本化独立 ID 进入材料库，完成 Vulkan、Qt 选择、保存重开、缺失拒绝及 TIFF/JPEG/PNG 回归后，才称可用 Profile。

工具使用及命令参数见 `windows_UI/material_fit/README.md`。已有 2302 印片功能不依赖这份研究报告，继续可用。

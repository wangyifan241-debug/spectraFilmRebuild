# 2302 / 扫描曲线拟合：实际交付结果

2026-10-31，北京时间。基于 `602ec32` 的续作；不是全部胶片光谱标定或全产品迁移完成声明。

## 程序

- GUI：`D:/学习资料/SpektraLab/build/qt-20261031-2302-scan/app/SpektraLabQt.exe`
- 离线工具：同目录 `spk_material_fit.exe`。Qt/MinGW DLL 已随 app 部署，不是运行时 Python；保留整个 app 目录。
- Release / Qt 6.11.3 / MinGW-w64 13.1.0。NVIDIA RTX 3050 Laptop GPU，4 GB。旧用户窗口未关闭；以下为应用自有测试驱动，非人工鼠标验收。耗时仅为本次观察，不是性能基准。

| 身份 | SHA256 |
|---|---|
| GUI EXE | 0952806ba7e5ec857f0b20ab8fdc7a3b39c320fe36f3fbc4d6d224eb1593e649 |
| 拟合 CLI EXE | 624d8bc98d17358d9ca08acc6fc3820225fba3f6596ad1731623730f031620f7 |
| 冻结源码/资源输入清单 | 21bde14c1850509f838ae0a4829d2695b55e275adb80e54267784c6a1d14f220 |
| CTest 完整日志 | fa7cfe6abd78dbc7ccc90b8d88f85d306ddbb584a26d7b625c5eae2cbba92a5e |
| 最终原生窗口报告 | f4928954670afadb93ba12b5f0fbf51c50a755a5c13057783975241d7943beaf |
| Pro Image 拟合报告 | 79da47609407b5f0c569855d2e067999ba25689d2d3e5dc7057821aa1c3fb0db |
| 新 2302 Profile | e2298d9d7ffe13f1ce216c813797c8801b97091735b3fa2324a0737e3c8b0952 |

构建证据：`build/qt-20261031-2302-scan/qt-build.json`、`build-source-files.json`、`build/2302-scan-release-final.log`。源码清单和 EXE 在构建后再次核对一致。此事后结果目录不属于既有编译输入清单，未修改 inventory 规则。

## 实际测试结果

| 门 | 结果 |
|---|---|
| 完整 Release / CTest | **29/29 通过，192.31 秒，退出 0**；无缺夹具跳过 |
| `spk_qt_open_material` | **9368 项**；7266、5222、2302 的转换守恒，实际 Vulkan、默认效果、确定性、旧 Portra 往返像素一致，TIFF/PNG/JPEG、保存重开，自定义 2302 导入/历史恢复，实际滤片参数及旧自定义银盐兼容门 |
| `spk_scan_curve_fit` | **124 项，1.76 秒**；已知合成 CDF、留出样本、固定 Pro Image 字节哈希、重复性、非 Profile 拒绝、CLI 署名/许可/固定 URL、拒绝覆盖、错误输入无半份输出 |
| 材料库核心 / 工作流 / 编辑记录 | **100 / 37 / 393 项**通过 |
| 完整 QML 工作流 | **191 个报告字段**，69.73 秒；实际控件、场景抓图、2302 选择和印片分类 |
| 原生 Windows + 真实 Sony ARW | **退出 0，passed=true，error 为空，191 个报告字段**；`platform=windows`，D3D11 场景图，DPR=1.25，实际 NVIDIA Vulkan；`qml_2302_real_vulkan_print` 与 `qml_2302_not_mislabeled_as_multigrade_paper` 为 true |
| 已有材料保护 | 审计原始 34 份 + 上轮 7266/5222 共 **36/36 个 Profile SHA256 不变**；未改核心物理、GPU 内核或 C ABI |

完整证据在本目录 `2302-scan-ctest.txt`、`2302-scan-native.json`、`proimage100-statusm-fit.json`。报告中的 `graph_post_mode=false` 为模式诊断，不是失败断言。新程序初始目录为 **27 款拍摄胶片 + 10 份印相材料**，不是全部叫“胶片”。

真实窗口截图保留在 `build/validation-20261031-2302-scan-native/material-2302.png`（SHA256 `2b1387c06d9c69b14e8abd0c5e22689502c9afcf5b6050979629914539f29aaf`），已目视检查最终画布；另有 `material-library.png`。原始 RAW 和截图不随 Git 再分发，未替换用户的 macOS README 截图。左栏标题本轮改为“印相材料 · 相纸 / 印片”。

## 2302 怎么用、是什么数据

先选黑白负片（例如 Double-X 5222），然后在材料库或“印相材料”选择 **Kodak Print Film 2302 (5 min, upstream 0.3.3)**。它是电影印片胶片，保留 `support=film/stage=printing`。新 ID `open_kodak_2302_5m_033`，固定源数据的 5 分钟条件，不擅自插值其它时间。

作者 Andrea Volpato，CC BY-SA 4.0；固定源 commit `6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507`，原始 SHA256 `3c04bb2e7701eae97eaaf14100cbf80c8f00dd07db3deda3f14b37ac060ca2be`，原样五条件与完整许可保留于 `engine/resources_open/`。新版本及其导入副本使用无 Multigrade 滤片；Ilford 及无关旧自定义材料的原有兼容设置保留。现有放大机照明/归一化仍在，不宣称模拟特定光学印片机或新增实测颗粒模型。

## 扫描拟合已经做到、尚未做到的事

工具对 Pro Image 100 的 Status M 仪器密度（含基底）完成可重复的三 CDF + offset 拟合，不平移测量曝光轴。每通道 201 点：161 训练、40 留出；留出 RMSE：

| 通道 | RMSE (D) | 最大绝对误差 (D) |
|---|---:|---:|
| R | 0.0008713 | 0.0016487 |
| G | 0.0005389 | 0.0009840 |
| B | 0.0009952 | 0.0018911 |

这是相关数字化点上的**一维拟合误差**，不是独立实拍误差。输入作者 OwlMightyCh，固定 commit `8d03a0e0c111625b8a88fb8647a35d95b491249d`，CC BY 4.0；原始曲线、感光度和代理染料及完整声明在 `engine/resources_research/scan/`。报告自身携带来源 URL、原始 SHA256、许可链接、作者和修改说明。

**仍未完成：** Status M 仪器密度到内禀染料层量的光谱前向分离、缺测光谱尾部与代理染料不确定性、感光度/曝光锚点标定、独立实拍颜色验证。因此报告固定 `profile_ready=false`，管理器拒绝当 Profile 导入；本轮没有增加一款伪称完整标定的 Pro Image 胶片。详见 `windows_UI/material_fit/README.md`。

此外未运行 macOS Metal parity、人工系统对话框/鼠标/显示器校准，也未交付对外签名安装器。现有胶片没有做跨旧二进制的穷举逐像素证明；原始字节保护与旧 Portra 往返、原有全套门已通过。

## 中间失败及提交边界

转换单元素数组错误、首次资源同步过早、最初参数断言误用嵌套格式均已记录在 `windows_UI/MATERIAL_LIBRARY_2302_SCAN.md`。修正为平铺 wire 后，刻意恢复旧的全黑白 Y=68 策略，回归如预期失败；恢复仅 2302 无滤片后转绿。最终大门不是靠删除断言通过。

首次新目录构建为补齐报告自身许可/署名而中止，只停止了已核实的自身 CMake 子树，未动用户程序；它不是成功构建。随后重新冻结，完整构建/测试成功，才采用上述 EXE/清单。

离线扫描拟合阶段独立提交 **`ff3f33e`**；本结果记录与 2302 运行时/Qt/回归由后续独立提交保存。没有自动推送或重写旧历史。

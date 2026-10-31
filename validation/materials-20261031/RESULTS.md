# Windows 材料库：最终验证记录（2026-10-31）

本记录补充 `windows_UI/MATERIAL_LIBRARY_IMPLEMENTATION.md`，将其中“最终门待运行”的状态更新为以下实际结果。仅本地开发 Release 交付，不是已签名安装器、许可全面验收或全部 Mac 功能迁移完成声明。

## 交付与身份

- 程序：`D:/学习资料/SpektraLab/build/qt-20261031-materials/app/SpektraLabQt.exe`；顶部“材料库”。保留整个 app 目录，不单独复制 EXE。
- Qt 6.11.3，MinGW-w64 13.1.0，Release；NVIDIA GeForce RTX 3050 Laptop GPU（4096 MiB），驱动 595.95。测试耗时为本机观察值，不是性能基准。
- EXE SHA256：`80a86f5025357620920c0ca2194f427a905ae17f4ded45deedb90816fb866544`。
- 输入清单 SHA256：`47cb6672a2de698c927ee2295ccaac9c057f6d79c4d9ecdfed4c8822f9ae9eae`；`build/qt-20261031-materials/build-source-files.json` 列出实际源码/资源哈希，`qt-build.json` 同时记录 EXE 和输入清单。
- 在最终构建前冻结源码，构建后校验输入清单与程序哈希一致。此事后验证记录位于 `validation/`，不属于现有 source-inventory 的编译输入范围，避免把结果记录反向当成二进制输入；未改变 inventory 规则。

## 实际通过的门

| 门 | 实际结果与范围 |
|---|---|
| Windows Release + 全部 CTest | **28/28，219.92 秒，退出 0**；不是挑选通过项或跳过夹具 |
| 新材料回归 | **6236 项**，17.85 秒；7266/5222 原始与转换数据守恒、条件/模型/子层、实际 Vulkan、默认效果、确定性、旧 Portra 前后像素完全一致、参数恢复、TIFF/JPEG/PNG |
| 材料管理核心 | **100 项**，6.29 秒；完整 Profile/pack 导入、不可变版本、来源/结构负门、错误索引、全局资源变动拒绝、历史存档、实际 Vulkan |
| Qt 材料工作流 | **37 项**，16.88 秒；自定义注册和保存、同名升级不重定向、禁用/移除后的历史恢复及 PNG 导出、v8 Profile/全局资源指纹、旧记录只读、缺失/损坏拒绝 |
| 编辑存储 | **393 项**，0.64 秒；含 v1–v8 兼容和未来格式拒绝 |
| 完整 QML 离屏工作流 | **185 个报告字段**，103.09 秒；真实控件、场景抓图、两款新片选择、自定义导入 handler、版本选择和 Vulkan |
| 最终原生 Windows 窗口 | **退出 0，passed=true，185 个报告字段**；`platform=windows`、D3D11 场景图、DPR=1.25、真实 Sony ARW、7266 直接扫描、5222 + Ilford 银盐印相、自定义版本选择 |
| 损坏索引启动 | 自有 `{invalid-owned-test-fixture` 索引，`--ui-smoke --edit-store`；受控退出 **1**，无截图，索引哈希不变；没有崩溃或覆盖 |
| 回归反证 | 独立资源副本故意不含 5222 Profile：新材料测试 **退出 1**（Cannot read material fixture）；补回同一文件后 **退出 0、6236 项通过**。未改真实程序资源 |
| 原有数据 | 对审计 `sources.json.local_resource_hashes` 的 **34/34** 个原有文件重新核 SHA256，**0 差异**；新增 2 份，最终 27 胶片 + 9 印相材料 |

全套 CTest 日志：`build/qt-20261031-materials/Testing/Temporary/LastTest.log`，SHA256 `c26589b5b54c39827fc8b0709e0517b034229d6e280b24e2e7b62e73aabce1f9`。

原生报告：`build/validation-20261031-materials-final/report.json`，SHA256 `11bebe181f17b48451b14ef5648152413cf28f1aa753df07b9c3b8955a007281`。`graph_post_mode=false` 是测量时的模式诊断，不是失败的断言；`passed=true` 且 error 为空。

真实窗口截图在同目录：`material-library.png`、`material-7266.png`、`material-custom.png`、`material-doublex.png`。已目视检查两款片的最终画布与材料面板，非设计图。RAW 仅作本机验证，不随源码或安装包再分发。先前失败窗口测试的原因和修复见实施记录；未把旧失败报告覆盖成成功。

## 新增数据及许可

| 引擎 ID | 条件/路径 | 来源与许可 |
|---|---|---|
| `open_kodak_trix_7266_033` | Tri-X Reversal 7266，单条件黑白反转片；非原有 400TX | Andrea Volpato / spektrafilm dev `6cd00c8d4f30b5b550f50f4bbd3753c9f2a48507`，CC BY-SA 4.0；原始 SHA256 `4576c0f34ed7e69dd99551b28560e08f597cca3d71385c1e35577edf4ae4a986` |
| `open_kodak_doublex_5222_65m_033` | Double-X 5222，明确选择原始五条件中的 6.5 分钟，负片/银盐相纸 | 同作者/固定 commit/许可；原始 SHA256 `37c333046554151ac5538e6eeb1f2cadef919876c63d0dd861c547952874fac3` |

原始 JSON、完整许可、归属、修改说明及两份转换数据保存在 `engine/resources_open/`，随程序复制到 `resources/open-materials/`；可复现转换器在 `windows_UI/material_audit/`。没有覆写已有材料，没有修改核心物理算法或 C ABI。颗粒/光晕仍用当前引擎默认，未宣称新增胶片独立实测拟合。

## 已知未完成 / 未验证

- **未接入** Kodak 2302 电影印片材料；需要正确配对/滤片与独立显影条件验证，不当作普通相纸。
- **未完成** film-scan-calibration 原始测量到前向模型的拟合，以及新的彩色胶片独立实测校准。
- **未实现** 内置材料历史跨安装自动重放；遇到材料或全局资源冲突明确拒绝并保留编辑。自定义历史版本存档已验证；“移除”是逻辑移除，不永久删除。
- **未验证** 外部鼠标操作、原生系统文件对话框、显示器校准/主观观感；电脑控制服务两次初始化失败，原生窗口门使用应用自有 QML/QEvent 测试驱动，不能冒称人工验收。
- **未运行** macOS Metal parity 或 Mac GUI 测试。本机的绿色 CTest 不等于跨平台逐像素完全一致证明，也不构成全部既有胶片的穷举数值基线；本轮旧 Portra 不变与原始 34 份字节不变已验证。
- **未交付** 对外发布的签名安装器。既有产品黑白数据授权边界、Qt/其它依赖源码及再分发 gate 仍按审计报告和 `windows_UI/THIRD_PARTY.md` 单独审核。

## 复跑命令

```powershell
& 'C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1' `
  -SourceDirectory 'C:/Users/Strickland/SpektraLab-work' `
  -BuildDirectory 'C:/Users/Strickland/SpektraLab-work/build/qt-20261031-materials' `
  -DependencyDirectory 'C:/Users/Strickland/SpektraLab-deps-20261028' -Jobs 6 -RunTests

# 选一个不存在的输出目录，勿覆盖上次证据。测试自动复制输入并隔离编辑目录。
$env:QT_QPA_PLATFORM='windows'
$env:QT_QUICK_BACKEND=$null
$env:QSG_RHI_BACKEND='d3d11'
& 'C:/Users/Strickland/SpektraLab-work/build/qt-20261031-materials/app/SpektraLabQt.exe' `
  --open '<真实 RAW 的绝对路径>' --reject '<损坏 RAW 测试夹具的绝对路径>' `
  --self-test '<新的绝对输出目录>'
```

提交按阶段保留：`b2575d4` 基线，`12b55b2` 审计，`2a284fd` 首款胶片，`7c3268f` 原字节保护，`e453b7a` 管理核心，`b3d90ea` Qt 与版本锁定，`5b11ce6` 第二款胶片；本记录与第四阶段修复由后续独立提交保存。不重写旧提交，不自动推送。

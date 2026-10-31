# Windows 材料库扩展：实际实现与验证

本任务从 2026-10-30 延续到 2026-10-31（北京时间）。审计见 `material_audit/REPORT_2026-10-30.md`；所有结论绑定其中的固定上游 commit，不自动跟随 main/dev。

## 已保存的起点

- `b2575d4`：用户明确批准保存现有 Windows 移植基线。
- `12b55b2`：完整材料审计、字段/内容哈希、来源/许可与扩展计划。

## 阶段 1：第一款真实开源材料

新增 `open_kodak_trix_7266_033`，Kodak Tri-X Reversal 7266；不是现有 Tri-X 400 负片。原作者 Andrea Volpato，CC BY-SA 4.0。固定原始文件 SHA256 `4576c0f34ed7e69dd99551b28560e08f597cca3d71385c1e35577edf4ae4a986`。原样存档、转换数据、完整许可、归属和变更说明在 `engine/resources_open/`；可重现转换器为 `material_audit/Convert-TriX7266.ps1`。

转换只改变单银通道的表示：感光度/曲线重复到三个通道、染料密度等分以守恒总光密度、模型/子层重复，Gaussian CDF 标记对应现有实现；不移动曝光轴，不修改旧材料或核心算法。保留无测量波段的 null。只支持上游已有的一个显影条件；颗粒/光晕采用当前引擎默认，未独立拟合 7266，也没有宣称实测成片等价。

实际构建：`build/qt-20261030-materials-1`，Release / Qt 6.11.3 / MinGW 13.1 / NVIDIA RTX 3050 Laptop Vulkan。

- 首次配置：中文 `$PSScriptRoot` 导致 Qt QML 扫描路径乱码；改用 `C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1` 映射路径配置成功，原目录未改名。
- 初次 CTest：25/26。新增测试持有临时 QJsonArray 的 QJsonValueRef，修复为按值读取；未放宽数值守恒门。
- 第二次窄门：TIFF 解码依赖可选 Qt 插件而失败；改为独立 IFD 头/尺寸验证，既有输出测试仍验证原生 TIFF 逐像素内容。
- 新增 `spk_qt_open_material`：**2258 项通过，6.26 秒**；转换数值/缺数据守恒、实际 Vulkan、默认效果、旧 Portra 像素完全不变、恢复和三种编码器。
- 修复后完整 CTest：**26/26，171.31 秒**；日志 `build/material-audit-20261030/phase1-ctest-final.log`，详细输出在对应构建目录 `Testing/Temporary/LastTest.log`。
- 当前为控制器/真实 QML 离屏测试及实际 NVIDIA GPU，不是物理窗口点击或显示器验收。未运行 Metal 同源 parity。

此阶段未开始批量替换；后续两款多显影时间材料必须先选择并记录条件。阶段 2/3 的管理与 UI 需要自己的测试，不能用本阶段通过代替。

## 阶段 2：材料管理核心

`MaterialLibrary` 使用独立用户目录和每个实例的私有运行资源视图；内置数据不变。支持完整 JSON Profile，以及 `SpektraLab.MaterialPack` v1 的 manifest/逐文件大小和 SHA256；每包最多 32 份、32 MiB，每个 Profile 最多 4 MiB。只读取数据，不加载插件代码；拒绝路径穿越/目录逃逸、别名依赖、未知/偏斜 CDF 和不兼容的显影维度。Qt 结构检查后再运行引擎原有 `Profile::from_json`，没有扩展 C API。

导入生成 `usr_<logicalID>_<sourceSHA前24位>`，保留 original.json 和独立 profile.json；索引记录完整 SHA256，使用锁和原子提交。相同原文件重复导入幂等，不覆盖历史；内容升级生成新版本。禁用/移除只改变新选择，存档保留供旧照片复现；损坏存档从运行视图隔离。`PreviewHost::reload_catalog` 是 C++ 桌面层的原子重扫，不修改引擎算法。

窄门 `spk_qt_material_library` **通过，4.17 秒**，执行真实 NVIDIA Vulkan。期间修复了误拒实测曲线局部起伏以及误拒空可选 adaptation window 两个校验问题；未改原始数据或放宽曝光轴检查。编译阶段修正了测试的 QString/std::string 转换和 JSON 初始化括号。日志在 `build/material-audit-20261030/phase2-build-6.log`，详细测试输出在当前构建 `LastTest.log`。

此时尚未接入 Qt 管理入口，也尚未验证编辑记录版本锁定；不能用核心测试宣称第三/四阶段已完成。内置升级历史自动重放不在此核心门的验证范围，后续必须明确缺失或不匹配时拒绝，不允许静默换材料。自定义 Profile 的许可字段是用户/来源声明，不是法律合规认证；逐胶片的 pack 默认参数/中性滤片并不会仅因导入 JSON 自动迁入。

## 阶段 3：Qt 管理入口与编辑记录

顶部“材料库”打开内置/自定义/已移除历史版本分类，可查看原材料 ID、当前引擎 ID、名称、来源、版本、完整哈希、质量与许可声明；支持导入完整 Profile 或 pack.json、启用/禁用、自定义项移除和用于当前照片。内置项不可删除。导入目录重扫不改变当前显示帧。

新编辑记录为 v7，胶片和相纸记录完整 SHA256。v1–v6 保留兼容解释，直到用户保存时迁移；旧格式此前没有的哈希不能倒推成历史证明。自定义材料 ID 固定内容版本；同名新版本导入不会重定向旧编辑。缺失/损坏/指纹不匹配时拒绝打开而保留记录，不回退 Portra。内置数据不由管理器更新；外部安装包改变内置内容时旧 v7 编辑会明确拒绝，不自动重放旧安装包材料。CineStill 指纹联合别名和 Vision3 基础文件，避免只锁定名字。

首轮窄门：`spk_qt_edit_store` 通过 0.66 秒，`spk_qt_open_material` 通过 6.85 秒；QML 门通过 **173 个报告字段、102.75 秒**，新增实际控件打开面板、使用 7266、调用导入 handler、注册自定义版本并通过选择按钮渲染。`spk_qt_material_workflow` **30 项通过、9.41 秒**，覆盖升级不重定向、禁用/移除后历史恢复、历史导出、v7 版本不匹配和损坏版本拒绝。修复过一次存储观察目录未随材料注册更新的红灯：渲染可选，但旧 catalogue 拒绝保存新 ID；现同时更新存储校验目录，并独立断言实际写入的 ID 和 SHA。

最终整合会使用新的 `build/qt-20261031-materials`，完整 CTest、最终程序哈希/源码清单以其 `qt-build.json` 为准。后续新增联合别名指纹及运行期间的内置数据防改检查还需由该整合门验证。真实物理窗口使用测试另记；此处离屏通过不当作已完成真机点击验收。

## 追加材料：Double-X 5222

`5b11ce6` 新增 `open_kodak_doublex_5222_65m_033`。来源仍为固定 dev commit，原始文件 SHA256 `37c333046554151ac5538e6eeb1f2cadef919876c63d0dd861c547952874fac3`，Andrea Volpato / CC BY-SA 4.0。转换器 `material_audit/Convert-DoubleX5222.ps1` 强校验源字节哈希，明确选取五组条件中的 **6.5 分钟**，基底、曲线、子层和模型统一切片；其余原始条件留在存档。没有插值、重新拟合或新的颗粒/光晕校准。新增边框铭文是桌面展示，不改胶片物理。

扩大的 `spk_qt_open_material`：**6236 项通过、17.51 秒**，分别覆盖 7266 与 5222 的轴/密度/模型/子层转换守恒、缺测保留、真实 Vulkan、扫描/印相路径、默认效果、确定性、旧 Portra 像素不变、保存恢复与 TIFF/PNG/JPEG。5222 自动配现有 Ilford 银盐相纸。该窄门先于最后的完整构建。

## 阶段 4：版本保护与交付验收

最终编辑格式升级为 **v8**：在阶段 3 的 Profile 指纹之外，联合锁定 `spektrafilm_constants.bin`、`neutral_print_filters.json`、`print_luts.json`。常数 blob 内含烘焙的印相 LUT，因此不能只锁定材料 JSON。运行期修改全局资源拒绝重扫；旧编辑遇到安装数据变更会报错，不自动找回旧安装包。v1–v7 兼容读取，不在读取时改写；旧版本没有的指纹只在后续保存时锁定当前资源，不能倒推历史数据。新增回归覆盖全局指纹冲突和旧格式读取不永久 dirty。

Windows 原生窗口测试第 1 次失败 `pan_clamp_no_dead_zone`：测试把 125% 缩放的 1 个逻辑像素误当整数物理像素；改为恰好一个物理像素的逻辑步长，仍以 1e-6 误差检查。第 2 次失败 `qml_real_latitude_graph_visible`：真实窗口的布局/Canvas polish 尚未发生，改为实际窗口抓图后检查，并增加诊断字段和失败截图。**没有删除断言或改渲染算法**。第 3 次真实窗口测试通过（`build/validation-20261031-materials-native-3/report.json`），包括材料面板、7266、自定义导入/版本选择；该次早于 v8 和 5222，需要最终版本再跑一次。

电脑控制服务初始化连续两次因“系统找不到指定的路径”失败，未能执行外部人工式鼠标/原生文件对话框自动化。替代门是应用自有 QML/QEvent 测试驱动，实际 `windows` 平台、D3D11 场景图、NVIDIA Vulkan 渲染和 QQuickWindow 抓图；不是 offscreen。它不能证明显示器校准、原生系统文件选择器或主观观感验收。

最终 Release/全套 CTest/原生窗口的实际结果记录在 `build/qt-20261031-materials/qt-build.json`、`Testing/Temporary/LastTest.log` 与 `build/validation-20261031-materials-final/`。只有这些命令通过并核对源码清单后才视为最终门通过；本文件不预先把待运行的测试写成成功。

## 未纳入本轮与边界

- Kodak 2302 属于电影印片材料，不能伪装为普通相纸套用 Ilford 滤片；需要独立印片配对/滤片与条件门后再导入。
- film-scan-calibration 的原始扫描/感光/MTF 数据仍需要前向模型拟合，不是可直接注册的 Profile；富士 C400 没有因此变成独立实测模拟。
- 内置材料历史自动归档/跨安装自动重放未实现；内容冲突采用拒绝打开保留记录。自定义版本存档可恢复，移除是逻辑删除，未提供永久清理。
- 未运行 macOS Metal parity 和 macOS GUI 测试；不是 Windows 能代跑的门。没有声称颗粒/光晕达到新增胶片的独立实测拟合。
- 这次是本地 Release 开发交付，不是签名安装器或可直接对外发布的许可验收。Windows 既有第三方源码/再分发清单仍须按 `THIRD_PARTY.md`、`package.ps1` 的 gate 单独检查。

提交链：基线 `b2575d4`、审计 `12b55b2`、阶段 1 `2a284fd`（原字节保护 `7c3268f`）、阶段 2 `e453b7a`、阶段 3 `b3d90ea`、第二款材料 `5b11ce6`；阶段 4 独立提交承载最终版本保护、测试驱动修复及本交付记录。

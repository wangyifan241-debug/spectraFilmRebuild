# Windows 界面与响应改进（2026-10-31）

对应用户指出的白色标题栏、粗大控件、列表无标记和操作卡顿。没有修改胶片物理、光谱资源、颜色算法或 C ABI。

## 界面

- Windows 原生标题栏采用 DWM 深色模式和与编辑区一致的 caption/text 颜色，不改系统设置，不去掉原生拖动、缩放、最大化、Snap 和窗口按钮。自测核实三个设置调用成功，并记录各自 HRESULT；CAPTION_COLOR 文档仅支持 Set，额外 Get 失败不误判为设置失败。此检查不是标题栏的屏幕像素验收。
- 用共享 QML 控件统一 Slider、ComboBox、CheckBox、ScrollBar：细轨道、小手柄、深色弹出列表、明确焦点和滚动条。保留原控制器接口及键盘行为。
- 胶片/印相材料行按 `info.use=cine` 显示 `CINE`，自定义非电影材料显示“自定义”。这是分类，不是给每份材料编造商标或精度徽章。CineStill 800T 原 Profile 为 `use=still`，仍按原数据分类，不因名字含 Cine 就强行打标。
- 材料名称允许换行，右侧留出标记和滚动条空间；分组可点击/空格折叠。未替换用户 macOS README 截图。
- 顶部快速选择框显式与当前胶片同步，避免模型初始化后下拉文字还停在第一项而列表已选另一项。

相关官方接口：[DWM 属性](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute)、[Qt 控件定制](https://doc.qt.io/qt-6/qtquickcontrols-customize.html)。共享控件为本项目代码，不含外部位图素材或新许可依赖。

## 响应

原来 Vulkan 工作线程返回后，`accept()` 在 GUI 线程调用显示图像工厂及几何/显影转换。现在随任务捕获 geometry/develop，在工作线程生成不可变 QImage，GUI 只在 generation/revision 门通过后一起发布 frame/图像/参数。过期结果不显示；失败保持原画面。工厂回调的线程契约在头文件注明，测试回调不能访问 GUI 对象。

新增 `spk_qt_responsiveness` 用自有 DNG、私有编辑目录和实际 Vulkan：注入 120 ms 的显示工厂工作，检查它不在 GUI 线程执行，且期间 GUI 心跳确实前进。先在旧实现实际失败 `Presentation conversion still executes on the GUI thread`，修复后窄门通过，观察到 24 次心跳。这个数字是线程归属/存活测试，不是帧率、真实照片耗时或整体性能基准。

显影后普通数值滑块改为拖动时显示待定数值、松开后应用完整精度，方向键仍可逐步应用，并显示相应提示。避免每个鼠标移动都同步处理整张图；没有假称已实现 macOS 的 GPU 实时 Layer 2。曲线拖动、部分几何/显影操作仍有同步 CPU 路径，后续完整实时着色器迁移尚未完成。

## 验证边界

窄门已跑：响应测试、552 项光谱反演/可达性测试、125 项旧曲线拟合测试。旧 v1 真实拟合报告保持原字节。原生空编辑器、显影后和下拉弹出三种窗口均由应用自身显示/抓图，stderr 为空；最终完整构建与真实 RAW 的结果见 `validation/materials-20261031/UI_RESULTS.md`。

Computer Use 技能服务两次初始化均报 `failed to write kernel assets ... os error 3`，重置后未恢复，未使用其它 Windows UI 自动化绕过。应用自测可以检查自己的控件、场景和 DWM 属性，但不是人工鼠标、系统对话框、屏幕校准或持续拖动帧率验收。旧用户进程未关闭；最终程序构建到新的 `build/qt-20261031-ui/app` 目录。

中间失败如实保留：旧目录 GUI 正由用户运行，尝试链接时被 Windows 拒绝，之后改用新目录，未停止用户进程；最初窄门 QML 子进程缺 qoffscreen 插件，补齐后重跑；`findChild` 无法遍历 ListView delegate 的视觉父子关系，断言改为遍历实际 `QQuickItem::childItems()` 而非删除标记检查。第一次真实 ARW 原生测试因错误要求 DwmGetWindowAttribute 读取仅文档支持 Set 的颜色属性而失败，改为分别记录设置 HRESULT 与可选读取，仍要求三个 Set 成功，不能删除标题栏门。首次失败的构建/自测不作为交付成功记录。

随后真实 ARW 门在已通过标题栏、滑块和半格后触及旧 180 秒总时限（173 字段，timeout），不记成功。原生大图门总预算单独调整为 360 秒，以容纳整个串行导出/多图场景；自有 2 MP 离屏门仍为 180 秒。这只调整测试运行预算，不改变渲染质量或证明交互速度。最终重跑情况必须看结果记录，不能将延长超时冒充性能优化。

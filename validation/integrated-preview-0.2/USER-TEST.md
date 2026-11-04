# 人工测试步骤 — Integrated Preview 0.2

状态：Ready for User Test；用户验收前不标 Accepted。

1. 保留完整 app 依赖目录，启动 SpektraLabQt.exe。安装包须安装到新的短路径空目录；拒绝覆盖已有版本。程序不写入照片目录。
2. 导入 Sony ARW。先用 Portra 400 / Endura 检查预览、前后对比、裁切、旋转、缩放、撤销/重做。
3. 在“胶片”列表选择 Phoenix 200 Round 5 Experimental；再切换 Phoenix II 200 Round 5 Experimental。两者是独立的新材料 ID，不覆盖 Experimental v1。
4. 旧/新比较：在同一照片上切换对应 Phoenix Experimental v1 与 Round 5，等待每次预览结束；空格/原图比较显示的是 RAW 解码图，不是旧版胶片。两代候选仍是未标定的光谱扫描代理。
5. 第五轮支持现有曝光、后期与几何操作；相纸、Y/M 滤色和 glare 不参与扫描。不要将这些控件无变化解释为求解成功。边框、日期、双拼和场景拟合不支持；Process 在第五轮禁用。原尺寸导出会明确拒绝，不能把 1024 长边预览冒充原尺寸。
6. 关闭程序，再打开并导入同一文件，检查材料选择和曝光等设置恢复。该 Preview 使用独立的应用数据目录，与旧版编辑记录隔离；旧版记录不会被覆盖。手动回退只需启动旧程序，旧程序的数据仍保留。
7. 标准负片可用“放大机 · 相纸亮度 → 配光 Process”。它使用当前会话报告的曝光/中性包，归零手动 Y/M 并重新印相；中性状态可以没有肉眼变化。配光诊断可查看 before/solve/actual 参数。它不宣称全自动主观色彩优化。第五轮扫描不使用该流程。
8. Advanced Color 诊断只显示 Windows 显示能力；本程序画布仍 SDR。RGB32F 引擎传输已接入底层，但没有 HDR 画布或 HDR 文件导出。
9. 稳定材料使用现有 TIFF16、PNG8/16、JPEG8 的 sRGB 输出；负片 TIFF 保留片基并使用 ProPhoto 标签，供外部反转。自动负片反转的正片显示与保留色罩的负片 TIFF 是不同输出模式。原尺寸是否通过以本次报告列出的材料/图像为限。
10. 验收记录请写明照片、材料、操作、预期和实际结果；视觉认可不等于物理精度已标定。Physical Film Accuracy: Not Verified。

本轮交付后停止，等待人工反馈，不自动新增研究或 PR。

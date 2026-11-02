# 第二轮执行报告 — 2026-11-02

本轮实验与要求的工程检查已执行；结果不支持将任何候选升级为更准确的产品材料。所有九个候选为 **Inconclusive**，产品 **Retain Baseline**。曝光单调性检查发现失败，不能宣称全项工程 Pass。

## 固定边界

PR4: ba076b2befe6586ad3095f897ebf40ea7e9ffd6c；PR6 已推送补强: 00405ee8ad5a448b09e64b1bf9195f1bb429f9a9。父材料与 SHA256 由各候选 plan 固定。v1、染料代理、底基、颗粒、光晕及印相包均未修改。II 特性图仍为 Unverified Generation。独立图源审查与先行设计见 RESEARCH_DESIGN.md。

## 实际执行

- D55：两款仅改归一化权重的候选；从受版本控制的常量包提取 colour/sd_illuminant/D55，81 点、380–780 nm、5 nm 步长。D55.json 记录源 SHA256、原表与相对功率单位。使用 sum(S*E*5nm)，通道归一化中的共同步长消去。未将图面照明条件等同于未校正感度。
- RAW：Sony ILCE-7M5 ARW，7168×5120，39510016 字节。真实输入与完整解码数据 SHA256、LibRaw 版本、AHD/as-shot WB/compatible16 策略见 raw-execution.json。执行一张全分辨率基线输出，全部十一款在同一解码数据的固定 stride=8 采样上对照。采样限制明确保留；没有声称所有候选完成全分辨率验证。用户照片及其渲染不入 Git。
- 24 色块与曝光：十一款 × EV -6..+6 整数档，共 143 组 native Vulkan 执行成功；每组驱动包含重复、重印与结果生命周期检查。exposure24-results.json 保存逐块 RGB、D50 Lab、相对 Y、同 EV 与 v1 的输出变化 ΔE00。色块是明确给定 RGB 的近似合成目标，不是测量反射谱，不是实拍 ColorChecker。肤色/绿/蓝分别由色块覆盖。
- 曲线：curve-checks.json 记录密度非负/有限/单调与边界，以及 log10 exposure -2/0/+2 附近的斜率。II 宽度候选保持纵轴与边界，仅共同缩放曝光坐标和 CDF centers/sigmas。
- 预览／导出／现有 Portra 回归：复用 PR4 Phoenix Qt 测试并扩展 ID、版本白名单，18014 checks，通过十一款的真实 PreviewController、PNG8/16、TIFF、JPEG、曝光 reset、配方持久化和恢复 Portra 图像一致性。preview-export-execution.txt 是本次凭据。这个进程内测试不是人工观察 GUI，也不验证显示器色彩或卡顿。
- FrameOutput 实际重跑通过 PNG8/16、TIFF16/ICC、JPEG、几何、resize、不覆盖、cleanup、source immutability；色彩输入回归通过；research checks 3254 通过；基础设施 39/39 通过。

## 失败及限制

曝光输出亮度单调性在 v1 与全部候选的极暗区域均存在失败，完整位置和下降量见 exposure-summary.json。例如一代 v1 灰块22从 -6 EV 的 Y≈0.1331 降至 -3 EV 的≈0.1251 后上升。不能用 uint16 输出有限代替内部 float NaN 检查；材料数组与导出观测无 NaN/Inf，但未直接捕获所有 GPU 中间值。

首次导出测试缺 JPEG 插件失败；设置 QT_PLUGIN_PATH 后完整重跑成功。配方 UI 字段 film_exposure_ev 显式转换成 C ABI exposure_compensation_ev，关闭 auto_exposure；父子同配方。未修改 PR6 配方文件。

执行复用了本地 PR4 时期预构建引擎/对象。native-execution.json 固定驱动哈希，RAW 记录 resolved 参数；并非本工作树重新构建全部引擎，不赋予完整源码重建认证。preview-export.cjs 明确生成测试源码并链接已有对象，保留该限制。脚本当前为本机路径，移机需显式配置相同依赖，不能声称零配置可复现。

## 决策

九个候选逐项 Inconclusive：线性纵轴、一代省略 tungsten 反除、共享通道归一化、D55 权重以及 II 两种响应宽度均是可辨识的敏感性实验，没有足够测量依据选优。候选不是因色差更小而 Adopt；该 ΔE00 比较的是模型输出。保留全部研究候选、产品保留 v1。无真实胶片配对参考，因此摄影真实性仍 Not Verified。单调性失败应独立调查其所属阶段，本轮不调整材料来遮盖它。

本轮到此停止材料实验，不自动进入第三轮。第三方调研另分支、另 Draft PR。

最终复核补充：SOURCE_REVIEW_FINAL.md 更正JCH图注误读（实际Phoenix II）。该标注确认不等于厂家测量认证。工程失败及剩余P0/P1/P2详见该文件；旧STATUS为阶段历史。

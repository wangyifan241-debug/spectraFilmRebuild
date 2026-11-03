# 实际测试与失败保留

日期：2026-11-03，Windows / NVIDIA GeForce RTX 3050 Laptop GPU / Vulkan。未做速度比较；墙钟时间仅用于识别测试是否实际运行。

| 门 | 实际结果 | 证据 |
|---|---|---|
| Windows 全套，包含新增扫描/数据检查 | 43/43，0 failed，296.57 s | evidence/windows-final-ctest.txt |
| 最后曝光组 CLI/固定校准改动后的目标门 | 3/3，0 failed | evidence/windows-round4-final-targeted.txt |
| 新数据标准 | 8/8，0 skipped | evidence/dataset-tests.tap |
| 独立最小积分参考 | 7 项合成检查通过 | evidence/spectral-proxy-tests.txt |
| 原验证基础设施 | 51/51，0 skipped | evidence/inherited-validation-tests.tap |
| 两代未插桩 C ABI vs 逐节点光学印相 | 每代最大 RGBA16 差 0，合成及真实 RAW 均通过 | raw-report/synthetic-report 的 uninstrumented-control |
| 原始指数光谱 vs 独立 float64 oracle | 每代 64 个实际层系数采样，误差见 metrics，门 3e-5 logT | evidence/raw-metrics.json、synthetic-metrics.json |
| 预览文件 vs TIFF16 | 合成 14 对，RAW 14 对；RGB16 逐通道精确相等，TIFF ICC 字节匹配 | *export-verification.json |
| 合成曝光组 −2/0/+2 EV | 同代扫描 recipe SHA 完全相同，补偿 gain 确实被激发，关闭补偿 gain 固定 | exposure-bracket.json |
| 真实胶片拟合 | 未运行；空数据集，预期拒绝 --require-fit | dataset.json |

单通道超出 [0,1] 的统计在层密度/logT 等阶段只是诊断统计，不应误称该阶段的显示裁切；只有标明线性/输出编码的裁切比例才按显示范围解释。recipe 端点钳制率单独报告。synthetic-metrics 中的 Lab 差是模型软件差异，不是照片真实精度。

实际只读 GUI 检查确认：第四轮窗口能够同时显示 v1 Optical Print 和扫描候选；旧实验版当前选中 R3 去负项候选、Frozen Print。一次截图错误抓到了用户 PowerShell，**已排除，不作为任何验收证据**；之后重新定位、激活窗口并核对了正确的第四轮 A/B 画面。没有把窗口标题/UIA 文本单独视为截图正确的证据。缩放/拖动控制已经提供；显示器校准、色彩管理后的屏幕测量未验证。

## 故意破坏的负例

- 把 Beer–Lambert 指数替换为线性近似，独立 oracle 失败（exit 1），保留 spectral-deliberately-broken.txt。真实实现恢复后通过；失败不计入正常通过数。
- 独立复制的数据验证器禁用跨 roll/scene 守卫，泄漏测试变红，保留 dataset-deliberately-broken.tap。最终实现保留 roll/scene/媒体哈希三类守卫。空集不可以绿灯代表真实验证。

## 开发中发现并修正的失败

独立 CMake 子目录最初没有在顶层建立 engine/Qt 作用域，配置失败；诊断调用私有 prefix 的初稿编译失败，现通过专用 research friend 访问；一次编辑脚本使用系统 GBK 解码而失败，改为显式 UTF-8。最终生成器和构建通过。

初版 A/B 默认选到了负片/关闭补偿对照，已改为 v1 Optical Print/扫描候选；缩放布局改为根据 viewport 尺寸计算。早期工具直接输出 sRGB，与原应用 ProPhoto 工作路径不同，最终统一为应用既有 OutputTransform。早期结果仅在本地 early 目录保留，没有替换最终报告。

完整生产 macOS 套件及 Python oracle 不在当前 Windows 环境执行。不能以 Windows 43 项代替 Metal parity。原生尺寸入口提供但未验收；本轮 RAW 证据是明确标记尺寸的预览渲染，照片及其派生图像仅保留本地，不随 PR 上传。

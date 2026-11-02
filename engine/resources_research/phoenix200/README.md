# HARMAN Phoenix 200 一代：公开资料重建可行性

审计日期：2026-11-02。首轮结论：**当前证据不足以生成已完成物理标定的 Profile**。
后续用户明确授权有来源等级的近似模型，已生成 Experimental v1，见 `../PHOENIX_MODELS.md`。
下面的物理标定缺口仍成立；数字化资料与实验配置的可用性是两件事。
这是一份研究记录，不会注册、打包或改变现有材料。仅针对一代；二代现已同步研究，见 `../phoenix2/README.md`。

## 固定来源与复核

- 作者/权利人：HARMAN technology Limited。
- 标题：HARMAN Phoenix 200 Technical Information，Nov 2023，6 页。
- 实际下载：https://www.revelab.es/fichas-carretes/harman-phoenix-200-datasheet.pdf
- PDF SHA256：`9c5c63e8e0123f66edf0165a0246beb98e0e872ca2748b0f6267d33ae334472d`
- 官方链接：https://www.harmanphoto.co.uk/amfile/file/download/file/1963/product/2143/
  官方在线解析为 Sep 2024、7 页；不能宣称它与本地 2023 文件字节相同。本次官方直接下载被远端关闭。
- 下载文件 `source.pdf` 和 Poppler 渲染 `page-2.png`、`page-3.png` 只作本地核查，已忽略；未获得再分发许可，不套用上游 profile 的 CC BY-SA。
- 本地用 Poppler `pdftoppm -f 2 -l 3 -scale-to 1800 -png source.pdf page` 渲染，两页均人工查看。

## 实际能从图里得到什么

第 2 页感度图：横轴 300–800（波长），纵轴 Relative Sensitivity 0–5；三条曲线标 Blue/Green/Red。
条件仅明确为 2856 K tungsten 的 wedge spectrogram。未说明纵轴是否对数、楔常数、固定密度判据或能量单位。
不能直接写进 `log_sensitivity`，也不能无条件除以黑体谱：这两种解释会产生不同的日光颜色。

第 3 页特性曲线：横轴 Relative Log Exposure 0–3，纵轴 Density 0–4。
条件为标准 C41。没有在图页注明 Status M/Status A、绝对 lux·seconds、曝光时长、曝光光源或曝光零点。
`characteristic-preview.csv` 是对本地 1273×1800 渲染的稀疏人工图面取点，不是厂家 CSV。
校准矩形 x=139..617、y=498..200；x→3*(x-139)/478，D→4*(498-y)/298。
样本保留图面像素坐标；未平滑、未把起点移到 ISO 200、未转换成染料层量。
读点为初步近似，低曝光两条下方曲线接近/交叉，通道归属不确定；CSV 仅记 lower/upper 包络，不伪造红蓝分离。
这些稀疏点只支持形状审计，不用于拟合验收，暂无经独立复核的误差上界。

## 后续完成的正式图面提取

`spectral-chart.csv`：从 PDF 的三条原生 cubic Bézier 路径取样，300–800 nm、1 nm 间隔共 501 行。
CSV 保存印刷图面的原始纵轴值，不把未定义刻度转换为 `log_sensitivity`；路径支持外留空。
`characteristic-chart.csv`：直接解码 PDF 的 795×559 内嵌位图，提取黑色笔画中心，共 666 列。
绿通道可单独分离；红蓝重叠及可能交叉的低曝光区保持未知，不以排序强制赋通道。
这些文件替代 `characteristic-preview.csv` 用于后续研究；旧文件保留作初步记录，不作为拟合输入。
`digitization.json` 保存完整校准、方法、源哈希与缺口；`digitize.cjs` 可离线重现。
先用上述 Poppler 命令生成 `page-2.png`，再运行 `node engine/resources_research/phoenix200/digitize.cjs`。
生成的本地 `digitization-review.html` 与 `characteristic-review.png` 可检查取点与源图重合情况。
数字化不等于物理标定；仍不会绕过下述输入语义和质量门。

## 为何暂不进行染料反演

1. `windows_UI/material_fit` 的一维模式要求 30–4096 点、确定的绝对 log lux·seconds 与 Status M 密度。这里两项语义均未知，不能为满足输入验证而伪造。
2. 光谱模式还要求明确的仪器响应、染料和底基光谱。凤凰自身这些数据均未获取。
3. 在仪器响应、染料和底基都未知时，同时拟合它们与层量，不能唯一恢复真实材料。代理模型闭合也只证明自洽，不证明凤凰准确。
4. ISO 200 是速度声明，不足以补齐上述判据和曝光轴零点。

未运行拟合器或渲染回归：没有满足输入语义的材料，运行会制造错误的置信度。没有生成 Profile。

## 同等程度的判据与后续选择

沿用 Pro Image 的四项阻断门：仪器密度到染料层量的光谱闭合、底基/染料及光谱尾部不确定性、感度和绝对曝光标定、独立照片/颜色/Vulkan 回归。
当前仅完成原页和形状审计，四项均未通过。

公开资料仍可继续寻找；若始终没有新增约束，单纯增加取点精度或换优化器不能解除这些缺口。
可选择明确接受较低证据等级的实验近似模型，但这不同于本次要求的“与其它胶片同等程度”，本研究没有擅自降级该要求。

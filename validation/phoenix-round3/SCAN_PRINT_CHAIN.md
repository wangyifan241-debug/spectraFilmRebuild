# 扫描与印相链路

| 阶段 | 本轮可观察范围 | 限制 |
|---|---|---|
| 输入线性RGB/光谱重建 | 固定float32 ProPhoto输入、SHA256；film_scale_and_expose输出 | 完整81波长场景重建Not Observable。引擎将Hanatos表在setup中压缩到感光层响应，不由最终RGB反推 |
| 感光层曝光与响应 | film_scale_and_expose、film_log_and_curves、film_couplers、film_grain的24取样像素 | 非全帧缓冲，不能宣称全GPU节点NaN扫描 |
| CMY+底基密度 | 实际GPU层系数和print_chd/base读取，CPU重建81点D | 数学重建，不冒称直接读取的光谱图像 |
| 负片透射 | CPU T=10^-D | 同上，保留未经输出裁切数据 |
| 固定光照负片观察 | baked D50+CIE1931 2°的独立CPU积分 | 理想观察者，无扫描器硬件响应，无反转 |
| 纸乳剂曝光/显影 | node_enlarger_spectral、node_print_exposure、node_print_curves | optical print路径，与负片扫描反转不同 |
| 正片观察XYZ/RGB | node_scan_spectral、node_xyz_to_rgb、node_gamut_compress、print_scan_finish | node_scan_spectral观察的是纸材，不是直接负片正像 |
| 输出色彩空间/ICC | 编码ProPhoto原生RGBA16；Qt预览/导出嵌入ICC独立测试 | 原生RGBA16并非自带ICC文件；其契约与标签不可混淆 |

诊断只在独立构建目录生成pipeline副本，不编辑产品engine源码。52组1像素及143组24色块插桩/未插桩输出逐字节一致；阶段值与常数保存。双模式实验另记录其诊断DLL身份。

Frozen Print继承父v1滤镜包、shift=0、print_exposure=1及原filter_shift_scale。Recalibrated模式以相同父包为初值，scale=60使shift等价于CC调整；预定中性0.18输入、D50 Y=0.18目标，优化m/y与log2曝光。目标、步长、边界见plan和脚本。只用灰，不以彩色评估反向调参。固定预算不保证零残差或最优，全部残差与轨迹报告。

两个后输出异常的扫描空间阶段包含scanner blur/unsharp，不把该分类扩大为已独立分离二者的贡献。真实扫描光源/探测器、反转算法、真实纸材测量仍有独立缺口。

## 自动印相补偿的独立归类

printing.cpp::print_constants通过不同EV的中灰探针重新计算gain。全部1729个事件都有通道方向被gain反转；compensation-decomposition.json将delta log10(纸材曝光)分解为透射光变化与gain变化，逐事件加和误差受控。1727个事件属于带符号代理与自动印相补偿的组合路径，2个在后续scanner spatial finish首次造成Y下降。不能只归因负染料项。非负投影候选仍有Y下降，也反驳“去负项就能解决全部异常”的说法。

Frozen Print固定v1印相包、滤镜shift和显式print_exposure，不是冻结内部gain；当前自动补偿属于冻结算法的一部分。两模式每EV常数都绑定，材料变量会通过算法派生改变补偿，此影响不能误称纯负片透射效应。未开展第三种关闭自动补偿的产品/参数改造。

## PR9 Fixed Internal Gain 诊断

参考条件：每代父v1分别使用32×32恒定线性ProPhoto 0.18、EV0、Endura纸、父中性滤镜包、m/y shift=0、print_exposure=1；auto exposure、grain、halation和glare关闭。父参考条件、输入SHA、母材料SHA、滤镜表SHA及原生阶段常数保存于review-hardening/frozen-parent-gains.json。

- 一代父gain：[0.40848703560034955, 0.40848703560034955, 0.40848703560034955]。
- 二代父gain：[0.32690421903309935, 0.32690421903309935, 0.32690421903309935]。

| 模式 | 用户滤镜/曝光 | 实际内部gain | 用途 |
|---|---|---|---|
| Frozen Print | 父v1固定用户参数 | 沿现有算法随EV/材料重算 | 复现当前冻结链路 |
| Recalibrated Neutral Print | 基线已冻结的灰阶校准参数 | 沿现有算法重算 | 保留独立中性校准对照，不用彩块调参 |
| Fixed Internal Gain | 与Frozen完全相同 | 同代父v1参考值，所有候选/EV共用 | 隔离gain贡献；不宣称更真实 |

诊断只在独立pipeline副本中、光谱印相使用gain之前替换实际用值。原有自动参考计算仍可执行，但不能决定实际印相gain；每次CONSTANTS输出必须与父冻结三元组逐项一致，候选无重标定权限。黑/白校正及EDR均为false，因此原参考增益没有通过这些旁路影响结果；print_exposure_bw_gain=1。这个限制由实际resolved参数验证，不外推到开启校正或EDR的配方。

禁用研究开关的364组原模式输出与审核基线SHA逐字节相同；另11款旧材料EV0控制通过。Fixed模式4368个负片层密度采样与Frozen一致，证明变化发生在负片之后。D/T由实际层系数和常数重建，纸材曝光、纸密度、未裁切XYZ/RGB、最终XYZ/Lab均保存，不用最终RGB反推阶段。

旧1729个下降事件，固定gain后1720消失、9保留；旧数据域另新增23项，合计32项。第三轮材料域固定gain有46项。两域事件记录78项存在部分重叠，深层实验使用54个唯一(id,EV)帧。

深层DLL额外读取node_scanner_blur与node_unsharp，并提供仅用于研究的unsharp旁路。54帧不启用旁路时全部与主Fixed输出逐字节一致；78项记录在blur之后没有相应Y下降、在unsharp之后首次下降，旁路后78项全部消失。原始层值/纸曝光/纸密度/扫描XYZ/模糊/锐化/最终Lab前后值与反事实输出SHA见fixed-gain-stage-events.json、deep-scanner-results.json。

这只定位了当前合成块布局下的扫描锐化贡献，不修改产品锐化、不声称真实胶片不存在非单调，也不证明Gold或非负投影染料准确。

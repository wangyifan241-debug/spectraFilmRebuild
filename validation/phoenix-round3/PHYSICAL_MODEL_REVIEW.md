# 物理模型复核

D_total(lambda)=D_base(lambda)+Σ D_layer,c A_c(lambda)，T=10^(-D_total)。D是无量纲十进光学密度，波长nm。网格380–780nm，步长5nm，81×3的channel_density与81点base_density分开传输；离散积分在相同网格的归一比值里抵消Δlambda。

当前A不是已证明的峰值归一、非负Phoenix吸收形状。它是Gold继承基函数，系数标度和拟合出的层响应是一对；独立替换染料时保留层系数仅能形成敏感性实验，不能宣称材料物理等价。metadata许可与原测量可信度是两个不同问题。

build-phoenix.cjs直接复制Gold三列，将null转0；base单独合成。forward只加一次base，经透射率加权积分取-log10，再进行有界层密度反演。Profile::load会减去每通道density_curves的最小值形成normalized_curves；粒子模型另有自己的density_min处理。不能把原始特性曲线和归一响应同时相加。诊断使用film_grain之后GPU实际层系数和运行时print_chd/base常数重建，绕过猜测这些归一步骤。

prepare_spectral_constants对含NaN的波长整行将积分权重置0，同时清空该行密度；那是未测支持的处理，不是T=1的物理测量。源JSON上的零染料测试与实际有效积分波长必须分别解释。

已知基函数负项允许在层系数增加时某些波长D下降、T上升。1727个事件分类为该带符号代理/印相路径机制共现，2个事件定位到后续scanner spatial finish；不把共现当作唯一化学因果证明。全部3432个第二轮色块的独立CPU积分误差见independent-math.json。

数学验证同时包含非负独立吸收模型的单波长单调性；原Gold基函数不满足这一解释前提。将负项投影到0的候选只为敏感性，不是修复产品。最终正片Y受纸材、观察、空间处理影响，亮度单调性与单波长Beer–Lambert条件分别报告。

峰位/宽度/重叠/尾部：observe-negative.cjs的非负、单位连续峰Gaussian实验使用450/550/650nm中心、+5nm位移和25/30nm带宽；组合变化单独标interaction。它们仅为无胶片身份的解析对照，不输出Phoenix材料；不能把它们冒称厂家吸收测量。更宽的形状增加重叠和尾部吸收，具体D50 XYZ影响保存在negative-observations.json。

## 自动印相补偿的独立归类

printing.cpp::print_constants通过不同EV的中灰探针重新计算gain。全部1729个事件都有通道方向被gain反转；compensation-decomposition.json将delta log10(纸材曝光)分解为透射光变化与gain变化，逐事件加和误差受控。1727个事件属于带符号代理与自动印相补偿的组合路径，2个在后续scanner spatial finish首次造成Y下降。不能只归因负染料项。非负投影候选仍有Y下降，也反驳“去负项就能解决全部异常”的说法。

Frozen Print固定v1印相包、滤镜shift和显式print_exposure，不是冻结内部gain；当前自动补偿属于冻结算法的一部分。两模式每EV常数都绑定，材料变量会通过算法派生改变补偿，此影响不能误称纯负片透射效应。未开展第三种关闭自动补偿的产品/参数改造。

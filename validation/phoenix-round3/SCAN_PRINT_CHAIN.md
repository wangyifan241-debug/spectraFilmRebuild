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

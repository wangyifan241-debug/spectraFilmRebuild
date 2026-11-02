# 光谱物理初审

D_total(lambda)=D_base(lambda)+sum_c D_layer,c*A_c(lambda)，T=10^(-D_total)。D为无量纲十进密度；波长nm，81点、5nm步长。这里A使用继承基函数，不能未经核实称为峰值归一的物理吸收。

build-phoenix.cjs直接复制Gold channel_density，null转0；base单独生成。forward仅加一次base，按归一密度计积分透射率，再取-log10。成像层系数通过有界反演获得，不能再当作独立密度计测量重复叠加。

math-results.json记录基函数负值：增加对应层系数能提高部分单波长透射。该事实阻止把当前系数直接解释为独立非负染料吸收，但不证明原始Gold测量无效。最终正片Y是否单调需另测，不能与该数学条件等同。

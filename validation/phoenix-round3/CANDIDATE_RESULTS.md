# 第三轮候选结果

所有ΔE00是相同EV、相同模式下的模型间差异，不是真实胶片误差。校准仅用固定灰；未达到零残差的结果仍报告，不再针对色块调参。

| ID | 模式 | 模型间ΔE00均值/最大 | 灰阶Y下降 | 输出触边比例 | 决策 |
|---|---|---|---|---|---|
| harman_phoenix_200_exp_v1 | Frozen Print | 0.0000 / 0.0000 | 36 | 0.0000 | Retain Baseline |
| harman_phoenix_200_exp_v1 | Recalibrated Neutral Print | 0.0000 / 0.0000 | 36 | 0.0000 | Retain Baseline |
| harman_phoenix_ii_200_exp_v1 | Frozen Print | 0.0000 / 0.0000 | 42 | 0.0000 | Retain Baseline |
| harman_phoenix_ii_200_exp_v1 | Recalibrated Neutral Print | 0.0000 / 0.0000 | 42 | 0.0000 | Retain Baseline |
| phx_200_base_neutral_r3 | Frozen Print | 7.5435 / 22.1726 | 36 | 0.0000 | Inconclusive |
| phx_200_base_neutral_r3 | Recalibrated Neutral Print | 1.3543 / 4.2041 | 36 | 0.0000 | Inconclusive |
| phx_200_base_amp010_r3 | Frozen Print | 4.0722 / 11.0037 | 36 | 0.0000 | Inconclusive |
| phx_200_base_amp010_r3 | Recalibrated Neutral Print | 0.8942 / 1.8248 | 36 | 0.0000 | Inconclusive |
| phx_200_base_center560_r3 | Frozen Print | 1.2056 / 2.9002 | 36 | 0.0000 | Inconclusive |
| phx_200_base_center560_r3 | Recalibrated Neutral Print | 0.7204 / 2.0931 | 36 | 0.0000 | Inconclusive |
| phx_200_base_sigma075_r3 | Frozen Print | 0.9956 / 2.5894 | 36 | 0.0000 | Inconclusive |
| phx_200_base_sigma075_r3 | Recalibrated Neutral Print | 0.1977 / 0.5723 | 36 | 0.0000 | Inconclusive |
| phx_200_dye_portra_proxy_r3 | Frozen Print | 1.5763 / 5.7097 | 36 | 0.0000 | Inconclusive |
| phx_200_dye_portra_proxy_r3 | Recalibrated Neutral Print | 1.9754 / 4.8181 | 36 | 0.0000 | Inconclusive |
| phx_200_dye_nonnegative_r3 | Frozen Print | 0.7912 / 4.1913 | 36 | 0.0000 | Inconclusive |
| phx_200_dye_nonnegative_r3 | Recalibrated Neutral Print | 1.2612 / 2.9635 | 36 | 0.0000 | Inconclusive |
| phx_ii_200_base_neutral_r3 | Frozen Print | 9.7111 / 22.7977 | 43 | 0.0000 | Inconclusive |
| phx_ii_200_base_neutral_r3 | Recalibrated Neutral Print | 1.6197 / 4.6039 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_base_amp010_r3 | Frozen Print | 4.7006 / 10.8316 | 43 | 0.0000 | Inconclusive |
| phx_ii_200_base_amp010_r3 | Recalibrated Neutral Print | 0.6007 / 1.8921 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_base_center560_r3 | Frozen Print | 1.5344 / 3.0168 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_base_center560_r3 | Recalibrated Neutral Print | 0.8148 / 2.1803 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_base_sigma075_r3 | Frozen Print | 1.0637 / 2.6099 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_base_sigma075_r3 | Recalibrated Neutral Print | 0.2441 / 0.5804 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_dye_portra_proxy_r3 | Frozen Print | 1.1767 / 3.7948 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_dye_portra_proxy_r3 | Recalibrated Neutral Print | 1.5970 / 4.0028 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_dye_nonnegative_r3 | Frozen Print | 0.9029 / 3.1397 | 42 | 0.0000 | Inconclusive |
| phx_ii_200_dye_nonnegative_r3 | Recalibrated Neutral Print | 0.7631 / 1.9892 | 42 | 0.0000 | Inconclusive |

密度/透射边界、未裁切阶段值、标量积分误差和校准轨迹见JSON。保留所有单因素变化，不按观感选择。固定预算优化不声称全局最优；灰阶Y非单调与单波长物理条件单独记录。

## PR9 最终审查补强（三模式，基线9940ca3f）

Fixed Internal Gain仅为因果诊断，不代表更真实印相。每个代际使用自己的父v1 EV0/线性0.18参考gain，整组候选和全部EV共用；不是跨两代共用一个gain。Recalibrated模式重放先前冻结的灰阶校准参数，不重新拟合彩块。

| ID | 模式 | 全24块下降事件 | 其中灰阶 | 未裁切扫描XYZ模型间ΔE00均值 | 最终输出模型间ΔE00均值 | 决策 |
|---|---|---|---|---|---|---|
| harman_phoenix_200_exp_v1 | Frozen Print | 142 | 36 | 0.0000 | 0.0000 | Retain Baseline |
| harman_phoenix_200_exp_v1 | Recalibrated Neutral Print | 142 | 36 | 0.0000 | 0.0000 | Retain Baseline |
| harman_phoenix_200_exp_v1 | Fixed Internal Gain | 7 | 1 | 0.0000 | 0.0000 | Retain Baseline |
| harman_phoenix_ii_200_exp_v1 | Frozen Print | 167 | 42 | 0.0000 | 0.0000 | Retain Baseline |
| harman_phoenix_ii_200_exp_v1 | Recalibrated Neutral Print | 167 | 42 | 0.0000 | 0.0000 | Retain Baseline |
| harman_phoenix_ii_200_exp_v1 | Fixed Internal Gain | 0 | 0 | 0.0000 | 0.0000 | Retain Baseline |
| phx_200_base_neutral_r3 | Frozen Print | 145 | 36 | 7.5975 | 7.5435 | Inconclusive |
| phx_200_base_neutral_r3 | Recalibrated Neutral Print | 142 | 36 | 1.3610 | 1.3543 | Inconclusive |
| phx_200_base_neutral_r3 | Fixed Internal Gain | 5 | 1 | 2.9647 | 2.9135 | Inconclusive |
| phx_200_base_amp010_r3 | Frozen Print | 142 | 36 | 4.0992 | 4.0722 | Inconclusive |
| phx_200_base_amp010_r3 | Recalibrated Neutral Print | 141 | 36 | 0.8993 | 0.8942 | Inconclusive |
| phx_200_base_amp010_r3 | Fixed Internal Gain | 6 | 1 | 1.5818 | 1.5558 | Inconclusive |
| phx_200_base_center560_r3 | Frozen Print | 142 | 36 | 1.2132 | 1.2056 | Inconclusive |
| phx_200_base_center560_r3 | Recalibrated Neutral Print | 142 | 36 | 0.7250 | 0.7204 | Inconclusive |
| phx_200_base_center560_r3 | Fixed Internal Gain | 7 | 1 | 0.3492 | 0.3429 | Inconclusive |
| phx_200_base_sigma075_r3 | Frozen Print | 142 | 36 | 1.0028 | 0.9956 | Inconclusive |
| phx_200_base_sigma075_r3 | Recalibrated Neutral Print | 141 | 36 | 0.1991 | 0.1977 | Inconclusive |
| phx_200_base_sigma075_r3 | Fixed Internal Gain | 7 | 1 | 0.3654 | 0.3608 | Inconclusive |
| phx_200_dye_portra_proxy_r3 | Frozen Print | 141 | 36 | 1.5956 | 1.5763 | Inconclusive |
| phx_200_dye_portra_proxy_r3 | Recalibrated Neutral Print | 141 | 36 | 1.9850 | 1.9754 | Inconclusive |
| phx_200_dye_portra_proxy_r3 | Fixed Internal Gain | 8 | 2 | 0.3846 | 0.3774 | Inconclusive |
| phx_200_dye_nonnegative_r3 | Frozen Print | 142 | 36 | 0.8008 | 0.7912 | Inconclusive |
| phx_200_dye_nonnegative_r3 | Recalibrated Neutral Print | 141 | 36 | 1.2673 | 1.2612 | Inconclusive |
| phx_200_dye_nonnegative_r3 | Fixed Internal Gain | 6 | 1 | 0.4259 | 0.4162 | Inconclusive |
| phx_ii_200_base_neutral_r3 | Frozen Print | 167 | 43 | 9.7562 | 9.7111 | Inconclusive |
| phx_ii_200_base_neutral_r3 | Recalibrated Neutral Print | 167 | 42 | 1.6241 | 1.6197 | Inconclusive |
| phx_ii_200_base_neutral_r3 | Fixed Internal Gain | 0 | 0 | 10.9485 | 10.7670 | Inconclusive |
| phx_ii_200_base_amp010_r3 | Frozen Print | 169 | 43 | 4.7239 | 4.7006 | Inconclusive |
| phx_ii_200_base_amp010_r3 | Recalibrated Neutral Print | 167 | 42 | 0.6032 | 0.6007 | Inconclusive |
| phx_ii_200_base_amp010_r3 | Fixed Internal Gain | 0 | 0 | 5.6465 | 5.5627 | Inconclusive |
| phx_ii_200_base_center560_r3 | Frozen Print | 167 | 42 | 1.5420 | 1.5344 | Inconclusive |
| phx_ii_200_base_center560_r3 | Recalibrated Neutral Print | 167 | 42 | 0.8184 | 0.8148 | Inconclusive |
| phx_ii_200_base_center560_r3 | Fixed Internal Gain | 0 | 0 | 0.8053 | 0.7985 | Inconclusive |
| phx_ii_200_base_sigma075_r3 | Frozen Print | 168 | 42 | 1.0698 | 1.0637 | Inconclusive |
| phx_ii_200_base_sigma075_r3 | Recalibrated Neutral Print | 167 | 42 | 0.2452 | 0.2441 | Inconclusive |
| phx_ii_200_base_sigma075_r3 | Fixed Internal Gain | 0 | 0 | 0.6068 | 0.5986 | Inconclusive |
| phx_ii_200_dye_portra_proxy_r3 | Frozen Print | 167 | 42 | 1.1864 | 1.1767 | Inconclusive |
| phx_ii_200_dye_portra_proxy_r3 | Recalibrated Neutral Print | 167 | 42 | 1.6047 | 1.5970 | Inconclusive |
| phx_ii_200_dye_portra_proxy_r3 | Fixed Internal Gain | 0 | 0 | 0.6517 | 0.6363 | Inconclusive |
| phx_ii_200_dye_nonnegative_r3 | Frozen Print | 167 | 42 | 0.9093 | 0.9029 | Inconclusive |
| phx_ii_200_dye_nonnegative_r3 | Recalibrated Neutral Print | 167 | 42 | 0.7676 | 0.7631 | Inconclusive |
| phx_ii_200_dye_nonnegative_r3 | Fixed Internal Gain | 0 | 0 | 0.9360 | 0.9132 | Inconclusive |

相同模式、相同EV下与父材料比较，所有ΔE00仍非真实胶片误差。三模式负片透射、纸材曝光、扫描XYZ/Lab与最终XYZ/Lab逐块对照见review-hardening/three-mode-comparison.json；完整阶段、密度/透射见three-mode-results.json与fixed-gain-physics.json。每个材料的修改字段及父/候选SHA仍由material-plan.v1.json绑定。

在第三轮14款材料上，Frozen/Recalibrated/Fixed的24块下降事件合计2168/2159/46，灰阶548/546/8。Fixed剩余46项在扫描锐化阶段首次出现；隔离的研究锐化旁路消除这些项，但产品算法保持不变。不能因此将非负投影或固定gain升级为物理正确材料。

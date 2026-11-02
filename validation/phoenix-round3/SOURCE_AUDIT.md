# 来源审计 — 2026-11-03

## 身份与证据

两代v1父材料及SHA256见freeze.json，12个候选身份、父/候选SHA256与公式见material-plan.v1.json。产品材料没有更新。所有无实测支持的数值均为敏感性假设，不是Phoenix测量。参数不可识别时不编造置信区间。

| 参数 | 单位与意义 | 来源/等级 | 条件与不确定性 |
|---|---|---|---|
| Gold channel_density，81×3 | 染料基函数，乘有效层密度形成十进光谱密度 | 其他材料代理；engine/resources/profiles/kodak_gold_200.json | CC BY-SA 4.0，Andrea Volpato/spektrafilm；不是Phoenix实测。原始测量光源/仪器本轮未独立认证；有负项，不能视为三个独立非负吸收形状 |
| floor_density=0.18 | 无量纲D，非图像吸收常量 | 纯估计；phoenix-model-assumptions.json | 无独立测量误差，真实值不可识别 |
| green_absorption=0.20 | D，Gaussian幅度 | 纯估计 | 扫描紫色外观不能确定此幅度；本轮0.10及0仅为受控敏感性，不是置信区间 |
| center_nm=550 | nm，底基吸收中心 | 纯估计 | 560nm候选只移动中心，无测量先验 |
| sigma_nm=65 | nm，标准差；FWHM=2.35482sigma | 纯估计 | 75nm候选只改宽度，无测量先验 |
| base_density | 81点无量纲D | 合成常量+Gaussian | 包含非图像密度；无橙罩不等于无吸收，也不保证中性 |
| 合成密度计中心650/550/450；sigma35/30/25 | nm；归一积分权重 | 纯估计，非Status M | 中心整体+5nm、带宽+10%分别是仪器敏感性；未取得可再分发、可核实的标准响应数据 |
| Portra400 channel_density | 81×3染料代理 | 其他材料代理，CC BY-SA 4.0 | 与Gold同来源许可；原始仪器条件未核实，不满足“已知Phoenix真实染料”条件，只作为带明确缺口的替代代理敏感性 |
| CIE1931 2°、D50 | 标准观察者与相对照明表 | 包内已烘焙数据 | 来源链为spektrafilm/colour，blob SHA绑定；理想观察者不是扫描器硬件，不执行反转 |

本地参数文件及脚本的精确内容哈希由evidence-manifest.json绑定；材料源URL/许可/版本由原文件metadata和material-plan保存。源项目：[spektrafilm](https://github.com/andreavolpato/spektrafilm)。许可不是对其原始测量条件或Phoenix适用性的认证。

## 官方资料核对

本次HTTP下载三个官方入口均返回403；source-downloads.json里的哈希是响应体身份，不能冒充PDF哈希。随后核对本机已保存副本，SHA与tracked digitization.json完全相同，见pdf-source-audit.json：一代Nov 2023，6页，SHA256 `9c5c63e8e0123f66edf0165a0246beb98e0e872ca2748b0f6267d33ae334472d`；二代Jul 2025，5页，`020260790a3b705d38ded5573abcb2af43b228b5e3440bc23463273cf78bff08`。未将副本伪装为当前官网最新版。

原始下载URL/作者/数字化方法见engine/resources_research/phoenix200/digitization.json和phoenix2/digitization.json。官方入口：[一代](https://www.harmanphoto.co.uk/amfile/file/download/file/1963/product/2143/)、[二代](https://www.harmanphoto.co.uk/amfile/file/download/file/1968/product/2208/)。一代感度图注明2856K，二代D55；这些是感度图照明，不能当作染料光谱测量条件。厂家定性描述二代反差与色彩平衡改变、颗粒更细、宽容度更宽，不能推导具体染料峰位或底基吸收。

两代扫描建议都指出无橙罩及调整扫描设置的必要；二代还讨论正片扫描再反转的工作流。这里没有采用商业软件数据或其预设。资料没有为本实验提供可独立复用的Phoenix染料/底基数字测量表。

II特性曲线媒体图的图注纠错沿用第二轮SOURCE_REVIEW_FINAL.md；父材料旧metadata图注说明仅为冻结历史，不能作为本轮最新审计结论。厂家签发真实性和精密测量条件仍未认证。

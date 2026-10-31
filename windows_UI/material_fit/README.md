# 扫描数据拟合工具（离线研究）

`spk_material_fit` 是可运行的 C++/Qt Core 命令行工具，不链接到交互渲染路径，无 Python/scipy、无 CPU 胶片渲染回退。输入为审计固定版本的 `*_datasheet_curves.json`，输出为 **SpektraLab.ScanCurveFit** 报告，绝不是可安装 Profile。

```powershell
# build.ps1 部署的 app 目录已有 Qt/编译器 DLL；也可单独构建 spk_material_fit。
& '<build>/app/spk_material_fit.exe' `
  --input '<repo>/engine/resources_research/scan/ProImage100_datasheet_curves.json' `
  --output '<不存在的新文件路径>/proimage100-fit.json'
```

要求：30–4096 组有限数值，严格递增的 log lux-seconds 曝光轴，R/G/B Status M 密度。未知单位、缺来源、错误尺寸、NaN/null 拒绝，不自动移动曝光轴、填测量或改单位。输出存在时拒绝覆盖。退出码：0=曲线残差门通过（**不是材料可用认证**），3=已写失败诊断报告，1=输入/写入错误。

模型是非负的三项 Gaussian CDF 加非负基底 offset。解析 Jacobian、阻尼最小二乘、8 个确定性起点、有界参数；中心排序只规范化等价混合分量，不排序或平滑源样本。每五个内部点留出一个，只用其余点拟合。报告保存拟合参数、逐点预测/残差、留出索引、训练/留出 RMSE 与最大误差；门为训练和留出 RMSE ≤0.02 D、留出最大误差 ≤0.08 D。数字化曲线点相关，**留出验证不是独立实拍验证**。这些阈值仅针对一维插值/拟合质量。

Pro Image 100 原始曲线、光谱感光度和染料数据及许可均原样存档于 `engine/resources_research/scan/`，作者 OwlMightyCh，CC BY 4.0，固定 commit 和完整 SHA256 见该目录 NOTICE。原始文件未改。已知固定哈希的报告自动携带作者、commit 来源 URL、CC BY 4.0 许可链接与修改说明；其它输入不擅自宣称有同一许可，仍须附原作者授权。输出报告是本项目的新拟合/衍生物，应保留这些归属和修改记录。此目录不自动复制进应用材料库。

尚不能注册为 Profile 的原因：

1. 输入 R/G/B 是 **Status M 仪器积分密度（含基底）**，不是 cyan/magenta/yellow 内禀染料层量；不能把拟合三条曲线直接塞进 density_curves。
2. 染料文件明确为 Vision3 代理基底分解，存在先验和不确定性；基底/光谱支持不完整，不能把尾部补零宣称测得。原始 Status M 红尾截断问题仍保留。
3. 需要以光谱前向积分验证仪器密度→层量分离，并校准感光度、照明与曝光锚点，再做独立图像与 Vulkan 回归。

因此 `profile_ready` 始终 false，报告故意没有 Profile 的 info/data 结构，材料管理器拒绝导入。此次是完成可复现的一维拟合阶段，而不是伪造一款已经完成光谱标定的 Pro Image 胶片。

回归 `spk_scan_curve_fit` 使用已知合成曲线、错误数据/明显不可拟合曲线、固定真实源字节哈希、留出预测、可重复报告和“非 Profile”拒绝门。默认 CLI 的报告与已存档 v1 真实数据报告逐字节比较，保证新增模式没有改变旧结果。

## 光谱前向积分与有界反演

```powershell
& '<build>/app/spk_material_fit.exe' --spectral `
  --input '<repo>/engine/resources_research/scan/ProImage100_datasheet_curves.json' `
  --dyes '<repo>/engine/resources_research/scan/ProImage100_dye_density.json' `
  --responsivity '<单独取得并确认使用权限的仪器响应 JSON>' `
  --sensitivity '<repo>/engine/resources_research/scan/ProImage100_spectral_sensitivity.json' `
  --output '<不存在的新文件路径>/proimage100-spectral.json'
```

仪器响应必须由调用者显式提供，不随程序或材料包分发。输入结构为 `source`、递增的 `wavelength_nm` 和 `responsivity_linear_peak1.{red,green,blue}`，不是从名称推断仪器类型或许可。研究所用外部表的固定版本和哈希见验证记录，不能将第三方标准表套用数字化胶片数据的 CC BY 4.0 许可。

算法逐波长计算 `D_RGB = -log10(sum(w_RGB * 10^(-base - dyes·amounts))/sum(w_RGB))`，使用稳定 log-sum-exp、解析 Jacobian、四起点阻尼有界求解，层量范围 0–8。不采用固定 RGB→CMY 矩阵。基底/染料必须同一 1 nm 网格；仪器权重只在其已提供支持内线性插值，在胶片支持内归一化。报告明确记录被排除的仪器权重面积及积分规则；不会补造光谱尾部、裁掉负的代理染料样本、给原始密度加 offset 或移动曝光轴。

输出 **SpektraLab.SpectralLift v1**，保存每个曝光点的目标/预测密度、层量、残差、局部 Jacobian 无穷范数条件数和边界命中。每行需最大残差 ≤0.02 D 且条件数 ≤10000；任何行失败返回 3 并保留全部诊断。即使数值全过，`profile_ready` 仍为 false：仪器重建闭合不等于真实胶片曝光/感光度标定，更不等于独立实拍验证。缺失感光度样本仍是未知，不当成零。

当前 Pro Image 100 实测输入：201 行中 **12 行未过门**（低曝光 −3.14 至 −2.92 log lux-seconds），最大 R/G/B 误差分别为 **0.0287451 / 0.00184634 / 0.000164917 D**。失败行 cyan 层量命中下界；25 个原始红密度低于本模型基底密度。它们提示数据/代理模型的低曝光不一致，不能以删除暗部、允许负层量或改基底掩盖。Status M 红响应 700 nm 以外在实际 1 nm 积分网格占 **0.487508%**；10 nm 原表求和约 0.28%，不是同一个积分口径。染料来源仍是代理模型，感光度绝对曝光锚点未校准，因此没有发布新的 Pro Image Profile。

回归 `spk_spectral_lift` 包含独立 long-double Beer–Lambert oracle、64 组已知 CMY 恢复、有限差分 Jacobian、权重缩放不变性、负代理样本不裁剪、秩亏/不可达密度拒绝、积分支持面积、缺失感光度、错误/超界数据、CLI 来源哈希与拒绝覆盖。`--linear-counterfactual` 故意用线性近似替代反演，必须红灯，证明测试能发现“用矩阵冒充光谱反演”。不修改现有运行时材料和物理算法。

### 独立可达性证书（可选 v2）

在上述 `--spectral` 命令加 `--certify-bounds`，输出 v2 报告；不加时 v1 报告字节保持不变。

对每个波长，非负层量盒子 0–8 内的最小密度为 `base + 8 * sum(min(dye_k,0))`。允许每个波长独立选量是原问题的放松，所以按此光谱算出的仪器密度是严格的必要下界，不依赖优化器或起点。低于它超过门限的目标无法由当前数据模型重建；高于下界则不能据此判为可行。

真实 Pro Image 数据下界 R/G/B 为 0.27350494 / 0.67705062 / 0.89314544 D。**25 个红目标低于下界，其中 9 个低于下界超过 0.02 D**；这 9 个点已可独立证明不可达（另 3 个数值失败仍仅属拟合诊断）。没有把“12 个拟合失败”偷换成“12 个数学证明失败”。

核对 Kodak E-4L（July 1997）原页 3 后，已确认感光度曲线的能量曝光单位为 `erg/cm²`，1 erg/cm² = 0.001 J/m²，条件为 1/50 秒、Status M 高于 D-min 0.2 D；特性曲线横轴是 lux·seconds。v2 对固定源哈希附原页来源 URL/哈希和单位审计；不改源 JSON，也不自动宣称两轴已完成光度/辐射度锚定。PDF 作者为 Kodak，镜像不是许可变更，不随程序分发。

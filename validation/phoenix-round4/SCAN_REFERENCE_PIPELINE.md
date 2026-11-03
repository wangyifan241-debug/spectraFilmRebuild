# 第四轮标准化扫描参考路径

状态：**Idealized proxy / Not Verified**。不是 Fuji SP3000 或 Noritsu 的仿真，也不是测量得到的 Phoenix 校准。与 Optical Print 独立并存，不修改任何材料或默认模式，不使用商业 LUT。

## 可运行实现

`windows_UI/phoenix_round4` 构建独立 Windows Qt/Vulkan 程序。胶片开发继续调用原有 GPU film pipeline；扫描的 Beer–Lambert 积分、曲线反转和矩阵应用也在 GPU 上完成。CPU 只做小规模曲线/矩阵 setup、数据审计和应用已有的文件/显示转换，没有 CPU 胶片渲染回退。

另有 `scan-reference.cjs` 作为可独立调用的最小积分参考。它输出仪器空间正片代理，不输出 XYZ，不是 Windows 候选的完整实现。`inspect.cjs` 独立用 float64 指数积分核对 GPU 输出；用线性近似替代指数的反例必须失败。

## 明确的计算约定

1. **密度/透射**：保存原 profile 的带符号数学染料基函数与开发层系数；D(λ)=D_base(λ)+Σc_k D_k(λ)，T(λ)=10^-D。不能逐染料裁负项来声称修复物理。81 点，380–780 nm、5 nm；不改变引擎材料采样结构。
2. **扫描仪器代理**：引擎已有 D50 SPD；三个 Gaussian 探测器，中心 610/540/450 nm，σ=30/30/25 nm。逐通道以无片入射积分归一。它们不是 CIE XYZ CMF，也不冒称实际 CCD 响应。各波长权重完整写入 recipe。缺失染料或底基样本的波长在整个积分中排除，并记录数量；不会把缺测当作新测量。
3. **底基处理**：以模型已知底基在指数前移除，得到积分 Σ w_j(λ)10^(-Σc_kD_k)。这是假想的逐波长数学扣除，真实 RGB 扫描器无法从三通道文件直接完成。它与“扫描后 RGB 减常数”不同，不能宣称是硬件能力。
4. **反转/归一化**：通过同一 film pipeline 渲染 129 个固定中性输入，0.184×2^EV，EV∈[−6,+6]；以每个探测器的 logT 构建严格单调逆曲线，映射至 log10 正片。非单调或近退化时拒绝，不排序修补。超域采用端点钳制并报告通道比例。不是从当前照片做自动 min/max 拉伸。
5. **中性及空间转换**：在固定合成中性周围做三个 +0.01 线性 ProPhoto 扰动，求“场景→光谱胶片→代理仪器→中性逆曲线”的局部 Jacobian；取逆并按行归一以保留中性。条件数上限 10000，拒绝非有限/退化矩阵。这是模型内仪器转换，**没有用真实照片或真实胶片拟合材料**。保存矩阵与条件数；局部矩阵不是任意 RGB 调色，也无法保证全色域/极端曝光的准确性。
6. **XYZ / ProPhoto / sRGB**：反转结果经过上述合成转换后定义为线性 ProPhoto 代理正片；引擎实际 ProPhoto→XYZ D50 矩阵保存于 recipe，用于 D50 Lab 软件差异。不可把探测器的三个原始积分直接贴 XYZ 标签。独立的 direct-negative 对照保留原引擎 D50/CIE 观察者扫描。
7. **高光/暗部/ICC**：候选亮度 Y≤0.184 保持不变；其上用 Y'=0.184+0.816(1−exp(−(Y−0.184)/0.816))，按共同亮度比例缩放，随后 ProPhoto TRC/16-bit 量化；负值保留至显示量化前。Optical Print 不套这个 shoulder。两者再走应用已有 OutputTransform（CAT02→目标 CAM16→sRGB TRC）和同一量化/ICC 写出。保存 shoulder 前 float 与裁切比例；显示输出不是无限动态范围。TIFF16 与 PNG16 精确比较，不用截图做色差。

此实现可以隔离相纸、补偿、扫描与锐化对外观的贡献；不能恢复唯一场景光谱或解决材料代理的欠定性。默认提供预览分辨率的独立实验结果。原生尺寸、实际扫描硬件、白点/仪器绝对标定及跨场景实测泛化仍未验收。

## 标准与来源

色度计算与硬件响应分开：CIE 的 [1931 2° 色匹配函数](https://cie.co.at/datatable/cie-1931-colour-matching-functions-2-degree-observer) 和 [D50 数据](https://cie.co.at/datatable/cie-standard-illuminant-d50) 定义标准观察条件，不是某型号扫描器的测量。引擎本轮复用既有 baked 数据，未下载替换资源。ICC.1 的 D50 PCS/1931 观察者约定见 [ICC Colorimetry Data](https://registry.color.org/colorimetry-data/)；sRGB 文件标签不等于把 D65 数值直接当成 D50 Lab。现有引擎颜色转换负责白点适应；Lab 比较使用 recipe 明确的 D50 XYZ。

真实扫描器路径未来需 SPD、探测器响应、暗电流/线性化、几何和测量误差、底片密度及底基测量。缺失这些时，本候选一直保留 proxy 名称。

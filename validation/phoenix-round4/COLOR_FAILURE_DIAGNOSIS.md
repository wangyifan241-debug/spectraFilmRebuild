# Phoenix 第四轮：色彩故障诊断

日期：2026-11-03。结论分为运行状态审计、软件链路实验和真实胶片验证；后者仍为 **Not Verified**。没有修改任何 Phoenix v1 或其他胶卷材料，没有开展真实参数拟合。

## 运行版本与复现身份

实际运行的 `SpektraLabPhoenixPreview.exe` 来自 `PhoenixPreview-0.1-local`。EXE SHA256 为 `f6dc75ff50d0262ae8f82f5181bdaf6e8215392be1c726a0b89155329e60046d`；其构建回执的源提交为 `03c214f41ebe7cea30e0340fd8843aae242b779d`。`evidence/running-package-audit.json` 保存实际 EXE、资源绝对路径、资源逐文件 SHA256、已保存设置及其哈希；23 个实验材料与随包 manifest 的哈希匹配。核心编译在 EXE 内，不存在 Python/CPU 渲染后端回退。

真实窗口只读检查显示：当前照片为 `DSC02443.ARW`，选中 **phx_200_dye_nonnegative_r3**，印相模式 **Frozen Print**，RAW 为兼容模式。这不是 Phoenix Experimental v1。不能把当前候选的显示直接归为 v1 的表现。

已发布 GUI 没有导出实时 `spk_get_params` 的入口。本次没有读取进程内存或把保存状态冒称实时 resolved 参数；该项明确为 Not Observable。第四轮独立实验逐配方导出引擎 `read_params` 的全部 wire 值、使用到的非 wire 扫描/补偿值，以及实际印相 gain。它们是新实验的 resolved 参数，不是旧窗口内存快照。

## 本轮固定实验

独立 `spk_phoenix_round4` 在 Vulkan 上运行。使用两代原始 v1、原 Endura、固定解码、关闭自动曝光/颗粒/光晕/随机 glare，未进行白平衡或材料重拟合。固定输入包含：

- 合成线性 ProPhoto 中性曝光序列及三个有偏通道的序列，−6…+6 EV。它不是实测色卡。
- 当前窗口同一张 Sony RAW：`DSC02443.ARW`。使用 LibRaw compatible16/as-shot WB/AHD/linear ProPhoto，统一 stride 取样到不超过 1024 长边；完整文件哈希、原尺寸和取样步幅写入报告。结果是预览尺寸的 TIFF16，不能标成原生分辨率验收。

每代保存光学印相、第四轮代理扫描、无 unsharp、无黑白扫描校正、无输出 CAM16 压缩、直接负片透射、关闭自动印相补偿七种结果。关闭补偿的反事实不是 Fixed Internal Gain：没有注入第三轮父 gain，名称也明确区分。没有替换默认印相。

阶段保存：输入线性 ProPhoto、film prefix、各 film stage、开发后层量、纸曝光前光谱积分、纸曝光/密度、扫描 XYZ、校正后 XYZ、线性工作 RGB、gamut 前后、scanner blur、unsharp、编码输出，以及代理扫描 logT/反转/线性 ProPhoto。每个缓冲有尺寸、float32 SHA256、有限性和通道分位数。层量不是光谱密度；D(λ)=base(λ)+Σc_k dye_k(λ)，T=10^-D 由保存的模型/层量重建。光谱 oracle 采样检查不冒称所有像素的物理边界证明。

## 根因证据与分类

| 证据 | 分类及范围 | 不能推导的结论 |
|---|---|---|
| 当前窗口选中 R3 去负项候选而非 v1 | 复现身份差异，必须先固定材料和配方 | 不能据此判断某个材料更真实 |
| Phoenix 元数据明确保留 Gold 染料代理、估计底基、曝光锚点及 II 代际来源疑点 | 材料证据不足；不是测量能力的确认 | 不得靠输出调色宣布模型准确 |
| `node_scan_spectral` 是透射积分；负片 `node_bw_correction` 旁路 | 直接负片扫描不是标准化正片反转 | 不能把其外观当作正片色彩失败 |
| `refresh_print_constants` 每次印相重算 gain，受曝光及材料条件影响 | 自动印相适应与负片本体须分离 | 固定用户 print_exposure 不等于固定内部 gain |
| 第三轮受控实验：旧 1729 事件中 1720 在固定 gain 后消失，9 保留，另新增 23 | 历史限定配方下的因果干预证据 | 不等于所有偏色源于 gain；不推荐直接采用 Fixed 模式 |
| 第三轮 78 项残余记录在 unsharp 后首次下降，研究旁路后消失 | 合成块空间布局与锐化的交互；旧实验历史，不计入本轮测试数 | 不外推真实扫描器或全部照片 |
| 第四轮同一负片分支可得到不同印相/扫描结果 | 输出链路对外观具有实质影响 | 软件间差异不能充当真实胶片色差 |
| 未插桩 C ABI 对照与本轮逐节点印相读取比较 | 防止诊断改变被诊断行为，结果见 report 的 uninstrumented-control | 不证明被诊断模型物理正确 |

第三轮依据来自另一工作树的 `validation/phoenix-round3/FINAL_REPORT.md` 与 `SCAN_PRINT_CHAIN.md`；原文件和旧 PR 没有更新。源文件哈希见 `evidence/third-round-source-identities.json`。第三轮数字仅引用历史证据，不伪装成本轮重跑。

## 颜色契约审查

本轮输入明确是 **线性 ProPhoto**，不再次解码 TRC；层量/光谱阶段不贴 RGB 标签。Optical Print 保持编码 ProPhoto 工作输出，再使用应用已有 `OutputTransform`（CAT02、CAM16、目标 TRC）到 sRGB。候选正片在线性 ProPhoto 做明示的亮度 shoulder，随后编码/量化并走同一输出转换。PNG 与 TIFF16 使用同一最终 RGB16，TIFF 嵌入实际 sRGB ICC。最初实验直接让引擎输出 sRGB，与应用工作空间路径不同；该实验保留在本地 early 目录，最终证据统一路径重跑。

没有发现并修复某个已证实的通道交换、重复 TRC 或错误 ICC 标签缺陷。负片与 Cineon/正片的解释差异属于需要防止的契约错误，不应写成已发现产品发生了此错误。监视器 ICC/校准没有测量，截图不用于 ΔE00。

## 当前可确认与剩余边界

本轮提供可复现输入、分阶段数据、输出消融和独立扫描预览；不是“Phoenix 颜色已恢复准确”的报告。合成中性误差、独立 Beer–Lambert 残差、D50 Lab 软件差异、曝光端点裁切比例见 evidence 数值报告。它们不设真实胶片验收分数。

缺少可靠真实配对底片、实际扫描器 SPD/响应及独立实测色卡；真实 ΔE00 等指标为 null。当前实拍照片也没有同场景真实 Phoenix 底片，不能选择“看起来更好”的一张来拟合。实时旧窗口完整参数仍不可直接观察；标准化色卡实际拍摄和曝光组采集待完成。macOS/Metal、原生尺寸全链路、显示器色彩验收未执行。

## 本轮数值解释补充

EV0 下，关闭自动印相补偿和关闭当前默认已关闭的黑白校正可出现零输出差。这不是它们在任何设置下都没有影响；EV0 的补偿探针与中灰参考重合。为避免只在守卫不起作用的配置下测试，另跑 −2/0/+2 EV：一代补偿 gain 在 −2 EV 为 0.1082684107，关闭补偿为 0.4084870356；二代独立 gain 见 exposure-bracket.json。两代三曝光的扫描配方哈希分别恒定，未针对曝光重做中性转换。

RAW 的 no-unsharp、no-gamut 与扫描候选差异见 raw-ablations.json，指标明确为编码 RGB 软件差异。没有用这个差异量决定材料胜负。代理扫描曝光端点钳制、负值和 >1 比例独立保留；每代 64 个实际层量采样通过独立光谱指数核对，但不把这个采样结果外推为所有波长/所有层量域保证。

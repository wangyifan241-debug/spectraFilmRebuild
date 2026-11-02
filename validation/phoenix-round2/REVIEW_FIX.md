# PR7 审查修复：色彩测量与曝光定位

本记录取代此前报告中的sRGB测量解释和“未定位”状态。原审核提交6eb54e9d的相关JSON/脚本/报告保存在history/reviewed-6eb54e9d；其余旧图可在该Git提交查阅。旧图像标签与Lab/Y无效，不能与新结果混合统计。产品材料无任何改动。

## P0 原因与修复

实现链：engine.cpp的spk_open先将输出设为ProPhoto RGB且编码，再应用delta；pipeline.cpp node_scan_spectral输出XYZ，经node_xyz_to_rgb转换至所选原色，再由node_cctf派发spk_cctf_encode_matrix.comp（ProPhoto模式1）。spk_to_rgba16.comp只clamp到[0,1]、四舍五入到uint16，并未转sRGB。render_fixture.cpp逐行拷贝结果，不做色彩管理。故输出是编码ProPhoto/D50，实际resolved参数亦一致；不是仅按名称猜测。

旧exposure24.cjs错用sRGB逆函数和sRGB-D50矩阵，导致Lab/Y/ΔE00错误。旧analyze.cjs把ProPhoto数值直接量化、嵌sRGB标签，图像显示也错误。color.cjs统一解码：encoded<16/512时L=encoded/16，否则L=encoded^1.8。由ProPhoto原色矩阵乘线性RGB到D50 XYZ，不再进行额外白点适应。Lab采用既有D50/2°白点。显示PNG另经过D50 XYZ→sRGB、sRGB编码与明确裁切。旧output-differences中的encoded RGB仅为工作空间数值，最新结果也明确不是色度误差。EV图片比较同时修正为同EV父材料，而不是不同输入的基础fixture。

原sRGB→linear ProPhoto的近似矩阵经独立组合复核总体正确，精度为6位小数，并非该P0根因。现改为先sRGB解码→Bradford D50 XYZ→逆ProPhoto矩阵；测试覆盖三原色和灰阶。输入哈希随精度微调变化，已在新exposure24-results记录，旧哈希在历史文件保留。

参考：[ICC ROMM登记](https://registry.color.org/rgb-registry/rommrgb)、[Android原色及传递函数文档](https://developer.android.google.cn/reference/android/graphics/ColorSpace.Named)、[colour ProPhoto数据集](https://www.colour-science.org/api/0.3.4/html/colour.models.dataset.prophoto_rgb.html)，查阅2026-11-02。数值核对同时以仓库colour.cpp及GPU内核实际分段为准。

6项色彩/观测集成测试通过，包含主色XYZ、D50白/黑、gamma/线性toe、sRGB输入组合及故意错误空间负对照。历史首次测试中灰阶期望值抄录错误导致1项失败，更正为D50×0.21404114后独立向量5/5，另加全观测与历史错误路径集成检查后6/6；原失败TAP保留，不隐藏。

## 干净构建与执行范围

源码提交6eb54e9dcbc8d1bd9fc04bfcff13e841547959c7，未改engine源码；engine树及compiler/驱动/DLL/SPIRV哈希见clean-build-provenance.json。MinGW GCC13.1.0；CMake3.25.0；Vulkan-Headers提交记录在JSON；glslang来自已安装固定工具路径。build/review-clean为本次新建目录，从配置开始构建spk_render_fixture及共享引擎，无预构建engine对象。原始configure/build日志已保存。未声称Qt、LibRaw、macOS或全应用从源码重建。

实际构建命令（PATH先包含MinGW bin）：

```powershell
cmake -S . -B build/review-clean -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DSPEKTRALAB_REQUIRE_VULKAN=ON -DSPEKTRALAB_BUILD_DESKTOP=OFF -DSPEKTRALAB_BUILD_NATIVE_RAW=OFF -DSPEKTRALAB_VULKAN_HEADERS=C:/Users/Strickland/SpektraLab-deps-20261028/Vulkan-Headers/include -DSPEKTRALAB_GLSLANG=C:/Users/Strickland/SpektraLab-deps-20261028/glslang-build/StandAlone/glslangValidator.exe
cmake --build build/review-clean --target spk_render_fixture -j 6
node --test validation/phoenix-round2/color-tests.cjs
node validation/phoenix-round2/exposure24.cjs
node validation/phoenix-round2/run-native.cjs
node validation/phoenix-round2/build-trace.cjs
node validation/phoenix-round2/trace.cjs
node validation/phoenix-round2/check-spectral.cjs
node validation/phoenix-round2/review-summary.cjs
node validation/phoenix-round2/summarize.cjs
node validation/phoenix-round2/result-index.cjs
node validation/phoenix-round2/analyze.cjs
node --test tools/phoenix-validation/tests.cjs
node validation/phoenix-round2/audit.cjs
node tools/phoenix-validation/cli.cjs verify
```

测试副本插桩读取GPU缓冲后打印阶段值，单独替换pipeline对象并链接独立trace DLL，build-trace.cjs保留构建过程；没有向产品engine添加开关或改变数学。trace必须gpu.read而不能假设Vulkan设备内存始终可由contents映射。初次contents尝试没有捕获数据，未作为证据；最终52记录包含阶段值和573个积分常数。

## 重跑结果与分阶段原因

11款×13档EV×24块全部重算：143组成功，逐块原始RGB、Y、Lab和父子ΔE00见exposure24-results。exposure-anomalies.json列1729个跨EV下降事件（包含彩色块，不全是灰阶）、暗部/中间调/高光分组、前后EV/Y/L*/RGB和变化量/是否输出触边。组别按区间中点EV≤-2、≥2或其间分类，避免用错误测量先决定样本类别。阈值ΔY<-1e-4，全部灰阶失败明细见exposure-summary。

两代v1分别DIR开/关、13档、恒定线性0.18中性像素，共52组阶段实验。film_log_and_curves后各通道为非负、增加的CMY密度；DIR改变很小，关闭仍复现。最早的曝光顺序异常在node_enlarger_spectral已可见：I代DIR开-6→-5 EV时扫描XYZ的Y从0.196817830降为0.193322599；DIR关降幅0.003496721，与DIR开0.003495231接近。后续node_print_exposure施加固定同一增益；node_print_curves通道出现混合方向；node_scan_spectral已出现下降，故晚于该阶段的XYZ→RGB、gamut、CCTF及uint16裁切不是这一区间异常的起点。

进一步检查继承的Gold染料基函数含负项（最小约-0.0469235）。即使三通道CMY分别增加，总密度D(lambda)=base+ΣCMY*dye也能在某些波长下降；该I代区间有3个波长下降。透射10^-D因此会上升，纸张多通道响应和最终Y不保证单调。check-spectral.cjs利用实际读取的print_chd/base/illum_x_sens/gain/offset，用独立CPU双精度标量循环重算光谱积分。52/52与GPU一致，最大log exposure误差1.4857e-7。完整波长密度、CPU/GPU与阶段值保存在spectral-independent-check.json和stage-trace.json。

这是当前代理基函数及重建/印相组合的数学行为，不是本轮发现的GPU积分实现错误，也不意味着所有色偏/非单调已有真实胶片物理解释。负的基函数可来自拟合/基底表示，不能仅凭负项断言原始Gold测量无效；将它直接用于Phoenix的物理适用性未验证。本轮不改变染料或曲线，不用“负片本来如此”解释。52组定位只针对两代v1的恒定灰；九个候选的全部色块异常不能未经逐项阶段跟踪一概归因于此机制。

## 审核结论与剩余问题

所有候选仍Inconclusive，产品Retain Baseline；纠正数值不等于提高真实胶片精度。旧图及旧Lab/Y不得作为采纳证据。

- P0：此次色彩测量错误已修复并重算；尚无内部证据支持解除产品采纳阻止，II厂家测量真实性及真实胶片验证仍未完成。
- P1：模型暗部非单调仍存在，基线特定灰区间已定位到代理光谱/印相阶段；候选全色块异常尚未逐项归因。新重建覆盖原生引擎/驱动，不覆盖Qt全应用、RAW decoder和macOS。量化输出不能替代全GPU节点NaN扫描。
- P2：本机工具路径与旧历史报告状态；历史结论以本修复记录取代。raw-*旧32px图是错误尺寸/标签的派生产物，已从当前图片清单移除，未作为RAW真实性证据。

本次不改材料、默认色彩或PR4/6/8，保持Draft等待再次审查。

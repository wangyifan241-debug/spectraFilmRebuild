# Phoenix 第四轮实验

**Not Verified：软件比较，不是真实胶片校准。** 两代 Experimental v1 和其他材料字节未修改。Optical Print 保留，第四轮扫描为独立候选。旧研究 PR 不更新、不合并。

文档：[色彩诊断](COLOR_FAILURE_DIAGNOSIS.md)、[扫描路径](SCAN_REFERENCE_PIPELINE.md)、[真实数据采集](DATA_COLLECTION.md)。`dataset.json` 是空集，拟合禁用。

## 构建与运行

在 Windows PowerShell 中，使用已有项目依赖（Qt 6.11.3/MinGW/Vulkan/LibRaw）：

```powershell
./windows_UI/phoenix_round4/build.ps1 -DependencyDirectory '<deps>' -BuildDirectory '<new-build>'
& '<new-build>/app/spk_phoenix_round4.exe' --render '<new-build>/app/resources' '<photo.ARW>' '<new-output>'
& '<new-build>/app/spk_phoenix_round4.exe' --view '<new-output>'
```

无 Python/Node 应用运行时依赖。`--render` 实际在 Vulkan 上做胶片和光谱扫描；输出目录必须不存在，源图像不写入、不读取旧编辑存储。输出默认为最长边不超过 1024 的固定 stride 预览，TIFF16 也保持这个尺寸；`--render-full` 是原生尺寸实验入口，本轮未验收，数据量明显增加。

也可把 RAW 参数换成 `synthetic`，生成统一中性/偏色曝光序列。合成数据不进入真实数据集。环境变量 `PHOENIX_ROUND4_EXPOSURE_EV`（−8…+8，缺省 0）仅用于明确的胶片曝光组实验；每次值记录在 provenance。扫描校准始终固定在 EV0，不随照片/曝光重拟合。

查看器左/右分别选择任一代 v1 Optical Print、扫描候选或消融结果；缺省为 Phoenix 200 的 Optical Print/扫描候选。滑块放大，拖动画面查看局部。同名 PNG/TIFF16 使用同一 RGB16 像素，TIFF 携带 sRGB ICC。数据目录保留 float32 阶段缓冲、完整 wire/研究参数、扫描 recipe、输出哈希与 build receipt 绑定。它是只读研究查看器，不替代正式应用的编辑/原生尺寸导出验收。

## 验证

```powershell
node --test validation/phoenix-round4/tests.cjs
node validation/phoenix-round4/scan-reference.cjs --test
node validation/phoenix-round4/inspect.cjs '<output>'
node validation/phoenix-round4/verify-exports.cjs '<output>'
node validation/phoenix-round4/analyze-ablations.cjs '<output>'
node validation/phoenix-round4/dataset.cjs validation/phoenix-round4/dataset.json --require-fit # 预期拒绝空集
ctest --test-dir '<new-build>' -R spk_round4 --output-on-failure
```

43 项 Windows CTest 全套通过；最后的曝光组工具改动后，3 项第四轮目标门再次通过。数据泄漏检查 8/8；原有验证基础设施 51/51。真实 RAW 和合成图各 14 对 PNG/TIFF16 精确对照；色彩验证与性能基准分开，没有速度提升声明。详见 [TEST_EVIDENCE.md](TEST_EVIDENCE.md)。

`inspect` 的 D50 Lab/ΔE00 是合成软件自洽指标。真实胶片 metrics=null。无真实底片、不做 Phase 4 拟合。实际扫描器 SPD/响应、独立场景配对、监视器 ICC、旧 GUI 实时 resolved 参数与 macOS/Metal 均仍有未验证项。

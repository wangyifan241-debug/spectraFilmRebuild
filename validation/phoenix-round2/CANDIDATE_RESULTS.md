# 逐候选差异及执行索引

所有数值为固定渲染链的模型间差异；不是与真实胶片比较。parent字节SHA256见candidate-audit.json。所有候选0.2.0，父v1为0.1.0。

| ID | 核心差异 | EV0 mean/max 模型间ΔE00 | 全曝光灰阶下降次数 | 决策 |
|---|---|---|---|---|
| phx_200_d55_r2 | data.log_sensitivity | 2.3719 / 12.9342 | 36 | Inconclusive |
| phx_200_linear_r2 | data.log_sensitivity | 5.3840 / 27.6429 | 36 | Inconclusive |
| phx_200_no_tungsten_r2 | data.log_sensitivity | 0.9812 / 7.9204 | 36 | Inconclusive |
| phx_200_shared_r2 | data.log_sensitivity | 13.5730 / 36.4045 | 39 | Inconclusive |
| phx_ii_d55_r2 | data.log_sensitivity | 0.2224 / 0.5770 | 42 | Inconclusive |
| phx_ii_linear_r2 | data.log_sensitivity | 1.8201 / 8.2001 | 42 | Inconclusive |
| phx_ii_shared_r2 | data.log_sensitivity | 7.5134 / 16.5057 | 45 | Inconclusive |
| phx_ii_width115_r2 | data.log_exposure, data.density_curves_model.centers, data.density_curves_model.sigmas | 1.2758 / 2.0151 | 44 | Inconclusive |
| phx_ii_width130_r2 | data.log_exposure, data.density_curves_model.centers, data.density_curves_model.sigmas | 2.4499 / 3.7199 | 44 | Inconclusive |

每个候选对应的plan含源记录、父材料SHA256和候选文件SHA256。每个候选均有13档24块Lab/RGB/Y及原生驱动报告；所有九款曲线非负有限且密度单调。shared仅修改通道整体比例；linear同时改变纵轴解释下的插值含义（同一类别）；no_tungsten仅取消一代反除；d55仅换归一化权重；width仅变响应宽度。所有未列出的数据字段与父材料一致，由PR6单类别审核验证。证据和公式见SOURCE_REVIEW_FINAL.md。

## 实际命令与凭据

工作目录为第二轮工作树。原生依赖固定本机构建路径；需Qt MinGW bin在PATH。

```powershell
node validation/phoenix-round2/generate.cjs
node validation/phoenix-round2/generate-curves.cjs
node validation/phoenix-round2/generate-d55.cjs
node validation/phoenix-round2/audit.cjs
node --test tools/phoenix-validation/tests.cjs
node tools/phoenix-validation/cli.cjs verify
node engine/resources_research/verify-phoenix.cjs
node validation/phoenix-round2/run-native.cjs
node validation/phoenix-round2/exposure24.cjs
node validation/phoenix-round2/run-raw.cjs
$env:QT_PLUGIN_PATH="D:/学习资料/SpektraLab/build/phoenix-20261102/app"
node validation/phoenix-round2/preview-export.cjs
& D:/学习资料/SpektraLab/build/phoenix-20261102/spk_qt_frame_output_test.exe build/round2/resources
& D:/学习资料/SpektraLab/build/phoenix-20261102/spk_qt_review_colour_test.exe build/round2/resources
node validation/phoenix-round2/analyze.cjs
node validation/phoenix-round2/summarize.cjs
```

run-raw.cjs依赖已解码固定输入，其生成命令（已实际执行）为：

```powershell
& D:/学习资料/SpektraLab/build/phoenix-20261102/engine/spk_raw_render.exe --input D:/学习资料/SpektraLab/build/export-repro-1/owned.ARW --output build/round2/fixed-raw.tif --report build/round2/fixed-raw.json --resources build/round2/resources --params build/round2/harman_phoenix_200_exp_v1-params.json --decoded-output build/round2/fixed-raw.f32
```

- PR6基础设施39/39；冻结检查64文件Pass；候选9/9与9个错误类别负对照拒绝；v1 Git字节不变。
- research3254 checks；PreviewController/导出/持久化/Portra恢复18014 checks。FrameOutput和linear EV/neutral identity回归各exit0。
- 原生11款基本fixture、143组曝光和11款RAW采样执行成功。过程成功不抹除亮度单调性Fail。
- 首次导出缺插件Fail，设置QT_PLUGIN_PATH后重跑Pass；结果记录分别见preview-export-execution.txt、frame-output-regression.txt、colour-regression.txt。
- 内部GPU全中间值NaN检查、macOS/Metal及完整Xcode suite：未运行，当前Windows环境不能替代。
- 早期material-sensitivity.json中的Not Run是生成时状态；最新执行以native-execution/exposure24-results/raw-execution及FINAL_REPORT为准。

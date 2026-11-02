# 第三轮复现入口

只在独立、可丢弃checkout重算生成物；保留本提交证据，不在冻结证据目录上反复覆盖。无产品材料安装步骤。原生C++/Vulkan/Qt/LibRaw，不依赖Python参考引擎。PDF内容复核仅使用外部pypdf读取已保存副本，与产品构建无关。

## 固定依赖

源码基线00251eac，engine tree见freeze.json；编译器/驱动/DLL/SPIRV/Qt/LibRaw身份见execution-provenance.json。MinGW13.1、CMake3.25、Qt6.11.3、Vulkan头1.4.341、LibRaw0.22.2。设置PATH包含MinGW bin和CMake bin；Qt运行时还需mingw_64/bin与QT_PLUGIN_PATH。默认依赖根C:/Users/Strickland/SpektraLab-deps-20261028。现有脚本内绝对路径是已知P2；移机需改为对应位置并重新生成工具身份，不能继续使用旧哈希。

## 原生与第二轮输入准备（没有build目录时）

```powershell
cmake -S . -B build/review-clean -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DSPEKTRALAB_BUILD_DESKTOP=OFF -DSPEKTRALAB_BUILD_NATIVE_RAW=OFF -DSPEKTRALAB_REQUIRE_VULKAN=ON -DSPEKTRALAB_VULKAN_HEADERS=C:/Users/Strickland/SpektraLab-deps-20261028/Vulkan-Headers/include -DSPEKTRALAB_GLSLANG=C:/Users/Strickland/SpektraLab-deps-20261028/glslang-build/StandAlone/glslangValidator.exe
cmake --build build/review-clean --target spk_render_fixture -j 6
New-Item -ItemType Directory -Force build/round2
Copy-Item build/review-clean/engine/resources build/round2/resources -Recurse
node validation/phoenix-round2/run-native.cjs
node validation/phoenix-round2/exposure24.cjs
node validation/phoenix-round3/build-trace24.cjs
node validation/phoenix-round3/trace24.cjs
node validation/phoenix-round3/classify.cjs
node validation/phoenix-round3/decompose-compensation.cjs
node validation/phoenix-round3/observe-negative.cjs
```

trace24读取第二轮143组；跟踪前后输出字节必须一致，否则中止。phase方法与固定24块相同，不用最终RGB反推光谱。生成源码只在build目录，产品engine不变。

## 第三轮材料与双模式

```powershell
node validation/phoenix-round3/generate.cjs
node validation/phoenix-round3/run-experiments.cjs
node validation/phoenix-round3/recapture-constants.cjs
node validation/phoenix-round3/summarize-results.cjs
node validation/phoenix-round3/source-identities.cjs
node --test validation/phoenix-round3/tests.cjs
```

每个完成的材料保存checkpoint；run-experiments跳过已有26组材料。要全量重算，使用全新checkout，不把旧experiment-results/calibration-results当作新的运行。recapture提供独立重复执行并核对全部364个输出SHA。gain随EV变化，必须使用per-ev-constants.json；旧spectral-constants.json仅属失败历史，不用于验证。

12款材料plan由generate生成；observation-plan.v1.json记录C组及解析对照的身份/条件/来源与不适用印相模式的原因。没有标准响应数据就不宣称标准密度计实验完成。

## UI/ICC与软件回归

```powershell
cmake -S validation/phoenix-round3/native-ui -B build/round3-ui -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DSPEKTRALAB_DEPS_DIR=C:/Users/Strickland/SpektraLab-deps-20261028 -DSPEKTRALAB_VULKAN_HEADERS=C:/Users/Strickland/SpektraLab-deps-20261028/Vulkan-Headers/include -DSPEKTRALAB_GLSLANG=C:/Users/Strickland/SpektraLab-deps-20261028/glslang-build/StandAlone/glslangValidator.exe -DCMAKE_PREFIX_PATH=C:/Users/Strickland/SpektraLab-deps-20261028/qt-6.11.3/mingw_64
cmake --build build/round3-ui --target phoenix_round3_ui -j 6
$env:QT_PLUGIN_PATH='C:/Users/Strickland/SpektraLab-deps-20261028/qt-6.11.3/mingw_64/plugins'
build/round3-ui/phoenix_round3_ui.exe build/round3-run/resources build/round3-ui/evidence
build/round3-ui/phoenix_round3_ui.exe build/round3-run/resources build/round3-ui/icc-evidence icc-only
node --test validation/phoenix-round2/color-tests.cjs
node --test tools/phoenix-validation/tests.cjs
```

UI测试读取副本里的候选材料；未覆盖产品安装或改变原始material。原生全部组使用相同输入和配方，渲染后重复/重印一致性与结果生命周期由spk_render_fixture验证。这里没有运行macOS/Metal或真实摄影精度门。

所有报告的生成命令、失败历史与源/材料/输入/输出哈希一并保留。完成后仅创建Draft供审查，不合并、不进入第四轮。

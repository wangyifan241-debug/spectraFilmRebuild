# 第三轮研究复现与PR9审查补强

产品材料/默认算法不安装、不修改。历史9940ca3f的双模式数据保留在本目录；本次审核结果写入review-hardening，优先阅读FINAL_REPORT.md。本轮没有开展第四轮。

## 显式配置

从仓库根目录执行。复制config.example.json到自己管理的配置文件并填写工具位置，再设置PHOENIX_CONFIG。也可直接提供PHOENIX_*环境变量；环境变量优先于JSON。路径可以是仓库相对路径或显式绝对路径，脚本没有个人电脑默认目录。

```powershell
$env:PHOENIX_CONFIG='path/to/my-actual-tools.json'
# 或单项设置：
$env:PHOENIX_CXX='path/to/mingw/bin/g++.exe'
$env:PHOENIX_CLEAN_BUILD='build/review-clean/engine'
$env:PHOENIX_TRACE_BUILD='build/round3-trace'
$env:PHOENIX_FIXED_BUILD='build/round3-fixed-gain'
$env:PHOENIX_RUN_BUILD='build/round3-run'
$env:PHOENIX_RESULT_DIR='validation/phoenix-round3/review-hardening'
```

目录键见config.example.json。历史provenance生成还可配置PHOENIX_QT_CORE、PHOENIX_LIBRAW_HEADER；不是根据用户名猜测。已有审计依赖为MinGW13.1、CMake3.25、Qt6.11.3、Vulkan头1.4.341及LibRaw0.22.2，准确字节身份见旧execution-provenance和新session-identity。不同编译产物即使版本文字相同，也不能恢复原checkpoint。

已有build必须来自审核基线兼容的engine树。全新机器需按仓库README构建spk_render_fixture，并准备原24块float32输入、两代v1/候选和继承滤镜包的私有资源副本；旧README命令对应的相对目录仍是默认值。可显式设置PHOENIX_ROUND2_BUILD等键，不依赖旧报告记录的绝对路径。没有现成build时不能只下载JSON就声称重跑成功。

## 独立Fixed Internal Gain重跑

```powershell
node validation/phoenix-round3/prepare-research.cjs
node validation/phoenix-round3/build-fixed-gain.cjs
node validation/phoenix-round3/review-run.cjs
node validation/phoenix-round3/legacy-review-run.cjs
node validation/phoenix-round3/analyze-fixed-gain.cjs
node validation/phoenix-round3/build-deep-scanner.cjs
node validation/phoenix-round3/deep-scanner-review.cjs
node validation/phoenix-round3/audit-dye-basis.cjs
node --test validation/phoenix-round3/hardening-unit-tests.cjs
node --test validation/phoenix-round3/hardening-integration-tests.cjs
```

review-run是支持的恢复入口；内部fixed-gain-experiment实现不作为绕过校验的入口使用。先核对父参考、冻结参数、记录header及gain，再由身份绑定器拒绝二进制/输入/材料/资源/脚本/参数变化。无身份旧checkpoint不自动升级。legacy-review-run保留恢复前已产生的正常输出凭据；不会把复用历史渲染计作新执行。

父参考在恒定0.18、EV0、父印相包下取得。Fixed仅改变实际参与光谱印相的gain，保留Frozen所有用户参数；两代分别共用各自父gain。Recal重放审核基线已经冻结的中性灰优化参数，未再优化彩块。诊断DLL只能在专门的研究目录加载，环境开关不进入产品代码。

深层诊断另外读取模糊/锐化节点；启用研究unsharp旁路是针对残余异常的干预，不是建议产品第四种模式。旁路关闭时必须与Fixed结果逐字节一致。

## checkpoint规则

session-identity绑定编译器版本/字节、正常与诊断渲染器/DLL、输入、灰阶、材料/资源清单、脚本、原校准和参考条件。记录还要匹配冻结参数、父gain和正常输出SHA。明确拒绝无绑定旧文件、错DLL、错参数和被改写的参考gain。升级工具或实验定义必须使用新的PHOENIX_RESULT_DIR；不得复制旧header为新二进制背书。

支持恢复已有完整结果仅表示校验通过并复用，不表示本次重新执行全部渲染。首次运行若产物已存在而binding缺失，会失败。审核本次进行了首次完整执行和正/负恢复验证。

原adaptive双模式生成脚本run-experiments.cjs也已改为显式配置，在结果目录下单独的adaptive-print-replay保存绑定；不会默认读入根目录的历史结果。PHOENIX_CHECKPOINT_ONLY=1只做身份预检，不渲染。它属于旧优化器的复现入口；本次三模式重放不调用它重新调参。

## 数据及测试

- three-mode-results：546组原始阶段、常数、XYZ/Lab及SHA。
- frozen-parent-gains：两代参考条件、实际gain与SHA。
- fixed-gain-physics：由GPU实际系数/常数重建的D/T与纸材积分，不是由最终RGB反推。
- legacy-causal-pairs：原1729事件的动态/固定gain配对。
- fixed-gain-stage-events、deep-scanner-results：所有残余事件的逐阶段和旁路证据。
- 三模式详细比较在three-mode-comparison，摘要在CANDIDATE_RESULTS。
- 本次TAP和恢复日志位于review-hardening；历史UI/ICC计数不冒充本次执行。

所有材料候选缺少真实独立测量，继续Inconclusive；photographic_fidelity_validated=false，photographic_gate=Not Verified，真实精度metrics=null。校验结果不能证明真实Phoenix颜色准确。

### 空构建目录补充

先配置并构建正常原生fixture（工具目录必须由调用方给出）：

```powershell
# CMake和MinGW bin需在PATH；CMake构建目录是配置中clean_build的上一级
cmake -S . -B build/review-clean -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DSPEKTRALAB_BUILD_DESKTOP=OFF -DSPEKTRALAB_BUILD_NATIVE_RAW=OFF -DSPEKTRALAB_REQUIRE_VULKAN=ON -DSPEKTRALAB_VULKAN_HEADERS="$env:PHOENIX_VULKAN_HEADERS" -DSPEKTRALAB_GLSLANG="$env:PHOENIX_GLSLANG"
cmake --build build/review-clean --target spk_render_fixture -j 6
node validation/phoenix-round3/prepare-research.cjs
```

prepare-research从正常构建资源创建私有副本，按原固定公式生成24块/中灰float32输入，核对审核输入SHA及候选/父材料；滤镜值重建必须与research-neutral-print-filters.v1.json深相等，再使用其精确字节以保留历史键序。该表只用于研究副本，来源/许可继承原CC BY-SA滤镜表和候选父继承关系，不安装到产品。已有资源/输入若不匹配就拒绝，不覆盖。审查本次的空目录准备和旧输入拒绝均实际通过。

本目录.gitattributes保留精确文件字节，允许Windows CRLF而不自动转换；清单不包含自身。历史报告中的本机路径只作为证据保留，新脚本不据此寻址。

执行字节补充：旧color.cjs为CRLF工作区、Git保存LF。source-checkout-identities.json分别绑定两个哈希域，executed-tools保存实际执行原字节；没有把换行等价当成字节相等，也没有改写旧PR工具。bind-source-checkout.cjs可重新核对；其余会影响物理计算的脚本实际字节与所绑定身份一致。不同字节的checkout只能新建实验，不恢复旧checkpoint。

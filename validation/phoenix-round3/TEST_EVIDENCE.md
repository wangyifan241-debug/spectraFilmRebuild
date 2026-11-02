# 第三轮测试证据

本次实际执行，软件与物理参考门分别报告：

| 检查 | 实际结果 | 证据 |
|---|---|---|
| 第三轮专项 | 7/7 Pass | tests.tap |
| 修复后的色彩观测 | 6/6 Pass | color-tests.tap |
| provenance与哈希 | 2/2 Pass | provenance-tests.tap |
| PR6基础设施 | 39/39 Pass | infrastructure-tests.tap |
| 两代v1+九款第二轮候选阶段跟踪 | 143/143 RGBA16字节一致；1729事件逐项分类 | trace24.json、anomaly-classification.json |
| 第二轮色块独立光谱积分 | 3432/3432 Pass，最大log误差3.35105e-7 | independent-math.json |
| 自动补偿分解 | 1729/1729 加和成立；全部有通道方向反转 | compensation-decomposition.json |
| 第三轮两代v1+12款独立候选、两模式、13EV | 364组；8736色块独立积分/有限性完成 | experiment-results、experiment-observations、candidate-summary |
| 每EV常数重采集 | 364/364 输出SHA相同 | constants-recapture.json |
| Qt/LibRaw独立构建与候选预览/导出/重开/Portra恢复 | 22925 checks Pass，exit0 | ui-build.log、ui-execution.log |
| 实际嵌入ICC的正确/故意错误标签对照 | Pass，exit0 | ui-icc.log |
| 灰阶最终正片Y单调性 | Fail；各模式36–43个下降事件，未隐藏 | candidate-summary.json |
| 将原Gold染料解释为独立非负吸收 | Fail（已知负项）；不是原测量失效证明 | math-results.json |
| 当前官网PDF重新获取 | 三次403；不能声称取得最新版 | source-downloads.json |

原生输出保存的是编码ProPhoto值；JSON观测由正确D50 XYZ/Lab计算。输出端点触边比例与打包前真正越界比例分别报告，样本仅24个测点，不是全帧统计。两模式打包前越界比例在本测试为0，不能据此推断任意图像无裁切。

历史失败保留：ui-configure-initial-failure.log（Qt父目录scope）、ui-build-initial-failure.log（独立目标未开AUTOMOC）；修复后独立构建通过。history/initial-constants-overwrite.json记录首次汇总因EV常数覆盖而失败；已修复为逐EV绑定，所有组重采集且输出SHA相同。

尚未运行：macOS/Metal/Xcode、真实胶片配对、全尺寸RAW/空间细节比较、扫描器硬件标定、全帧全GPU节点有限性。色块肤色/蓝天/绿植/高光均为合成RGB近似，不冒充实测反射率或真实照片。Qt fixture另含合成灰阶、肤色/彩色及高光小区域。

复现顺序和依赖见README.md。执行工具/源/二进制身份见execution-provenance.json与evidence-manifest.json；清单不包含自身哈希。

全部8736色块另完成固定D50理想负片观察与三组归一合成密度计积分，见candidate-negative-observers.json；不执行反转，不将其当成光学印相正片。

## PR9 最终审查补强：本次重新运行

审核基线9940ca3f。以下为本次执行，不沿用旧UI suite的Pass计数。

| 本次验证 | 通过 | 失败/限制 | 原始证据 |
|---|---|---|---|
| 补强单元测试 | 6/6 | 0；含旧checkpoint、二进制/输入/参数/脚本不匹配、gain篡改拒绝和基函数标度不变性 | review-hardening/unit-tests.tap |
| 补强集成测试 | 8/8 | 0；含真实启动器错DLL/参数拒绝和adaptive旧checkpoint拒绝 | integration-tests.tap |
| 原第三轮测试 | 7/7 | 0 | baseline-tests.tap |
| 色彩观测 | 6/6 | 0 | color-tests.tap |
| PR6基础设施 | 39/39 | 0 | infrastructure-tests.tap |
| provenance | 2/2 | 0 | provenance-tests.tap |
| 三模式渲染 | 546组完成 | Frozen/Recal内部gain仍动态；Fixed仅为因果诊断 | three-mode-results.json |
| 正常算法输出未变 | 364/364 SHA相同，旧材料另11/11 EV0相同 | 不外推为全部真实RAW或macOS通过 | three-mode-comparison.json、legacy-fixed-gain.json |
| 原1729事件固定gain对照 | 143组完成 | 1720消失，9仍存；另新增23项 | legacy-causal-pairs.json |
| 独立光谱积分 | 16536/16536采样通过，最大log误差3.79075e-7 | 24点采样，不是全帧全GPU有限性 | three-mode-comparison.json |
| 负片阶段隔离 | 4368/4368层密度与Frozen相同 | 不能证明真实染料测量正确 | three-mode-comparison.json |
| 深层扫描跟踪 | 54/54固定gain帧SHA相同 | 两域78项事件记录有重叠，不能当作78个独立场景 | deep-scanner-results.json |
| 研究unsharp旁路 | 78/78记录的下降消失 | 不修改或推荐产品旁路；不是第四种产品印相模式 | deep-scanner-results.json |

本次JS测试共68/68通过，0失败。负对照的预期拒绝计为测试通过，不冒充正常渲染成功。原始条件仍有失败：第三轮Frozen/Recal/Fixed分别2168/2159/46个全色块Y下降事件（其中灰阶548/546/8）；Gold基函数非负性不满足。两类失败与软件测试分开报告。

本次没有重跑Qt/LibRaw预览导出22925项、macOS/Metal、真实胶片对照；此前Qt/ICC结果属于审核基线历史，不贡献本次测试通过数。产品engine/windows_UI源码和材料对9940ca3f无差异。

恢复测试：review-run.cjs先验证checkpoint记录与冻结参数、输出、父gain，再由内部运行器验证session身份；legacy-review-run.cjs保留跨恢复的正常输出凭据。错误DLL在运行前被真实拒绝；篡改参数、gain或无身份旧checkpoint也拒绝。会改变数学或二进制身份的升级必须使用全新结果目录，不自动迁移旧数据。

构建调试中曾出现一次仓库根路径多退一级的研究配置错误，已修正；没有把那次失败计入任何成功渲染。深层和固定gain源码只生成于独立build目录，真实产品输出不变性如上验证。

空目录输入准备另发现一次滤镜JSON键顺序导致的字节身份不一致，已保留preparation-first-attempt.json。新增私有研究滤镜表快照只为复现字节序，生成参数必须与它深相等；从空目录准备的输入、材料和表SHA一致，过期输入拒绝测试通过，不改产品表。

Checkpoint回归已证明能变红：在独立Node进程中故意禁用身份守卫，6项单元测试变为4通过/2失败（预期红，exit1），原始TAP为review-hardening/unit-tests-deliberately-broken.tap。恢复正常实现后6/6通过。该反例不改变产品文件，不计入正常实现的68/68通过数。

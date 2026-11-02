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

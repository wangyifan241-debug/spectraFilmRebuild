# 数据来源、许可及可信度

核对：2026-11-02。A=厂家图或明确说明；B=来源不完整的二手资料；C=数学/借用/估计代理。等级不等于摄影认证。

| 来源 | 用途与可信度 | 许可及缺口 |
|---|---|---|
| HARMAN Nov 2023 技术表 | 一代感度/特性曲线，A；2856 K 楔形光谱图 | 厂家保留图表权利；纵轴解释及绝对曝光未证实 |
| HARMAN Jul 2025 技术表 | II 感度，A；D55 | 当前官方 PDF 未提供所需特性曲线 |
| JCH 二代发布文章截图 | II 暂用特性曲线，B/C | 图注写 Phoenix I，代际未核实；不能称官方 II 测量；转载许可待核查 |
| Andrea Volpato / spektrafilm Gold 200 | CMY 染料形状代理，C | CC BY-SA 4.0；衍生 JSON/印相库保留归属、许可与修改记录 |
| 项目估计 | 紫底、尾部、曝光、颗粒、光晕、DIR、合成密度计、印相包，C | 没有实测 HARMAN 地位 |
| 固定 PNG | 工程字节回归 | 不是物理胶片参考 |
| references.v1.json | 唯一生效物理参考清单 | 当前 0 样本；example 未激活 |

原始 URL、提取方法与哈希以 [一代来源记录](../../../engine/resources_research/phoenix200/README.md)、[二代来源记录](../../../engine/resources_research/phoenix2/README.md) 及目录内数字化元数据为准。冻结清单见 [baseline-lock.v1.json](../../../validation/phoenix-accuracy/baseline-lock.v1.json)。本轮未重新下载原 PDF；已有 URL/hash 记录不等于本轮验证原件。

许可依据 [NOTICE.md](../../../engine/resources_experimental/NOTICE.md)、[上游 spektrafilm](https://github.com/andreavolpato/spektrafilm) 与 [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/)。厂家图未被该衍生许可重新许可。

证据基线 `ba076b2befe6586ad3095f897ebf40ea7e9ffd6c`：[PR #4](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/4)；冻结基础设施 `99e3c417644a02be13dbace49ab765713804b5eb`：[PR #6](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/6)。

未来新增来源必须记录 URL、作者、访问日期、许可/再分发条件、原件/派生文件 SHA256、提取方法、代际/实验条件和等级。真实样本还需底片/卷/场景/RAW 身份、曝光/冲洗/扫描/ICC、配对审阅证据。未知保留未知。本轮未新增数据或确认新许可。

# Phoenix 项目状态与交接入口

核对日期：2026-11-02（Asia/Shanghai）。依据已提交代码、测试凭据及 GitHub 查询；本次仅新增文档。

| 项目 | 当前记录 |
|---|---|
| Phoenix 200 | `harman_phoenix_200_exp_v1`，版本 `0.1.0` |
| Phoenix II | `harman_phoenix_ii_200_exp_v1`，版本 `0.1.0` |
| 模型冻结提交 | `ba076b2befe6586ad3095f897ebf40ea7e9ffd6c` |
| 基础设施提交 / 当前 HEAD | `99e3c417644a02be13dbace49ab765713804b5eb` |
| 当前分支 | `codex/phoenix-validation-infrastructure-20261102` |
| 模型分支 | `codex/phoenix-experimental-review-20261102` |
| 真实参考 / 摄影精度 | 0 样本；Not Verified；metrics:null |

[PR #4](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/4) 提供实验模型与隔离测试。[PR #6](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/6) 冻结上述版本并新增独立验证基础设施。先行差距分析提交为 `a4bd80ddfb83a6a0039f03cad85b88aa2fc24243`。两款仍为 `photographic_fidelity_validated:false`；基础设施升级不代表材料升级。

本次 PR 搜索未发现 #6 之后的验证 PR。#5 为并行自动负片反转工作，不能当作 Phoenix 精度验证。#4/#6 描述要求保持 Draft；查询未返回实时 draft/merged 字段，不把该要求当成实时状态证明。本次未合并 PR。

工作区已有其他未提交模型边界/UI/输出改动和报告，本记录不将其归入已提交成果。材料哈希和固定渲染索引见 [models.v1.json](../../../validation/phoenix-accuracy/models.v1.json)。

## 每轮研发结束必须执行

1. 同步核对全部五份文件；没有变化也记录本轮核对及证据 SHA、PR 链接。
2. 状态记录模型 ID/版本、base/head SHA、分支、PR URL、完成与未完成边界。
3. 决策追加证据、替代方案、否决理由与重开条件；来源补齐 URL、许可、哈希及可信度。
4. 验证追加精确命令、环境、数据/材料/配方/renderer 哈希、结果、失败/跳过、日志和未验证项。
5. 更新下一步优先级、阻塞项和验收条件，检查文档链接及 diff 后再结束交接。

统一条目：日期｜轮次｜模型版本｜代码 commit SHA｜PR URL｜证据路径｜结果/限制｜下一步。SHA 必须对应已存在提交；文档自身无需自引用。未建 PR 写“未创建”，建成后补齐。不得编造 SHA、链接、测试或完成状态，不覆盖历史失败。本次只建立人工流程，未添加自动 CI 强制器。

本轮：文档尚未提交、未创建独立 PR；证据 HEAD 为上述 #6 提交，未重跑模型或 native 测试。

入口：[决策](RESEARCH_DECISIONS.md) · [来源](DATA_SOURCES.md) · [验证](VALIDATION_RESULTS.md) · [下一步](NEXT_STEPS.md)。

# 技术决策台账

核对日期：2026-11-02。以下是历史实现及明确约束，不是本轮新实验。

| 决策 | 证据与理由 | 否决方案 / 限制 |
|---|---|---|
| 冻结 v1，候选使用新 ID/更高版本 | baseline-lock.v1.json、candidates.cjs | 不覆盖旧配方引用的材料 |
| 工程与摄影分开验收 | report.cjs、acceptance.v1.json | 不用固定 PNG、合成色块或内部残差证明实物精度 |
| 复用正常光谱材料链 | PHOENIX_MODELS.md、profile/curves 实现 | 不另造 Phoenix 后期 LUT 或独立 shader 替代模型 |
| DIR 材料基准 0.15 | 通用耦合造成曝光轴折返，灰阶回归失败；增加严格单调检查 | 参数是估计，不是 HARMAN 化学测量；保留原始曲线取点 |
| 显式印相起始包，中灰在印相端求解 | 防止继承上一 stock；Endura/TH-KG3 校准报告 | 非真实印相实测；其他组合仍为通用估计 |
| 感度按 log10 解释，一代修正 2856 K 光谱 | build-phoenix.cjs、phoenix-model-assumptions.json | 纵轴解释未证实，待官方条件/测量重开 |
| Gold 染料、紫底与合成密度计代理 | PHOENIX_MODELS.md、NOTICE.md | 非 HARMAN 染料、非 Status M；待真实测量替换 |
| 参考按底片/整卷/场景/RAW/内容哈希传递分组 | references.cjs | 拒绝关联扫描跨 split；test 仅 final_evaluation |
| 单假设候选、私有 staging 与独立 native 凭据 | candidates.cjs、receipts.cjs、tests.cjs | fake renderer 只验证工程流程，不能当真实候选证据 |

模型实现 `f7e994fd86ba2c090ef1b91479f257a605d4911b`，来源哈希修复 `7119ab4e72141ec703bc4cc782aa789dfdf77dce`，隔离凭据 `ba076b2befe6586ad3095f897ebf40ea7e9ffd6c`，对应 [PR #4](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/4)。基础设施 `99e3c417644a02be13dbace49ab765713804b5eb` 对应 [PR #6](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/6)。

代码在 `tools/phoenix-validation/`，模型说明在 `engine/resources_research/PHOENIX_MODELS.md`。未来追加日期、状态（暂定/接受/撤回）、SHA、PR URL、证据、备选与否决理由、重开条件；未实际试验的方案不能写成已失败。本轮仅整理，无新增技术决定。

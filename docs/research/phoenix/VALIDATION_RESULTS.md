# 验证结果台账

核对：2026-11-02。本次只整理历史记录，没有重新执行下述模型测试。

| 轮次 | 结果 | 证据与限制 |
|---|---|---|
| PR #4 隔离审核 | 7/7 CTest，0 失败，55.96 秒；Phoenix 3281 检查，研究门 3254 | validation/phoenix-review-20261102/ctest-summary.txt、ctest-details.txt；仅选定门禁 |
| PR #6 基础设施 | Node v18.17.0 历史 27/27；研究门 3254；冻结 64 文件；两张 Phoenix PNG 字节相同 | validation/phoenix-accuracy/evidence/、reports/infrastructure-v1-final/report.json |
| PR #6 反例 | 常数零 ΔE00 被七个发表向量全部拒绝 | evidence/negative-control.json；非真实胶片验证 |
| PR #6 摄影门槛 | Not Verified；0 真实参考；metrics:null | references.v1.json；验收政策 provisional_unreviewed |

PR #4 证据提交 `ba076b2befe6586ad3095f897ebf40ea7e9ffd6c`：[PR 链接](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/4)。PR #6 提交 `99e3c417644a02be13dbace49ab765713804b5eb`：[PR 链接](https://github.com/wangyifan241-debug/spectraFilmRebuild/pull/6)。#6 的 native 7/7 是复用 #4 历史凭据，不能记为本轮重跑。

本次按该 HEAD 查询 Actions：[基础设施 41424445029](https://github.com/wangyifan241-debug/spectraFilmRebuild/actions/runs/41424445029) success；[Build 41424444844](https://github.com/wangyifan241-debug/spectraFilmRebuild/actions/runs/41424444844) success；[Desktop 41424444833](https://github.com/wangyifan241-debug/spectraFilmRebuild/actions/runs/41424444833) 仍 in_progress。这是查询快照，不预判最终状态。

PR #4 对假定密度计的拟合最大误差约 I 0.03458 D、II 0.01354 D，不是实物误差。相同尺寸/配方 PNG8 的预览导出 RGB MAE <0.6 个码值，保存重开要求完全相同；不能外推至任意分辨率及空间效果。

未验证：真实 ΔE00 mean/p95/max、相对 Y MAE/RMSE、灰阶 L* RMSE/每 EV 斜率、光谱与曲线、II 代际、染料/底基、反转、颗粒和光晕。扫描 Y RMS/偏度/lag1、halo/core 能量不是标准密度颗粒度或孤立乳剂 PSF。本轮未重建应用、未新增 Windows/macOS native 或 Python oracle parity 结论。

复现入口（仓库根目录，推荐 Node 20+）：

```sh
node --test tools/phoenix-validation/tests.cjs
node tools/phoenix-validation/cli.cjs verify
node tools/phoenix-validation/cli.cjs report --run <唯一轮次>
node tools/phoenix-validation/cli.cjs report --run <唯一摄影轮次> --gate photographic
```

退出码 0=所选门槛 Pass，1=Fail/输入无效，3=Not Verified。详细条件见 [验证说明](../../../validation/phoenix-accuracy/README.md)。每轮追加环境、命令、SHA/PR、输入/材料/配方/renderer 哈希、结果/退出码、日志和失败/跳过，不覆盖旧记录。

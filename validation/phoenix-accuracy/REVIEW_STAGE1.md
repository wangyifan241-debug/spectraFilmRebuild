# PR #6 第一阶段审查补强 — infrastructure v2

不启动第二轮物理曲线研究；不修改 PR #4、Experimental v1、印相表或任何 Phoenix 色彩参数。
当前真实参考清单仍为零样本，摄影门槛为 **Not Verified**。

## P1：分离配对验证层次

`references.cjs` 继续复用原传递分组 / split；新增 `evidence.cjs` 校验接口。

1. **文件身份**：读取本地文件，验证 manifest 与独立证据中的 SHA256 绑定。
   这只证明所比较的字节，不证明底片型号或场景真实性。
2. **人工与文档审核**：`pairing_evidence` 必须为独立的
   `SpektraLab.PhoenixPairingEvidence` / version 1 JSON，包含 sample_id、raw_sha256、
   scan_sha256、四个物理 identity、human_review，以及 capture_records。
   human_review 需 status=reviewed、reviewed_by、reviewed_at（ISO 日期）、method、
   scope=capture_provenance_and_scene_correspondence。capture_records 至少有
   film_frame_record 与 scene_capture_record 两种角色，每项有 description 与文件绑定。
   capture_relation 需 timing、lighting、motion、exposure_relation 的评估记录。
   仅有 manifest 的 pairing.status=verified 或任意“配对正确”文本不再足够。
3. **场景几何**：RAW→扫描与 RAW→模型输出必须各自有独立配准文件，重新计算留出点残差。
   人工审核、哈希通过和几何残差通过分别显示，不能互相替代。

这些文档审核仍是可审计的人类声明，工具无法认证拍摄日志真实性、相机时间、
乳剂身份或伪造的对应点。报告保留该限制，不能把配对检查称为物理准确性认证。
只有完整条件下的配对观测才有条件式指标；真实摄影验收仍有独立政策、样本组和空间证据门槛。

## P1：ROI 的可扩展外部配准接口

`SpektraLab.PhoenixRegistration` / version 1 的 required 字段：

- sample_id、source_sha256（真实 RAW）、target_sha256（扫描/模型图片）。
- status=reviewed、reviewed_by、reviewed_at、method、coordinate_source。
- coordinate_units=target_image_pixels；source_size/target_size 为 [宽,高]。
- transform 为外部工具/人工审核产生的 3×3 RAW→目标图片 projective 矩阵。
- control_points 每项 `{source:[x,y],target:[x,y],role:"fit"|"held_out"}`。
  至少四个非退化拟合点与两个独立留出点，不允许重复点。
- regions 每项 `{id,source_rect:[x,y,w,h],target_rect:[x,y,w,h]}`，
  目标包围框须与矩阵变换结果一致；两种输出的同名 ROI 须指向相同 RAW 区域。

接口不估计矩阵，也不自动解码 RAW/TIFF 或做镜头校正。它重新计算提交坐标的
留出 RMSE 和所有点的最大误差，拒绝奇异矩阵、越界点、退化控制点及不一致 ROI。
几何 QC policy v1：留出 RMSE ≤1 目标像素、全点 max ≤2 像素；这是一项明确的
工程配准阈值，不是乳剂还原度阈值。坐标与尺寸来自外部适配器并须审阅，残差本身
不能证明对应点属于同一真实场景。未来非投影/畸变模型应新增有版本的适配器，不能默默套用此模型。

新增文件绑定：sample.files.reference_registration；sample.model_outputs[id].registration。
观测包的 roi_registration 记录 artifact_sha256、coordinate_source、reviewed_by、
reviewed_at、method、error_px（held_out_rmse/max）及完整 regions；导入时重新核验。
空间 ROI 另需 registration_roi_id，且目标 ROI 尺寸匹配。缺少可靠配准时返回
Not Verified / metrics:null；不把“未配准”当作零误差。

PNG 提取 spec 增加 raw_source 文件绑定与 registration 文件绑定：

```sh
node tools/phoenix-validation/cli.cjs extract --input scan.png --spec rois.json --data-root references --out observations.json
```

未指定 data-root 时使用 spec 所在目录。实际 PNG 尺寸、字节哈希和 ROI 坐标均与
证据交叉检查；物理/模型观测无证据则退出 3。synthetic_engineering 仅供数学测试，
即使能提取数值也标记 Not Verified，不会获得摄影资格。

## P1：本次执行、历史凭据、未运行分栏

JSON 报告格式升级为 version 2；原 infrastructure-v1-final HTML/JSON 原样保留。
新报告的 engineering_gate 包含：

- current_execution：本次真的执行的冻结检查、research gate；显式要求时执行新增工具测试。
- historical_evidence：父 PR 原始 source commit、Git blob SHA、工作区字节 SHA、原日志路径，
  executed_this_run=false。历史 7/7 结果不再贡献本次执行 Pass。
- not_run_this_execution：逐项列出未在当前报告执行的 native 测试。

```sh
node tools/phoenix-validation/cli.cjs report --run review-stage1-v2 --run-tests yes
```

`--run-tests yes` 真正启动测试子进程，将命令、返回码、原始 TAP、Node 版本和测试源 SHA
写入同一报告。没有该选项时新增套件标为 Not Run。候选导入的 native 凭据也仅为历史
支持证据，不再伪装成本次 native 执行；没有本次执行时工程门槛保持 Not Verified。
任意软件门槛都不代替独立的真实胶片门槛。

## P2：印相重新标定独立设计

现有 frozen_parent_print 行为不变；render-run 记录父/暂存印相包哈希及未重新标定状态。
独立重新标定采用四格对照、训练集校准和独立 artifact 的设计，见
[PRINT_CALIBRATION_MODES.md](PRINT_CALIBRATION_MODES.md)。本轮不运行拟合器，
请求 separate_recalibration 会明确拒绝，避免将白平衡适配混入物理假设变化。

## 与 EXE 保存错误修复的边界

另行处理的 Windows 关闭/编辑记录修复不混入 PR #6 的验证基础设施提交。
旧时间戳和全局资源版本可以同时冲突；当前会话自动保存到独立的源身份/资源上下文目录，
保留原记录，不按首尾采样自动套用旧设置。关闭不再要求手动“备份”按钮；一般保存仍失败时
尝试写不能自动套用的恢复快照，窗口仍允许关闭。该本地 EXE 的测试凭据另行保存，
**不列为本报告重跑了 PR #4 native suite**。

## 本轮实际执行结果

- `report --run review-stage1-v2 --run-tests yes`：本次启动子进程执行 **39/39**
  项工具测试（原 27 项全部保留，新增 12 项审查边界测试）；原始 TAP 存在新报告与
  `evidence/node-tests-review-stage1.tap`，原 27 项历史 TAP 未覆盖。
- 本次 report 内执行 research gate：**3,254** 项模型/软件自洽检查。
- 冻结文件 **64/64** 通过；两款固定输入 PNG 与 v1 原结果相同。
- 原七项 native 测试：本次 **Not Run**，历史来源 SHA 与日志原样保留。
- 真实参考 **0**：摄影门槛 **Not Verified**；没有真实摄影分数。
- 新提交的 CI 结果以该提交的 Actions 为准，不沿用上一个提交的绿色状态。

# 材料库使用

1. 启动新版程序，导入 RAW；顶部点 **材料库**。
2. 内置列表新增 **Kodak Tri-X Reversal 7266 (upstream 0.3.3)**，点击“用于当前照片”。它是反转片，直接扫描，不使用相纸。
   同时新增 **Kodak Double-X 5222 (6.5 min, upstream 0.3.3)**：固定上游 6.5 分钟条件，负片自动配对现有 Ilford 黑白相纸；不提供其它显影时间插值。
   **Kodak Print Film 2302 (5 min, upstream 0.3.3)** 位于“印相材料”，不是拍摄胶片或普通相纸。先选黑白负片（例如 5222），再选 2302；它使用无 Multigrade 滤片的印片路径。切回 Ilford 时恢复滤镜 2。此版本保留现有放大机照明/归一化，不代表某台电影光学印片机的独立标定。
3. 导入自定义材料：选择一个完整 Profile JSON，或选择材料包的 `pack.json`。不支持 ZIP、插件代码、darktable 整包/光谱全局表自动转换、单通道原始曲线或多显影时间家族。
4. 每份新材料必须有 `info.stock/name/type/stage/channel_model`，现有 81 波段/三通道数据，以及 metadata 的 version、license、source_url 或 datasource。格式错误会给出拒绝原因，不部分激活。来源/许可字段由提供者声明，用户仍须确保有权使用和再分发。
5. 导入后生成 `usr_...` 内容版本 ID；原始文件和转换的命名空间副本都保存在用户目录，现有资源不覆盖。相同原文件再导入是同一版本；改动文件生成新版本。
6. “禁用新选择”不改旧照片。“从可选列表移除”是逻辑移除，**历史存档仍保留**；在历史分类可重新启用。当前没有永久清理存档功能，以免破坏旧照片。
7. 编辑记录 v8 同时保存材料 ID、完整 Profile SHA256 和全局光谱常数/中性滤片/印相 LUT 的联合指纹。缺失/损坏或内容不匹配会明确报错；不会偷偷换成别的胶片。内置数据或全局数据升级冲突也会拒绝，不自动找回旧安装包；保留旧程序目录或修复对应资源后再重开。旧 v1–v6 没有材料指纹，v7 没有全局资源指纹：只按原有 ID 读取，读取本身不改记录，后续保存才锁定当前数据；这不能证明它与最初编辑时的数据相同。

默认数据位置为应用本地数据目录的 `Materials/`；测试及 `--edit-store` 使用显式编辑目录下 `Materials/`。`versions/<id>/original.json` 保留原样，`profile.json` 只改引擎 stock ID，CHANGELOG 记录修改；`registry.json` 原子记录版本、启用及移除状态。每个进程在 `runtime/session-*` 下有私有视图，退出会回收该视图，不删除历史材料。不要手工修改 versions 或 registry。

材料包示例（放在 profiles/ 所在目录，大小和 SHA256 必须匹配实际字节）：

```json
{"format":"SpektraLab.MaterialPack","version":1,"files":[
  {"path":"profiles/my_film.json","size":12345,"sha256":"<完整小写 SHA-256>"}
]}
```

每包最多 32 份、总计 32 MiB，单份不超过 4 MiB。不能通过路径访问包目录以外的文件；不下载依赖，不执行转换脚本。

数据质量边界：结构通过不等于实物测量通过。管理器不会自动导入 darktable 的逐胶片默认颗粒/光晕/耦合或中性印相滤片；新命名空间材料在缺少专用校准时使用当前引擎默认。7266、5222 和 2302 保留上游光谱/阶调数据，只做通道表示或明确条件切片转换，颗粒与光晕尚非独立拟合。原作者与 CC BY-SA 许可、原样来源和变更说明在程序资源 `open-materials/`；详细审计见 `material_audit/REPORT_2026-10-30.md`。

扫描数据研究工具 `spk_material_fit` 只输出 Status M 曲线拟合报告，`profile_ready=false`，**不能导入材料库**；这不是错误，而是避免把仪器密度冒充乳剂层数据。用法与未完成的光谱标定门见 `material_fit/README.md`。

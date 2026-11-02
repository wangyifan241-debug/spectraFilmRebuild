# HARMAN Phoenix II 200：数字化资料与候选特性曲线

2026-11-02。二代为当前优先目标；邮件联系已按用户要求取消。
已完成官方图面感度数字化，以及二代发布文章中的候选特性曲线数字化。
后续按用户明确允许的近似范围生成了可用的 Experimental v1；见 `../PHOENIX_MODELS.md`。
这不改变候选图归属待核实、真实胶片还原度未验证的结论。

## 官方感度图

来源为 HARMAN technology Limited 的 July 2025、5 页技术表。
官方：https://www.harmanphoto.co.uk/amfile/file/download/file/1968/product/2208/
实际下载：https://www.fotoimpex.com/shop/images/products/media/72015_5_PDF-Datasheet.pdf
本地 PDF SHA256：`020260790a3b705d38ded5573abcb2af43b228b5e3440bc23463273cf78bff08`。
不同站点的 PDF 可能重新压缩，不能把其它版本的哈希用于此文件。

第 2 页明确写 D55 日光楔形光谱，图面纵轴是 Relative Sensitivity，未定义线性/对数意义。
图为原生 795×559 索引位图。本次直接解码 PDF 内图片与调色板，不追踪放大插值的页面。
`spectral-chart.csv` 保存 670 个原生横向像素列，对应非整数 nm。
每列分别检测三条曲线的黑色笔画中心；手工路径引导只选分支，不把引导点写成测量值。
交叉处留空，原图未覆盖波段留空，不能将空值当零或物理截止。
`digitization.json` 保存源哈希、坐标校准、引导点、阈值、有效点统计与缺口说明。
`spectral-review.png` 是本地原图叠加取点的检查图；已人工查看提取位置。

## 找到的特性曲线：候选资料，存在代际归属冲突

Japan Camera Hunter 的 2025-07-16 发布文章：
https://www.japancamerahunter.com/2025/07/film-newfilm-news-harman-phoenix-ii-risess-harman-phoenix-rises-again/
曲线截图：https://www.japancamerahunter.com/wp-content/uploads/2025/07/Screenshot-2025-07-14-at-15.49.04.png
SHA256：`9ca4c40d38637e84f5dc125a6f259d270f61b9550f437a5e479fbff1ea38fbe3`。

文章以 Phoenix II 发布资料呈现该图，但图下注明确是 **Phoenix I**，且同文感度图写 2856 K，与当前官方 PDF 的 D55 标注不同。
这可能是早期资料、编辑错误或混用；现有证据不能裁定。
不能称为已核实的官方二代曲线，也不能因其反差较低便认定归属。
其曲线明显不同于本地一代 2023 PDF，仍值得保存并继续追查原始出处。

`characteristic-candidate.csv` 保存该截图 298 列的彩色笔画中心数据。
横轴仍为 Relative Log Exposure，纵轴 Density；绝对曝光零点和密度标准未知。
坐标 (85,52)..(384,237) 映射曝光 0..3、密度 2..0。
交叉/颜色混合无法辨认的通道留空，不平滑、不补点。像素步长约 0.0108 D，不是准确度保证。
`characteristic-candidate.json` 明确 `verified_generation:false`、`profile_ready:false`。

## 重现

把以上两份源文件分别下载为 `source.pdf` 和 `characteristic-jch.png`，然后从仓库根运行：

```powershell
node engine/resources_research/phoenix2/digitize.cjs
node engine/resources_research/phoenix2/digitize-characteristic.cjs
```

PDF 提取脚本固定源哈希；候选截图脚本记录实际哈希并固定图片尺寸/坐标。
原始 PDF、截图及检查图只作本地研究，不随应用打包；没有把厂家或媒体图套用本项目代码许可。
不需要 Python、npm 包或修改运行时。

## 还差什么

优先核实候选曲线归属和测试条件，再讨论曝光标定、染料/底基替代及闭合。
当前官方 PDF 无特性曲线，本次没有以一代数据冒充二代。
即使候选图归属得到证实，也仍需光谱/曝光标定及独立实拍验证，才能声称真实还原度已验证。
社区检索及排除理由见相邻 `../phoenix-community.md`。

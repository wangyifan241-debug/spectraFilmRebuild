# 第五轮真实引擎视觉检查

所有模拟图均由 SpektraLab Vulkan 光谱引擎实际生成。没有 AI 图、Photoshop 调色或占位效果。物理准确度：**Not Verified**。

## 打开产物

本次工作树：`C:/Users/Strickland/.codex/worktrees/phoenix-round5/SpektraLab`。

在该目录打开 `build/round5-visual/index.html`：同一 DSC02443.ARW 的中性数字输入、第四轮红/蓝凤凰、第五轮红/蓝凤凰。按钮切换适合窗口和 100% 像素，容器可滚动。PNG 是实际 sRGB16 输出；各图的 TIFF16、浮点阶段、配方和 SHA256 在原实验目录。未授权再分发该 RAW，因此照片不提交到 Git。

`build/round5-comparison/index.html` 是合成阶梯的 v1 Optical Print / R4 / R5 三列对照。`build/round5-scene2` 是第二个实际 RAW DSC02337.ARW，包含人物和绿色背景；它不是天空或红色物体的充分验证。没有拿合成图冒充这些真实场景。

## 固定条件与变化

输入为同一 ARW、LibRaw compatible16、as-shot WB、线性 ProPhoto、AHD、同一步长预览。默认补偿 0 EV，无自动曝光、颗粒、光晕或 glare。保留原有解码带来的裁切限制。两轮同一理想 D50 光源、Gaussian 探测器、逐波长底基扣除、固定中性逆曲线、亮度 shoulder、OutputTransform 和 sRGB ICC。中性数字输入也使用同一 shoulder，故不是无曲线科学 RAW 查看器。

第五轮只替换扫描转换的求解方法：从中性点局部 Jacobian，改为 320 个固定合成输入、-2..+2 EV 的中性约束 ridge 拟合；每代单独求矩阵。没有拟合参考照片，没有 3D LUT。材料感光、染料和特性曲线未改变；不能声称本轮证明了新的光谱材料参数。光谱胶片响应仍决定反转后的输入及每代矩阵。

## 独立视觉判断

**红凤凰**：在 DSC02443 上，R5 的肤色比 R4 略暖，面颊和手部更偏桃色，暗部仍偏暖棕；这是可见的风格变化，不足以证明更接近 Phoenix。白衬衫仍有青/紫区域，不能宣布已解决高光色偏。黑发依然很暗。植物背景测试仍有较鲜明青绿和亮部压平的风险。

**蓝凤凰**：R5 人像比 R4 更暖、略有红润感，差异较小但可见。白衬衫的青/紫表现仍然存在。两代成片在这个场景非常接近，当前模型未展示足以可靠识别代际的强证据。不得用宣传中的“二代更正常”推导本结果已准确复现。

**高光与阴影**：共同亮度 shoulder 控制整体亮度，但不能保证所有高饱和颜色均在 sRGB 内。中性约束只约束矩阵的行和；整个 film/inversion 的中性仍有浮点误差和端点范围限制。极端曝光和宽色域量化裁切依然需要观察，不能用无 NaN 代替视觉合格。

**天空、植物、红色**：当前真实输入覆盖不足。第二张 RAW 有植物，合成阶梯有通道偏置高饱和输入，但没有充分的真实蓝天、红物体验收，不能报告已通过。

## 软件证据与真实参考

合成测试图线性 ProPhoto 软件误差（通道输入 <=0.75）一代 0.06835 → 0.06671，二代 0.05754 → 0.04924。它们不是实测胶片 ΔE 或准确率。此测试图不同于拟合训练网格，但属于同一数学模型，不能证明跨真实场景泛化。

五档 -2/-1/0/+1/+2 EV 使用完全相同的扫描配方；这证明没有逐图自动调参，不能单凭此结论声称色彩恒定。

指定两张真实胶片参考均为 CC BY-SA 4.0：Felipe Tofani 的 [Time to take the train](https://commons.wikimedia.org/wiki/File:Time_to_take_the_train_-_Flickr_-_ftrc.jpg)，以及 Mario A. P. 的 [Lone flight](https://commons.wikimedia.org/wiki/File:Lone_flight_-_Flickr_-_Mario_A._P..jpg)。前者标注 Mamiya RZ67 / Negative Lab Pro；后者作者称未修饰，但扫描、反相、冲洗条件未知。两者都是展示 JPG，不是线性负片。HTML 保留远程原图、来源和许可链接；本机下载超时，没有伪造图片 SHA256，也未宣称完成本地图像库。非配对参考不能回答“差多少 ΔE”，当前不能独立验证真实胶片接近程度。

官方资料支持一代高反差、二代较正常反差和更宽容度的定性区分；当前材料仍有 Gold 代理染料、未实测底基、感度纵轴解释和二代特性曲线归属问题。参考官方描述不等于验证物理模型。

## 重现

```powershell
./windows_UI/phoenix_round5/build.ps1 -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 -BuildDirectory build/round5
./build/round5/app/spk_phoenix_round5.exe --render ./build/round5/app/resources C:/Users/Strickland/Downloads/DSC02443.ARW ./build/new-round5-raw
# R4 控制必须用 PR #11 的独立 EXE、资源与相同输入；不要覆盖历史目录。
node validation/phoenix-round5/gallery.cjs build/round4-raw-control build/new-round5-raw build/new-visual
node validation/phoenix-round4/inspect.cjs build/new-round5-raw
node validation/phoenix-round4/verify-exports.cjs build/new-round5-raw
```

完整尺寸入口为 `--export-full`。此前与其他 GPU 实验并发的尝试发生 `VkResult -4`，不能算完成导出；顺序重试结果须单独记录。现阶段为独立研究 EXE，**未接入主 Windows 编辑器**，不能当作主程序材料、保存恢复或预览/导出集成验收。

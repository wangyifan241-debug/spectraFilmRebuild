# Windows JPG / PNG 导入：实现与验证（2026-11-01）

## 行为与调用链

Qt 的“导入照片”、多选和拖入入口现在接受 JPG/JPEG 及 8/16 位 PNG，保留 RAW 与双拼项目。编辑、预览、历史记录、原图对比、几何、重新打开、半格双拼及四种导出格式继续走原控制器。

没有增加第二套胶片算法。`PreviewHost::open_raw` 保留历史 C++ 名称，按内容识别 JPG/PNG，复用 `engine/host/image_io.cpp`，将 ICC 解码后的 **线性 ProPhoto float32** 装入现有不可变输入帧。之后 `input_cctf_decoding=false` → 原 Vulkan/胶片/相纸管线 → 编码 ProPhoto RGBA16 → 原 Qt 后期与几何 → 显示 ICC 或 sRGB 导出。无新的运行时依赖，没有 C API/wire 或材料 Profile 变更。RAW 分支继续直接调用原 LibRaw 解码器。

- 有 RGB matrix/TRC ICC：实际转换像素；不支持或损坏的 ICC 明确拒绝，保留上次成功画面。
- 无 ICC 的 JPG/PNG（包括 PNG16）：按 sRGB 反传递函数解码。修复共享 host 原先把无 ICC PNG16 误按线性 ProPhoto 处理的类型判断；无 ICC 深位 TIFF 的原规则保留。
- 透明 PNG：先转线性光，再合成到白底；不保留透明输出。host 缩略图使用相同规则。
- JPG EXIF 方向仅应用一次。普通图片禁用 RAW 高光扩展；新导入与同步参数也不会把 RAW headroom 模式传播给 JPG/PNG。
- JPG 已含相机/开发器色调曲线，导入不意味着能恢复 RAW 辐射或已丢失高光。PNG 非 ICC 的自定义 gAMA/cHRM 并未新增解释能力；此版本的无 ICC 合约为 sRGB。CMYK/LUT ICC 等不支持的配置会拒绝。

## 实际验证

Windows 11 x64、Qt 6.11.3、MinGW 13.1、本机 Vulkan。Release 构建完成，CTest **36 通过 / 0 失败 / 0 跳过**，362.40 秒。见 `ctest.txt`、`qt-build.json`。

新增 `spk_qt_raster_input`：55399 次断言，拥有的中文路径/大写扩展名 JPG/PNG8/PNG16、灰阶、P3 ICC、JPEG APP2、非对称方向 6、8/16 位透明度及缩略图、坏图片/ICC 拒绝与画面保留、RAW 模式隔离、真实 Vulkan 渲染、编辑与重启恢复、JPG+PNG 双拼、裁切旋转后 TIFF/PNG8/PNG16/JPEG 编码并重新解码。TIFF 使用 host 独立读取器，因为本机 Qt 无 TIFF 插件；TIFF 像素/ICC 的原有专项测试仍通过。

原 `spk_qt_qml_workflow` 也通过（222 个报告字段，拥有的合成 Bayer DNG）；这不是新的真实相机 RAW 或 Mac parity 验证。另使用最终 app 的 `--snapshot` 分别导入 JPG 和透明 PNG16：均退出 0、成功生成画面、stderr 为空；两幅截图见 `jpeg.png` / `png16.png`。它们来自 **offscreen Qt software 场景**，不证明可见窗口、原生文件对话框或物理显示器色彩正确；未覆盖 `screenshots/` 中的用户截图。

旧版分屏修复构建对相同灰阶 PNG 的 `--snapshot` 探针退出 1，未生成画面；新增控制器导入用例已绿。开发中的初次构建因源码在编译期间继续改动，被已有的来源清单检查拒绝；最终构建在稳定源码上重新完成，不使用该中间构建作为交付证据。

## 数值与测试参考

- 无 ICC sRGB 灰阶 code 128/255：独立分段反 CCTF 得到线性 `0.21586050011389926`，实际最大误差 `8.17841e-10`。
- P3 编码 (128,64,32)/255：独立双精度 D50 P3 → XYZ → ROMM 参考为 `(0.1495610382731677, 0.06382599711230727, 0.01605068841476101)`。host 为约 `(0.149557,0.0638272,0.0160569)`，满足 **1e-5** 门限，包含嵌入 ICC s15Fixed16 色度量化。
- Qt 对照约 `(0.149508,0.0637217,0.0161746)`。最初临时使用 1e-4 的 Qt 精确参考假设在两次窄门中失败；未修改胶片或转换参数去迎合它。改为更严格的独立公式 1e-5 检查，并保留此前审查已确立的 Qt/host **0.001** 对照门限，差异继续记录。这不宣称 Qt 与 host 逐 16 位 code 相同。
- 额外独立 `lowbit_probe.cpp`（不计入 36 个 CTest）使用 PNG16 codes 32768/32769/32770：相邻线性差 `1.412630081e-05`，独立 CCTF/白点最大误差 `6.769178079e-09`。证明保留低 8 位信息；该验证链接本次已编译的 reader，没有修改交付源码。见 `lowbit-probe.txt`。

## 命令与交付

```powershell
& C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1 `
  -SourceDirectory C:/Users/Strickland/SpektraLab-work `
  -BuildDirectory C:/Users/Strickland/SpektraLab-work/build/qt-raster-input `
  -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 `
  -Jobs 6 -RunTests

# 在 build/qt-raster-input 下，复用该次 CMake 的编译/链接参数：
& C:/Users/Strickland/SpektraLab-deps-20261028/qt-6.11.3/Tools/mingw1310_64/bin/g++.exe `
  -std=c++20 '@CMakeFiles/spk_qt_raster_input_test.dir/includes_CXX.rsp' `
  C:/Users/Strickland/SpektraLab-work/validation/raster-input-20261101/lowbit_probe.cpp `
  -o lowbit-probe.exe '@CMakeFiles/spk_qt_raster_input_test.dir/linkLibs.rsp'
./lowbit-probe.exe
```

新程序：`D:/学习资料/SpektraLab/build/qt-raster-input/app/SpektraLabQt.exe`。EXE SHA256 `cb5dbc80e840cb2e58c51b9f3472605052f58a64404ce2fb6ddc29e9eb87f80a`。源码/部署清单摘要见 `qt-build.json`。旧窗口和旧启动路径不会自动更新，未关闭或强制结束用户窗口。

本次未重做安装包；此前 review.2 包不包含分屏修复或 JPG/PNG 导入。仍为待人工审查的 Preview，之前审查报告的正式发布风险并未因此消除。

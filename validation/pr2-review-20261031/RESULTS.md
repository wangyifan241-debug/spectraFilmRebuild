# PR #2 Windows 复核与修复记录

基准为 PR #2 HEAD `0bf216b3831595d28d8d0bbb00009a4c1c28c0a4`。修复分支为 `codex/pr2-stability-colour-review-20261031`。main 与 PR #2 分支均不修改、不合并。分类及原始行号见 [AUDIT.md](AUDIT.md)。

## 已实现的修复

| 项目 | 实现与范围 |
|---|---|
| 任务生命周期 | `PreviewController.cpp:89` 的任务信封保留 generation、settings revision、operation；worker 捕获标准与非标准异常，错误能归还 UI，worker 可继续工作。`accept:129` 保留先拒绝旧 generation 的规则。原 busy 竞态在当前公共入口下为误报；新增测试还覆盖将来允许排队时的旧回复。 |
| 曝光 | `FrameOutput.cpp:74-84` 新配方按真实 sRGB / ROMM CCTF 解码 → 乘 2^EV → 编码。`EditStore.cpp:244,367,449` 的 v9 明确保存 `linear-v1` / `gamma22-v1`。v2-v8 自动读取为历史模型；v1 原本无后期调整。旧记录读取不改写，实际保存才升级容器，旧模型仍保留。默认未调整时像素不变。 |
| 内存 | `FrameOutput.cpp:495` 在最终编码前释放不再需要的工作空间中间图。原有 128 行浮点色彩转换与 TIFF 分条写入保留。此项缩短一张 RGBA16 图的持有期，**不保证整个进程峰值必然降低同样数值**。 |
| 编辑保存 | `PreviewController.cpp:811-833` 保留 pending、显示错误、最多 3 次自动重试（1/2/4 秒），成功后清除提示；提供手动重试。正常关闭会同步尝试保存；仍有未保存修改时取消关闭并显示对话框。原子写入、冲突拒绝、照片指纹与材料版本校验保留。 |
| 安装与卸载 | `preview.nsi` 校验 UTF-16 marker 的产品/构建和实际安装目录；受管理目录及其祖先存在联接/符号链接时拒绝。删除前先检查全部目录，随后只删准确文件清单，目录仅非递归删除。解压/写卸载器/写 marker/注册失败会返回错误，不启动应用，不留下有效 marker；为保护个人文件，失败残留不递归清理。 |
| 构建与打包 | `source-inventory.ps1` 增加部署目录的准确文件集合及 SHA256；`build.ps1` 保存测试后的清单；`package.ps1` 在打包前及复制后核对 DLL、插件、资源与 EXE，并拒绝 reparse 输入。该关系是可追溯构建，尚非逐字节确定性构建。 |

胶片物理算法、shader、C API、材料资源没有修改。后期曝光是有版本区分的新行为，不宣称与 Mac 的历史 pseudo-linear 调整相同。v9 文件不能保证被旧程序读取；旧程序应拒绝未知版本，使用前请保留编辑目录备份。

## 数值对照

下表为 +1 EV 的 16 位编码值，输入先量化到 16 位。新测试允许独立双精度 CCTF 计算与 float32 host 实现最多相差 1 个 16 位码值。

| 空间 / 输入编码值 | 历史 Gamma 2.2 | 新线性 EV |
|---|---:|---:|
| sRGB / 0.01 | 898 | 1310 |
| sRGB / 0.18 | 16165 | 16953 |
| sRGB / 0.46 | 41311 | 41447 |
| sRGB / 0.70 | 62865 | 62443 |
| ROMM / 0.01 | 898 | 1310 |
| ROMM / 0.18 | 16165 | 17337 |
| ROMM / 0.46 | 41311 | 44307 |
| ROMM / 0.70 | 62865 | 65535 |

高光在最终归一化编码边界裁切；真正增加一档曝光更早达到白端是预期行为。灰阶、低亮度、彩色 patch、白端和零曝光恒等均纳入测试；这不是人像相机配置文件的肤色金样验证。

ICC 独立 Qt 解码 TRC 最大绝对误差：sRGB / P3 均 `7.60396e-06`，ROMM `1.32181e-05`。ICC PCS 白点检查 D50。host 与 Qt 的中性色/低彩度 patch 输出最大绝对差：sRGB `0.000381476`、P3 `0.00030518`、ROMM `0.000976577`，界限为 `0.001`（约 0.26 个 8 位码值），**不是 16 位逐码一致性声明**。超色域 CAM16 压缩与 Qt 裁切策略不等价，未混在上述断言里。

Qt 当前导出仍明确为 sRGB：先转换像素，再嵌入对应 ICC。广色域选择器属于功能差距，本轮提交了统一契约及数值测试，没有仓促改写输出链。详见 AUDIT.md。

## 内存测量

原生 Windows x64 / RTX 3050 Laptop 4 GiB / 约 16 GiB RAM。独立进程使用合成 RGBA16，执行后期曝光、裁切、2° 拉直、90° 旋转、翻转和 TIFF16 编码；每例退出自动清理临时文件。

| 尺寸 | PeakWorkingSetSize 字节 | MiB |
|---|---:|---:|
| 6000×4000（24MP） | 710823936 | 677.89 |
| 8256×5504（45.44MP） | 1330663424 | 1269.02 |
| 9600×6400（61.44MP） | 1794011136 | 1710.90 |

这是 **Qt 导出子链**，没有包含真实 RAW 解码、host 保留帧或 GPU 分配，不可当作完整 RAW 峰值。测量时后台有构建活动，不用耗时作为速度基准。完整 45/61MP 相机 RAW 的 CPU/VRAM 峰值及跨显卡表现仍未验证。

## 复现与运行命令

以下路径中的 `C:/Users/Strickland/SpektraLab-work` 是当前仓库的 ASCII 开发联接，编译工具使用该路径。安装测试使用物理目录 `D:/学习资料/SpektraLab/.../安装 测试`，不通过联接执行卸载。

```powershell
& C:/Users/Strickland/SpektraLab-work/windows_UI/build.ps1 `
  -SourceDirectory C:/Users/Strickland/SpektraLab-work `
  -BuildDirectory C:/Users/Strickland/SpektraLab-work/build/qt-pr2-review-final `
  -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 -Jobs 6 -RunTests

& C:/Users/Strickland/SpektraLab-work/windows_UI/tests/installer_safety.ps1 `
  -NsisDirectory C:/Users/Strickland/SpektraLab-work/build/packaging-tools/nsis-3.12 `
  -WorkDirectory C:/Users/Strickland/SpektraLab-work/build/pr2-installer-final-3

# 每次使用新的测试目录；内存探针各尺寸分别启动一个进程：
./build/qt-pr2-review-final/spk_qt_review_memory_probe.exe 6000 4000 ./build/qt-pr2-review-final/app/resources
./build/qt-pr2-review-final/spk_qt_review_memory_probe.exe 8256 5504 ./build/qt-pr2-review-final/app/resources
./build/qt-pr2-review-final/spk_qt_review_memory_probe.exe 9600 6400 ./build/qt-pr2-review-final/app/resources
```

环境：Windows 11 x64，Qt 6.11.3，MinGW-w64 GCC 13.1.0，CMake 3.25，LibRaw 0.22.2，NVIDIA RTX 3050 Laptop。没有把 Linux、Wine 或合成 DNG 当作真实相机验证。

复现记录：原 FrameOutput 的曝光测试退出 1（例如 ROMM 0.46 得到 41311，预期 44307）；原 NSIS 接受伪造 marker 并删除受管理文件（退出 0）。早期构建有新增 host 数值测试的 include/link 缺失，已补齐；首轮完整 CTest 为 32/34，随后为 34/35，均作为失败记录保留。失败断言没有删除：旧 Gamma 精确断言保留在 legacy 模型，新增线性 EV 断言；旧材料 fixture 必须移除当时不存在的 exposureModel 字段，仍断言旧文件字节未改写、画面不变、版本缺失不替换。

## 最终验证

代码提交 `eff48d12`，最终稳定源码 Release 构建成功。完整 CTest **35 通过 / 0 失败 / 0 跳过，206.80 秒**。完整输出为 [ctest.log](ctest.log)，构建身份为 [qt-build.json](qt-build.json)。其中包括原有 32 项与新增 controller、colour、payload inventory 三项。先前失败的断言已修复为正确的版本 fixture，全部重跑，没有跳过或降低精度界限。

| 实际运行 | 结果 | 限定 |
|---|---|---|
| Windows Qt Release + CTest | 35/35 | 原生 Windows，包含 Vulkan/CPU 数值契约、OOM 拒绝、PNG/JPEG/TIFF16 独立解码、ICC 和几何回归 |
| 真实 Sony ARW 原生 Qt 自测 | 退出 0，passed=true；45 次画面发布、39 次任务结束 | 真实 LibRaw / NVIDIA Vulkan；原尺寸 TIFF 6024×4024；合成 DNG 未冒充该照片。**窗口隐藏、window_exposed=false**，不是可见窗口交互验收 |
| 安装器攻击/失败 fixture | 1/1 脚本通过 | 非管理员；中文路径；伪造 marker、移动安装目录、目录联接、写权限拒绝、覆盖拒绝、用户文件保留 |
| 完整新安装包 | 1/1 脚本通过；1721 个安装文件 SHA256 正确 | 中文路径安装后独立 PATH 启动、拒绝覆盖、安全卸载、保留用户文件、用户真实注册表不变；非干净 VM |
| 归档源码 | 480/480 条目哈希正确、无额外条目 | 与稳定源码清单一致 |
| 大图探针 | 3/3 独立进程退出 0 | 上述 24/45/61MP 合成 RGBA16 的 Qt 子链 |

真实 RAW 的完整自动化序列还覆盖连续编辑去抖、真实在途渲染被新设置取代、导出与编辑交错、显示发布失败后的恢复、胶片/相纸、裁切、对比、日期、片边、双拼、材料版本保存和文件导出。原生进程每 500ms 读取 Windows peak counter，观测到峰值工作集 `2875416576` 字节，峰值 paged/commit counter `4474970112` 字节；这是**整段多操作自测**，不是单次导出或 GPU 峰值。详情见 [native-raw-report.json](native-raw-report.json)、[native-process-memory.json](native-process-memory.json)。

Computer Use 窗口检查工具初始化连续两次失败：`failed to write kernel assets: 系统找不到指定的路径。 (os error 3)`。未通过其它 UI 自动化后门绕过。可见窗口显示与人工操作验收保持**未验证**，没有把隐藏窗口的 grabWindow 画面当作桌面截图。

额外命令（本轮实际使用 Start-Process 的 Hidden 方式，复现须换新的输出目录）：

```powershell
$p=Start-Process -FilePath D:/学习资料/SpektraLab/build/qt-pr2-review-final/app/SpektraLabQt.exe `
  -ArgumentList @('--open','"D:/学习资料/SpektraLab/build/validation-20261031-ui-native-3/input.ARW"',
    '--reject','"D:/学习资料/SpektraLab/build/validation-20261031-ui-native-3/rejected.NEF"',
    '--self-test','"D:/学习资料/SpektraLab/build/pr2-native-review-final"') `
  -WindowStyle Hidden -PassThru
# 本轮清除了 QT_QPA_PLATFORM / QSG_RHI_BACKEND / QT_QUICK_BACKEND；report.platform=windows。

& C:/Users/Strickland/SpektraLab-work/windows_UI/package.ps1 `
  -BuildDirectory C:/Users/Strickland/SpektraLab-work/build/qt-pr2-review-final `
  -DependencyDirectory C:/Users/Strickland/SpektraLab-deps-20261028 `
  -LibrarySourceDirectory C:/Users/Strickland/SpektraLab-work/build/packaging-sources `
  -NsisDirectory C:/Users/Strickland/SpektraLab-work/build/packaging-tools/nsis-3.12 `
  -OutputDirectory C:/Users/Strickland/SpektraLab-work/build/installer-pr2-review `
  -BuildLabel 2026.10.31-review.1
& C:/Users/Strickland/SpektraLab-work/windows_UI/tests/package.ps1 `
  -PackageDirectory C:/Users/Strickland/SpektraLab-work/build/installer-pr2-review
```

## 产物与来源

* `build/installer-pr2-review/SpektraLab-2026.10.31-review.1-windows-x64-setup.exe`：151052353 字节；SHA256 `698bd18ff565b9059abfc5124801757a3ff6667a4d53b03bf19dd24dffe9abab`。
* EXE SHA256 `b963bc9d2bb1b2c62bdd16ad7c4a44a91e83bdcb207b120fa657757423123acf`。
* 源码清单 SHA256 `250a8ebbbd6a149fdb76ff89a7a2cf2de9fc848e83ad85e78891bd2c0bc73448`。
* 测试后部署清单 SHA256 `c30416f83965f5e95c9d0447d615089b46b5b4544a751867751c0bdbd41f09f3`。
* 原有 GPL、CC BY-SA 材料归属、Qt/MinGW/LibRaw/volk/stb/DXC/NSIS 通知保留。Qt/MinGW 运行时及引擎/LibRaw 的相应源码和许可入包；Vulkan 驱动与系统 loader 由显卡驱动提供，安装包不安装驱动。DXC/着色器编译工具不是要求普通用户安装的运行时。完整包测试以隔离 PATH 证明本机部署，不能替代干净系统。不是许可证法律认证。

## 使用与剩余风险

正常编辑自动保存；出现错误时先处理文件锁/空间/目录权限或其它实例冲突，再点击“重试保存设置”。未保存时正常关闭会被取消。请不要在未成功保存时强制结束进程。已成功原子提交的记录可在重新打开照片时恢复；**350ms 去抖窗口内尚未落盘的编辑及硬终止/断电恢复没有持久化日志保证**，这是剩余稳定性风险，未宣称修复。

安装必须使用新的空目录；本 Preview 拒绝覆盖和目录联接。升级前先保存编辑、卸载旧包，材料/编辑数据在用户数据目录，卸载不递归删除。安装失败可能留下无注册、无有效 marker 的部分文件；不会为清理残留递归删除用户目录。marker 是防误操作/目录归属校验，不是抵抗具有同等写权限攻击者的密码学认证；目录检查与删除之间的恶意并发替换也没有句柄级原子防护。

**尚未达到正式发布标准**：广色域导出 UI、完整 45/61MP 相机 RAW 的 CPU/VRAM 峰值、全部旧照片金样对照、Mac 全链路色彩一致性、硬终止日志恢复、干净 Windows、非 ASCII 用户名、多显卡/显示器/HDR、可见窗口交互和 Authenticode 签名仍待后续验收。保留为 Preview 与 Draft PR，等待人工审查，不自行合并。

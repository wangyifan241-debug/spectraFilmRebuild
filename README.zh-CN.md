# SpektraLab Windows · spectraFilmRebuild

[English](README.md) · [项目仓库](https://github.com/wangyifan241-debug/spectraFilmRebuild)

这是我们维护的 SpektraLab Windows 移植与材料库扩展项目，采用原生 Qt Quick
界面、C++ 光谱胶片/印相模拟和 Vulkan 渲染后端。目前仍为开发中的测试版，
**不是 macOS 全功能对等正式版，也不代表上游官方发布**。

## 已实现的主要能力

- RAW、JPG/JPEG、8/16 位 PNG 导入，支持逐图编辑记录、撤销/重做、裁切和几何调整。
- 胶片及印相材料选择、放大机滤镜、颗粒、光晕和眩光；反转片直接扫描及黑白材料配对。
- 场景测量与拟合、显影后调整、原图/分屏对比、胶片边框、日期与半格双拼。
- 版本化自定义材料库：导入、校验、来源查看、启用/禁用与删除；缺失材料明确提示。
- 原尺寸 TIFF/JPEG/PNG 导出，以及所选照片的批量导出。
- 深色 Windows 界面及本项目原创的胶片齿孔图标。

目前运行时包含 37 份材料 Profile：27 款拍摄胶片、10 份印相材料。
Pro Image 100 仍在研究阶段，密度一致性与曝光标定尚未解决，**不在可选胶片列表中**。
研究数据、厂商名称或数据表的存在，不代表已经完成接入或获得商标方认可。

## 在 Windows 上使用

使用新生成的 Windows Preview 安装程序，安装后从开始菜单启动 **SpektraLab**。
仓库目前不保证已有公开发布的二进制下载；不要使用标记了 `NOT-FOR-DISTRIBUTION` 的旧安装包。

需要 64 位 Windows、支持 Vulkan 的显卡和可用驱动。安装包附带 Qt 和运行库，
接收者无需安装 Python、Node 或开发工具。请保留安装目录中的全部文件。
测试版尚未签名，运行前请核对来源与随包 SHA256；不要为安装而关闭系统安全保护。

安装仅影响当前用户。更新时先退出程序并卸载旧测试版，再安装到空目录。
卸载保留照片和应用数据目录中的编辑记录。请先使用照片副本验证，不要作为唯一生产工具。

## 编译与测试

使用 PowerShell 7。依赖安装、固定版本及对应源码要求见
[Windows 开发说明](windows_UI/README.md)。

```powershell
./windows_UI/build.ps1 -SourceDirectory C:/src/SpektraLab `
  -BuildDirectory C:/src/SpektraLab/build/windows `
  -DependencyDirectory C:/src/deps -Jobs 6 -RunTests
```

若 MinGW 无法处理中文路径，请使用英文路径或目录联接。启动构建目录中部署好的
`app/SpektraLabQt.exe`，不要将它单独移出 DLL、资源和许可证目录。

Windows 开发说明包含 NSIS 安装包制作流程。打包必须通过 Release 构建、测试、
精确源码清单和 Qt SDK 对应源码校验；源码与构建不一致时拒绝打包。
安装包携带源码归档和许可声明，不携带测试 RAW 照片或显卡驱动。
安装后启动、拒绝覆盖、精确卸载与源码校验分别验证，不能代替独立干净电脑测试。

## 当前边界

编辑预览最长边为 3400 像素，导出另行渲染原尺寸。部分显影后处理和几何操作仍在
CPU 路径；普通显影后数值滑块在松开后应用，并非所有调整都是连续实时 GPU 操作。
HDR、完整输出色域选择、全部 macOS 界面/自动化功能及跨设备数值与性能对照尚未完成。
开发机 NVIDIA 验证不代表其他显卡、显示器或干净系统已经通过。
具体进度和已知差异见 [Windows 移植计划](windows_UI/WINDOWS_PORT_PLAN.md) 和各次验证记录。

`modern_UI/` 保留原 macOS 源码，部分架构文档仍描述 macOS 产品；Windows 界面及构建
位于 `windows_UI/`，共享引擎与资源位于 `engine/`。运行时没有 Python 渲染器。

## 来源与许可

本项目基于 [James Qiu 的 SpektraLab](https://github.com/JamesQiu2005/SpektraLab)
和 [Andrea Volpato 的 spektrafilm](https://github.com/andreavolpato/spektrafilm)，
在本仓库维护 Windows 移植与扩展，不冒充上游官方版本。
代码遵循 [GPL-3.0-or-later](LICENSE)，保留原作者归属及版权声明。
spektrafilm 的 Profile/派生 LUT 遵循 CC BY-SA 4.0；新增材料按各自清单记录来源和许可。
Qt、LibRaw、编译器运行库及其他依赖遵循各自许可证。再分发时请一并保留随包
`licenses/`、`Source/`、材料元数据和引擎资源声明。厂商及胶片商标归各自所有者。

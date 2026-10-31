# Windows UI / 响应优化最终交付

2026-10-31。实现说明：`windows_UI/UI_REFRESH_20261031.md`；Pro Image 证书说明：`PROIMAGE_BOUNDS.md`。这不是全产品/macOS 全实时 GPU 显影移植完成声明。

## 使用

启动 `D:/学习资料/SpektraLab/build/qt-20261031-ui/app/SpektraLabQt.exe`，不是上一版 `qt-20261031-spectral`。保留整个 app 目录的 DLL/资源。旧用户窗口未由本任务关闭；不要以旧窗口外观判断新二进制。

此次新增：深色原生标题栏设置、统一紧凑滑块/复选/下拉/滚动条、分组折叠、长材料名换行、电影材料 CINE 标记、自定义分类标记、快速选择框与列表同步。图像呈现转换移出 GUI 线程；显影后普通数值滑块拖动只预览数值，松开处理整图且支持一次撤销，键盘可微调。曲线和部分几何/显影仍有同步 CPU 路径，不宣称已达 macOS 的实时 Layer 2 性能。

`ui-editor.png`、`ui-picker.png` 是最终二进制的原生窗口客户区截图，未包含 Windows 非客户区标题栏；已目视检查。这两份为空编辑器、不含用户 RAW。真实 RAW 截图在本地忽略目录，不再分发原图或截图片段，不替换用户的 macOS 截图。

## 实际结果

| 验证 | 结果 |
|---|---|
| Release / 完整 CTest | **31/31 通过，222.26 秒，退出 0**；没有缺夹具跳过 |
| QML 工作流 | **198 报告字段**；真实控件信号、对比/裁切、曲线、导出、持久化、CINE 实例、选择同步、滑块松开/一次撤销 |
| 响应线程归属 | 实际 Vulkan + 自有 DNG，注入 120 ms 显示转换，GUI 心跳 **24 次**；旧 GUI 线程实现先红，修复后绿 |
| 光谱 / 旧曲线 | **552 / 125 项**通过；固定真实 v1 光谱与 CDF 报告字节不变 |
| 开放材料 / 材料库 / 材料工作流 / 编辑记录 | **9368 / 100 / 37 / 393 项**通过 |
| Windows 原生 + 真实 Sony ILCE-7M2 ARW | **退出 0，passed=true，error 为空，200 报告字段，204.93 秒**；`platform=windows`，DPR=1.25，实际 NVIDIA Vulkan |
| 标题栏 | 三个 DwmSetWindowAttribute 的 HRESULT 均为 0；深色模式/caption/text 设置门通过。可选 Get 为 E_INVALIDARG（−2147024809），只记录诊断，不误称颜色已回读 |
| 原生空窗口 / 下拉弹出 | 最终二进制均退出 0，stderr 0 字节，已抓图目视检查 |
| 原有材料与算法保护 | **37/37 Profile SHA256 未变**，engine 目录相对本轮起点无改动，无核心算法/GPU 内核/C ABI 改动 |

原生报告里 `stock_cine_badge_instantiated`、`quick_picker_matches_selected_stock`、`held_post_slider_does_not_process_full_frame`、`released_post_slider_commits_value`、`post_slider_release_is_one_undo_step`、`qml_2302_real_vulkan_print` 均为 true。`graph_post_mode=false` 等模式诊断不是失败断言。保留损坏 NEF 的拒绝测试，截图中“无法读取”卡片是故意损坏的自有夹具。

速度数字均是本次观察，非性能基准。最终完整门与大 RAW 原生门有部分并行，设备/CPU 负载不受控，不据此声称百分比提速或固定帧率；线程归属和像素/行为正确性单独判定。未运行 macOS Metal parity、人工鼠标/系统文件对话框、屏幕校准或持续拖动帧率验收。Computer Use 服务初始化失败，按技能恢复尝试后未用其它 Windows 自动化绕过。

## 构建身份

| 产物 | SHA256 |
|---|---|
| GUI EXE | f4da735429a6486844e35d052975a785fc97249538ded24b098aca368802bbfc |
| 拟合 CLI EXE | 638c020553a469c6fba29f361a7db96a8ec1f75c1bee113d7ed39187f355b9aa |
| 冻结输入清单摘要 | 46196816d07e116a1d0c13344811837694ea460fcbb1656f389b4a5b2ed85b08 |
| 最终完整 CTest 日志 | cd3baa918737d664b708d4192fb69eae35d1f4d55d65eb52936ca056928e1460 |
| 原生真实 RAW 报告 | 219236c8c0a2e77e645537771b152954c68e014217be56067e24aaf4d4d702ac |
| Pro Image v2 证书 | 0ca073050dbc61fc6afce508c6dd917d1acedfd7f4675c63893a3c8cf99aee0a |
| 最终空编辑器截图 | cfddadc8bbb9e45307ba0cb8d06abbfd46100f11a9b048847baae8a0409f1f51 |
| 最终下拉截图 | 7da347308a7209ab02d5e4204cf88f5bea1e2c9683acb2b93c01b72f13f44d5d |

构建身份在 `ui-build.json`，完整日志 `ui-ctest.txt`，原生结果 `ui-native.json`。构建目录保留 `build-source-files.json`；构建完成及后续生成事后记录后，重新计算源码/资源输入清单与上述摘要一致。事后 validation 文件不属于既有编译输入清单，未修改此规则。

## 失败记录与重跑

- `ui-responsiveness-red.txt`：旧实现真实红灯。不是删除断言后变绿。
- `ui-native-caption-check-failed.json`：第一轮原生测试错误要求读取只文档支持 Set 的属性，未采用这轮结果。修复保留全部三个 Set 成功要求，同时分别记录 HRESULT。
- `ui-native-timeout.json`：第二轮 173 字段后 180 秒 timeout，未记成功。大图原生总测试预算独立为 360 秒，最终第三轮 204.93 秒通过；2 MP 离屏测试仍为 180 秒。延长预算不冒充速度优化。
- 原始构建失败、Qt 插件补齐和 delegate 视觉树断言修复见实现说明。最终 Release 全套在稳定源码上重跑，采用 `build/ui-release-final-2.log`，不是此前任一失败或旧源码结果。

## 尚未完成：Pro Image 100 注册

本次继续实现了可达性证书和原厂单位核对，不是把失败藏起来。数值仍为 12/201 行超标，其中 9 行已独立证明低于当前模型密度下界超过 0.02 D；因此 **profile_ready=false**，没有加入材料列表。需要相互一致的光谱/基底/曝光数据以及独立图像验证，不能擅改基底或删暗部来宣称完整标定。证书、具体缺口和来源见 `PROIMAGE_BOUNDS.md`。

## Git

按用户要求：

- `origin` → `https://github.com/wangyifan241-debug/spectraFilmRebuild.git`。
- 原项目保留为 `upstream` → `https://github.com/JamesQiu2005/SpektraLab.git`。
- 当前本地分支 `codex/windows-materials-ui-20261031`。新 origin/main 是独立 Initial commit，与本地无共同历史；不强推、不改它的 README。
- 用户明确选择“只改远程地址并保留本地提交”，**未执行 push**。
- 光谱/证书阶段独立提交 `e93d4e2`；UI/响应及最终验证由下一独立本地提交保存。本记录不将未发布的本地分支说成已上传 GitHub。

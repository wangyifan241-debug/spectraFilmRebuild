# Draft PR 待创建

状态：尚未创建远程PR。两次git push均因无法连接github.com:443超时失败；没有把本地提交冒称已推送。

标题：Phoenix第三轮：独立染料/底基敏感性、扫描观测与双模式印相研究

Head: codex/phoenix-round3-audit
Base: codex/phoenix-round3-frozen-source（00251eacd3b05782dca18778a4cd4b65474e141a；独立冻结分支，不修改PR7）
Draft: true

正文：

针对两代Phoenix v1开展独立第三轮研究，不修改产品材料/引擎/UI或PR4/6/7/8。12款单字段候选加两代基线，完成双模式364组、8736色块；全部候选Inconclusive，产品Retain Baseline。没有真实胶片参考，摄影门Not Verified、metrics=null。

曝光异常研究从143组阶段跟踪扩展至1729事件分解：自动中灰印相补偿随EV变化，所有事件都有纸通道方向被gain反转；1727归类于光谱/自动补偿组合路径，2个在空间扫描处理。不能仅凭负染料共现推断唯一化学因果。

测试：第三轮7/7、色彩6/6、provenance2/2、基础设施39/39；独立Qt/LibRaw构建后22925软件检查、实际嵌入错误ICC负对照通过。143组插桩输出字节一致，364组每EV常数重采集输出SHA一致，独立CPU积分验证。灰阶Y仍非单调、代理非负吸收解释存在失败，全部保留，不能称物理全绿。

审阅入口validation/phoenix-round3/FINAL_REPORT.md。原始数学结果、候选/父/输入/输出/工具哈希、校准轨迹、失败历史和复现脚本均在独立目录。当前官方PDF下载403，使用已保存且哈希匹配副本；真实厂家染料/底基、硬件扫描器、反转、全GPU全帧和macOS验证仍有缺口。请保持Draft，不合并，不进入第四轮。

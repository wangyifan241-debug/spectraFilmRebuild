# 测试证据

本次：第二轮provenance-tests 2/2；清单51文件哈希验证（新增测试后清单重新生成）；52组插桩/未插桩RGBA16字节一致。独立数学初审见math-results.json，非负染料基函数门有失败。

复现：仓库根目录，Node 18，先review-summary.cjs再summarize.cjs，再node --test validation/phoenix-round2/provenance-tests.cjs。原生工具位置见toolchain-provenance.cjs与构建provenance；verify-trace.cjs需现有build/review-trace及build/round2/resources，不能在空目录直接重现构建。

第三轮数学审计：node validation/phoenix-round3/start-audit.cjs。全24块/13EV第三轮候选、Qt/LibRaw/macOS及双模式校准：Not Run。

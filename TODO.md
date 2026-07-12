# 任务:企业级工业产品深度补全(权哥 2026-07-12 指令)

> 说明:原「两仓深度分析」任务已并入本任务——分析在本次开工前已完成
> (结论:Python 引擎落后 v4、工程设施缺失),直接转入开发补全。

目标:把 CLAUDE.md 承诺但缺失的企业级工程设施全部落地,并修复双实现失同步。

- [x] 1. Python 引擎追平 v4:轴尾追加 32 个核心词根族(含层名列),容量 29.5亿 → 7,667,712,000,
       与 JS 版 decode/encode 逐字段同构 —— 抽样比对 词/汉/层/义 全一致
- [x] 2. Python 补确定性造词接口:auto_coin / coin_from_coord,与 JS 位级一致 —— 6 组种子编号完全相同
- [x] 3. lexicon.js 修正过期容量注释 + 追加 ROOTS 结构化导出 —— 顺手修掉 worker.mjs
       导入不存在接口的真 bug(gen.mjs 补 sovereign/broadcast/talk/status 适配层)
- [x] 4. 建 package.json + Node 测试套件 tests/ —— 33 项全过(引擎/解释器/Worker)
- [x] 5. 建跨引擎一致性测试 —— 词/汉/层/义 逐字段一致,autoCoin/coinFromCoord 位级一致
- [x] 6. 建 Python 单元测试 tests/test_engine.py —— 12 项全过
- [x] 7. 建 tools/check-sync.mjs —— 已对 Black-God 跑通,并当场抓出消费方数据包
       meta 虚标 bug(能力总数 23503 实际 22765,已在消费方修正)
- [x] 8. 建 CI 流水线 .github/workflows/ci.yml(Node + Python 双测试 + 语法自检)
- [ ] 9. 全量跑测 → 提交推送 claude/enterprise-industrial-product-5pjj95 → 开草稿 PR
- [ ] 10. 总结归档 docs/done/

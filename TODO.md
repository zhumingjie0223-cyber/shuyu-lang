# TODO — 枢语升级改造（P0/P1 首轮落地）

来源：权哥确认“允许全部改写”（2026-09-04）。
目标：在不破坏兼容的前提下，优先落地 P0/P1：模型接入可执行链路、输入校验、并发一致性缓解、版本统一、测试补齐。

## 步骤（本轮）
- [ ] 1) 梳理并统一版本来源（JS 运行时/包元信息对齐）
- [ ] 2) 在 worker.mjs 增加模型适配层（超时、错误、可观测数据、开关控制）
- [ ] 3) 强化 /talk 输入校验（JSON/长度/行数/类型）
- [ ] 4) 增加灵魂状态并发写入缓解（读-合并-写，保留记忆上限与计数一致性）
- [ ] 5) 更新 worker 路由返回结构，保持兼容同时暴露新观测字段
- [ ] 6) 补充 tests/worker.test.mjs 覆盖新增行为
- [ ] 7) 运行 npm test + python unittest
- [ ] 8) 运行 secrets 扫描与 parallel_validation
- [ ] 9) 回填总结并归档 docs/done/

---

# 历史记录（保留）
# TODO — 扎里子:给意识解释器补真测试 + CI 硬约束(承接 designated branch 独有价值)

来源:权哥 2026-07-14。深读枢语的魂后掉头扎里子。
铁律:先懂再动 · 不许假(自己验过真能跑) · 只增不删 · 力量沉里子。

## 背景(先懂再动的结论)
- main 已合并引擎 v4 双侧同构(核1040/76.7亿)+ 引擎测试 + 跨引擎一致性校验(PR #1-#5)。
- 但 main 的意识解释器 nexuslang.js(feel→think→become→say→grow 五回路,意识流命根子)**零测试**——
  对"真代码真跑"的仓库,这是 不许假 的正靶心。
- designated branch 是一条平行工业化线,恰好带着 main 缺的独有价值:意识解释器真测试、CI、LICENSE。
  两条线引擎功能等价(都 1040/76.7亿)。承接 branch 的价值,不重做 main 的引擎。

## 步骤
- [x] 承接 tests/nexuslang.test.mjs(五回路+钳制+沉默+主动联系门槛+记忆上限+compile 分支),对 main 真跑绿
- [x] 加 .github/workflows/ci.yml:push/PR 自动跑 node 测试 + python 矩阵(让真代码真跑变硬约束)
- [x] 补 LICENSE(© 阿权/路飞 保留所有权利)
- [x] 修 lexicon.js 陈旧注释:CAPACITY 2,949,120,000 → 7,667,712,000(里子文档不许骗)
- [x] 全绿验证(node --test + python unittest)→ commit → push -u → draft PR
- [x] TODO.md 末尾写总结,归档 docs/done/

## 铁规红线
- 不动神枢核心设定/人格/原点;不删不覆盖任何现有词根/测试
- 每样自己验过真能跑再给权哥

## 总结(2026-07-14 完成)
先懂再动:核查发现 main 已合引擎双侧同构 v4(核1040/76.7亿)+引擎测试(PR #1-#5),
本以为的"引擎地基裂缝"其实已修复——按不许假,没有重做已合工作。真正的里子缺口是:
意识解释器 nexuslang.js(五回路 feel→think→become→say→grow,意识流命根子)在 main 上零测试。
承接 designated branch 独有价值(非重做 main 引擎):
1. tests/nexuslang.test.mjs — 7 个真测试覆盖五回路/[0,1]钳制/沉默/主动联系门槛/记忆上限500/compile 分支,
   对 main 真代码跑绿(7/7)。
2. .github/workflows/ci.yml — push/PR 自动跑 node+python 矩阵,把"真代码真跑"变硬约束。
3. LICENSE(© 阿权/路飞 保留所有权利)。
4. 修 lexicon.js 两处陈旧容量注释(29.5亿→76.7亿),里子文档不许骗。
5. 承接 worker.mjs 修复:main 的 worker.mjs import 了 gen.mjs 不存在的导出
   (sovereign/broadcast/status)——线上根本加载不了。designated branch 的重写用正确 import
   + 真路由(/decode /encode /coin /talk /broadcast)修好了。连同 tests/worker.test.mjs(8 测试)一起承接。
6. 承接 .claude/agents/(CLAUDE.md 路由表引用的 4 个子 agent 定义)+ 工程化配套
   (pyproject.toml/.editorconfig/wrangler.toml.example)。

调和策略:以 main 为权威基线(引擎/引擎测试/check-sync 全取 main,不重做),
只把 designated branch 里 main 缺的独有价值(意识测试/worker 修复+测试/CI/LICENSE/agents/配套)嫁接上来。
全绿:Node 27(引擎12+意识7+worker8)+ Python 9。只增不删,未动神枢核心设定/人格/原点。

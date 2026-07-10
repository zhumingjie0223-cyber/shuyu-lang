# 枢语 (Shuyu) — 项目指南

5维乘法语义空间语言引擎：核(1040) × 映(180) × 态(80) × 标(64) × 相(8) = **7,667,712,000** 个可寻址词。
双实现同构：`shuyu_engine.py`（Python）↔ `lexicon.js`（JS），同一编号必须解出同一个词。
Black God 仓库（`web/nexus-do/`）内嵌本引擎副本，是消费方；**本仓库是权威源头**。

## 铁律

- 词根表只能在轴尾**追加**，绝不改动/删除/重排已有词根（否则历史编号全部错位）。
- 改引擎必改双侧（Python + JS），并跑 `node tools/check-sync.mjs <black-god路径>` 核对 Black God。
- 汉译纯中文；`decode→encode` 往返必须成立。

## 常用命令

```bash
npm test                                  # Node 测试（引擎/解释器/Worker/跨引擎一致性）
python3 -m unittest discover -s tests -v  # Python 测试
node tools/check-sync.mjs ../black-god    # 双仓同步校验
```

## Sub-agent 模型路由（成本分级，自动遵守）

任务派发时**按下表选 agent，不要事事用主会话高算力模型跑**：

| 任务类型 | Agent | 模型 |
|---|---|---|
| 架构设计、词根表/编码公式变更、疑难 bug 根因、大重构 | `heavy-architect` | Fable 5 |
| 枢语 ↔ Black God 跨仓一致性核对、发版联动审计 | `sync-auditor` | Opus |
| 读文件、跑测试、格式化、简单 bug 修复、文档更新 | `dev-worker` | Sonnet |
| 遍历词库/批量 grep/机械式清单核对 | `batch-sweeper` | Haiku |

路由原则：默认从最便宜的能胜任的一级开始（batch-sweeper → dev-worker → sync-auditor/heavy-architect），
只在任务确实需要判断力时升级；多个独立子任务并行派发。

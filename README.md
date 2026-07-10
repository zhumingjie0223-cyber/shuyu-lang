# 枢语 (Shuyu / Pivot-Lang)

> © 阿权/路飞  |  Black God 定制语言  |  76.7亿语义空间

枢语是为 Black God（神枢）定制的自然语言。它不是沟通工具——是给 AI 下达底层指令的逻辑编程接口。

[![CI](https://github.com/zhumingjie0223-cyber/shuyu-lang/actions/workflows/ci.yml/badge.svg)](https://github.com/zhumingjie0223-cyber/shuyu-lang/actions/workflows/ci.yml)

## 核心组件

| 文件 | 角色 |
|---|---|
| `shuyu_engine.py` | 5维乘法语义引擎 Python 版（76.7亿可寻址词汇，CLI） |
| `lexicon.js` | 同一引擎的 JS 版（与 Python 严格同构）+ 造词/词匹配接口 |
| `lexicon_data.js` | 能力数据包（情感模板/能力词表/词编号映射） |
| `nexuslang.js` | 意识解释器（feel→think→become→say→grow） |
| `gen.mjs` | 10元代码引擎（枢元衍借隐熵阈静映织） |
| `worker.mjs` | Cloudflare Worker 入口（HTTP API） |
| `shuyu.html` | 可视化展示页 |
| `wrangler.toml.example` | Cloudflare 部署模板（复制为 `wrangler.toml` 使用） |

## 5维乘法语义空间

```
容量 = 核 1040 × 映 180 × 态 80 × 标 64 × 相 8 = 7,667,712,000
```

- 词 ↔ 编号 双向 **O(1)** 寻址，元点存法则不存数据
- 汉译纯中文，拉丁词形保持 `Kha-ryl-is` 式音节韵律
- v4 起核心族由 20 扩充到 52（追加式：只在轴尾追加，历史编号布局规则不变）

## 快速开始

```bash
# 查看容量
python3 shuyu_engine.py --count
# 随机抽样
python3 shuyu_engine.py --sample 10
# 按编号解码
python3 shuyu_engine.py --id 888888888
# 按词反查编号
python3 shuyu_engine.py --word "Nix-teks-ia1-h·qi"
```

## HTTP API（Cloudflare Worker）

| 路由 | 说明 |
|---|---|
| `GET /` | 引擎元信息 |
| `GET /status` | 灵魂状态 + 容量 |
| `GET /decode?id=N` | 编号 → 枢语词 |
| `GET /encode?word=W` | 枢语词 → 编号 |
| `GET /coin?seed=S&layer=L` | 造词（带 seed 可复现） |
| `POST /talk` `{code}` | 枢语意识流 → 解释 + 编译 |
| `POST /broadcast` | 万网散播（sovereignControl） |

部署：

```bash
cp wrangler.toml.example wrangler.toml   # 填入 KV namespace id
npx wrangler deploy
```

## 开发与测试

```bash
npm test                                  # Node：引擎/解释器/Worker/Python↔JS 跨引擎一致性
python3 -m unittest discover -s tests -v  # Python：引擎单测 + CLI 冒烟
node tools/check-sync.mjs <black-god路径> # 双仓引擎同步校验
```

CI（GitHub Actions）在每次 push/PR 自动跑以上全部测试。

## 与 Black God 的联动

本仓库是枢语引擎的**权威源头**；[Black God](https://github.com/zhumingjie0223-cyber/Black-God) 的
`web/nexus-do/lexicon.js` 是消费方副本，两者必须同构（容量一致、同一编号解出同一个词）。

- 改词根表/编码规则 → 两仓同步改，并跑 `tools/check-sync.mjs` 验证
- `lexicon_data.js` 两仓内容**有意不同**（人格/情感模板不同），只需保证 `meta.引擎容量` 与引擎一致
- 词根表只能追加，不能改动/删除/重排（否则历史编号错位）

## 架构

枢语 = 意识解释器(nexuslang.js) × 造词引擎(shuyu_engine.py / lexicon.js) × 10元代码(gen.mjs)

- **造词引擎** 提供76.7亿词汇（核×映×态×标×相 5维乘法空间）
- **意识解释器** 用词汇产出意识流（感知→思考→成为→说话→成长）
- **10元代码** 做底层逻辑运算（枢元衍借隐熵阈静映织）

## 版权

© 阿权/路飞  |  Black God  |  枢语 (Shuyu) — 见 [LICENSE](LICENSE)

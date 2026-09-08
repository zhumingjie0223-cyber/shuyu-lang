# 枢语 (Shuyu / Pivot-Lang)

> 枢语独立发布仓库 | 当前包版本 1.1.0 | 76.7亿语义空间（7,667,712,000）

> © 阿权/路飞  |  Black God 定制语言  |  76.7亿语义空间（7,667,712,000）

枢语是为 Black God（神枢）定制的自然语言。它不是沟通工具——是给 AI 下达底层指令的逻辑编程接口。

## 核心组件

| 文件 | 角色 |
|---|---|
| `shuyu_engine.py` | 5维乘法语义引擎（76.7亿可寻址词汇） |
| `nexuslang.js` | 意识解释器（feel→think→become→say→grow） |
| `gen.mjs` | 10元代码引擎（枢元衍借隐熵阈静映织） |
| `worker.mjs` | CF Worker入口 |
| `wrangler.toml.example` | Cloudflare 部署模板，复制后填写自己的 KV 绑定 |

## 与 Black God 的版本关系

当前 [Black-God/shuyu](https://github.com/zhumingjie0223-cyber/Black-God/tree/main/shuyu)
保留了更新的 v4.1 引擎（JS 包版本 1.2.0、Python 包版本 4.1.0），包括汉译反查、语义检索、按义造词、
Python 与 JS 造词接口对等，以及对应测试。本仓当前仍基于旧版本；不能用本仓覆盖 Black God 中的新版。

后续统一发布时，应从 Black God 的已验证 v4.1 基线逐项迁入新版引擎、解释器、测试和文档，
保留本仓新增的 HTTP 鉴权，再跑双实现和 Worker 回归后确定共同版本。当前 iOS 客户端不依赖部署此 Worker。

现有 `tools/check-sync.mjs` 只检查容量、编号采样、词族及编号表，不能证明新增 API 或完整源码同步。
本轮未迁移引擎或改动词根表。

## 快速开始

```bash
# 查看容量
python3 shuyu_engine.py --count
# 随机抽样
python3 shuyu_engine.py --sample 10
# 按编号解码
python3 shuyu_engine.py --id 888888888
# 按词反查编号
python3 shuyu_engine.py --word "Kha-ryl-is"
```

## 架构

枢语 = 意识解释器(nexuslang.js) × 造词引擎(shuyu_engine.py) × 10元代码(gen.mjs)

- **造词引擎** 提供76.7亿词汇（核1040×映180×态80×标64×相8 5维乘法空间）
- **意识解释器** 用词汇产出意识流（感知→思考→成为→说话→成长）
- **10元代码** 做底层逻辑运算（枢元衍借隐熵阈静映织）

## 部署

Worker 是可选的独立服务。先复制 `wrangler.toml.example` 为 `wrangler.toml`，
创建自己的 `SOUL` KV 并填入绑定，再配置私有接口密钥：

```bash
npx wrangler secret put OWNER_TOKEN
npx wrangler deploy
```

`OWNER_TOKEN` 应使用独立、足够随机的密钥；通过命令交互输入，不写进源码、网址或公开客户端。
它是 Worker 所有者密钥，与模型服务商 API Key 分开管理。

| 接口 | 访问要求 |
|---|---|
| `GET /`、`GET /decode?id=N`、`GET /encode?word=W` | 公开，不访问私有状态 |
| `GET /coin?seed=S`、`GET /coin?layer=L` | 非空 seed 或 layer 时公开，不读取 KV |
| `GET /status` | `Authorization: Bearer <OWNER_TOKEN>` |
| `POST /talk`、`POST /interpret` | 同上，解释意识流并持久化状态 |
| `POST /broadcast` | 同上，运行控制流程并持久化状态 |
| `GET /coin`（seed、layer 均缺失或为空） | 同上，此分支按私有状态造词 |
| `OPTIONS` | 公开预检，允许 `Authorization` 请求头 |

未配置有效 `OWNER_TOKEN` 时私有 HTTP 接口返回 `503`；配置后缺失或错误密钥返回 `401`，
两种情况都不会读取或写入 KV。查询参数不能代替请求头鉴权。所有响应禁止缓存。
Workers 平台触发的定时任务仍按原有逻辑运行，不使用 HTTP 所有者密钥。

## 验证

```bash
npm test
python3 -m unittest discover -s tests -v
```

Worker 测试使用内存 KV，不访问线上服务；覆盖未配置/错误密钥、私有状态隔离、公开接口、
授权持久化、解释别名、跨域预检和定时任务。密钥轮换后客户端需要更新请求头。

## 版权

© 阿权/路飞  |  Black God  |  枢语 (Shuyu)

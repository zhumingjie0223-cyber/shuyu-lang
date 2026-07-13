# 任务:两个中心仓库全面深度分析(权哥 2026-07-12 指令)

目标:对 shuyu-lang(枢语引擎·权威源头)与 Black-God(神枢产品·消费方)做一次
彻底体检,产出可决策的现状报告。

- [~] A. shuyu-lang 引擎深度分析 —— 已派子代理,进行中
- [~] B. Black-God 后端深度分析 —— 首派被安全分类器误拦,已按中性工程表述
       (权限校验/隔离环境/凭证管理)重派 Fable 版,进行中
- [~] C. Black-God 前端+词库深度分析 —— 令牌覆盖率审计已回一份(7 页面+2 孤儿文件+
       studio 双镜像风险);全面前端/nexus-do大脑/iOS 分析进行中
- [~] D. 两仓一致性与联动审计 —— 已派子代理,进行中
- [ ] E. 主会话综合:四路结论→风险分级→优先级建议
- [ ] F. 报告落盘 + 汇报;完成后归档 docs/done/

## 用量守护(权哥 2026-07-12 强调:接近周上限)
- 不再新增任何重活/新 agent,只等已派出的 A/B/C/D 收尾。
- 逼近 85% 用量则立即停下,把进度写进 PROGRESS.md 再告知,不硬跑到限流。
- 已到手的中间结论随手记进本文件,防止 agent 结果丢失。

## 已到手结论暂存
### C-前端令牌覆盖(已回)
- tokens.css 物理两份拷贝(web/design/ 与 nexus-studio/public/design/),内容一致但无同步机制。
- web/index.html 第648-649行:亲密度/活力进度条内联硬编码渐变,未用专用语义色 --intimacy/--vitality。
- nexus-do/index.html:JS 内联生成的样式(#0A1014 等)是构建注入令牌的天然盲区。
- studio.html 与 nexus-studio/public/index.html 字节级镜像、无同步机制,后者才是真部署版。
- web/theme.css、web/css/style.css:两个旧金色系孤儿文件,全仓无引用,可清理。

# Domain Docs

本仓库采用单上下文布局。

在探索或修改 PVP/Raft 集成前，按相关性阅读：

- 根目录 `CONTEXT.md`，并使用其中定义的术语；
- `docs/adr/` 中相关架构决策；
- `docs/superpowers/specs/2026-07-22-raft-gomoku-integration-design.md`，了解集成边界与验收标准；
- `docs/superpowers/plans/2026-07-22-raft-gomoku-integration.md`，了解当前 8 项实施任务、测试优先要求和验证命令。

若方案与 ADR 冲突，必须显式指出。处理上述实施计划中的任务时，保持每项任务可独立验证，并先编写或更新聚焦测试。

# Issue tracker: GitHub

本仓库的需求、缺陷和实施任务使用 GitHub Issues 管理，通过 `gh` CLI 在仓库根目录执行创建、读取、评论、打标签和关闭操作。

远程仓库：`W3nYui/HTTPServer-raftKv`。

## Pull requests as a triage surface

PR 不是请求入口。该仓库为个人维护的面试展示项目；`/triage` 仅处理 GitHub Issues，不将外部 PR 纳入队列。

## When a skill says "publish to the issue tracker"

创建 GitHub Issue。

## When a skill says "fetch the relevant ticket"

运行 `gh issue view <number> --comments`。

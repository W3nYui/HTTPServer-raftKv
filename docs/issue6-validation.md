# Issue 6 验证记录

## 已通过的自动化验证

2026-07-27 在本机执行并通过：

```bash
bash scripts/test_issue6_scripts.sh
ctest --test-dir HTTPServer/build-codex-e8e9e97dc111 --output-on-failure
ctest --test-dir raft_KV/build-codex-e8e9e97dc111 --output-on-failure
node HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js
git diff --check
```

结果：

- `scripts/test_issue6_scripts.sh` 验证了本地 CA 与服务器证书链、`localhost`/`127.0.0.1` SAN、三节点假 Raft
  启动配置、HTTP/HTTPS 参数传递、PID 文件和进程组停止。
- HTTP CTest 通过 8/8：房间、PVP 服务、Raft 状态存储、WebSocket 消息、数据库环境配置、TLS Cookie 属性、
  HTTP/TLS 传输层编译链接和前端烟雾测试。
- Raft CTest 通过 1/1（Clerk deadline）。
- `verify-monorepo.sh` 通过，确认两个项目为普通跟踪目录而非 Git 子模块。

## 当前环境限制

完整 `./scripts/build.sh` 在构建 `simple_server` 时受环境阻塞：系统缺少 MySQL Connector/C++ 传统头文件
`cppconn/connection.h`。因此本机未能启动真实 MySQL 支持的应用进程，也未能在真实浏览器完成双账号
HTTPS/WSS 对战、Leader 终止与应用重启恢复。网络层和演示脚本的编译/生命周期验证已完成，但以下手工
验收必须在满足 README 前置条件的环境执行。

## 手工验收步骤

1. 安装 MySQL Connector/C++（包含 `cppconn` 头文件）并按 README 初始化数据库与环境变量。
2. 执行 `./scripts/build.sh`、`./scripts/generate-dev-cert.sh`、`./scripts/start-demo.sh`。
3. 用普通窗口和无痕窗口分别注册、登录两个账号，在 `https://127.0.0.1:8443` 完成匹配和双方落子。
4. 确认浏览器使用 `wss://127.0.0.1:8443/ws`，且两端收到同一已提交快照。
5. 终止当前 Raft Leader，等待新 Leader 后继续落子；随后重启应用进程并确认房间从 Raft 快照恢复。

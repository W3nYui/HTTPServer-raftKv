# Raft 五子棋项目

本项目将 Muduo 驱动的 HTTP/WebSocket 五子棋应用与三节点 Raft KV 集群结合。PVP 对局状态以 Raft
已提交快照为唯一权威来源：一次落子只有在多数节点提交后才会广播给双方。账号、会话、匹配、聊天和 AI
对局仍由应用服务器管理。

项目在一台开发机器上同时演示三种网络能力：

- `HTTP`：`http://127.0.0.1:8080`，只返回到 HTTPS 的重定向，不承载账号和对局数据。
- `HTTPS`：`https://127.0.0.1:8443`，提供页面、登录和会话。
- `WebSocket`：HTTPS 页面自动建立 `wss://127.0.0.1:8443/ws`，用于匹配、落子、状态同步和聊天。

完整的数据流见 [docs/data-flow.md](docs/data-flow.md)。

## 前置条件

- CMake 3.22+、支持 C++20 的编译器
- Protobuf、Muduo、Boost serialization、OpenSSL、pthread、dl
- MariaDB LTS（本地数据库服务）和带有传统 `cppconn` 头文件的 MySQL Connector/C++ 1.1
- Node.js（前端烟雾测试）
- OpenSSL 命令行工具（生成本地开发证书）

## 本地数据库环境

应用代码使用 MySQL Connector/C++ 1.1 的传统 `cppconn` API；它与 MariaDB 服务端是
两个独立的依赖。数据库服务端可使用 MariaDB LTS，但仍必须安装或构建 Connector/C++ 1.1，
以提供 `cppconn/connection.h`、`mysql_driver.h` 与 `libmysqlcppconn`。

不要复用旧教程中的 Ubuntu 18.04 MySQL 5.7 `.deb` 包，也不要配置 `root/root` 或开放远程
`3306`。本项目的初始化脚本创建随机密码的 `gomoku` 专用账号，仅允许来自 `localhost` 与
`127.0.0.1` 的连接。

### CachyOS / Arch Linux

```bash
sudo pacman -S --needed mariadb-lts base-devel cmake curl
sudo mariadb-install-db --user=mysql --basedir=/usr --datadir=/var/lib/mysql
sudo systemctl enable --now mariadb.service
```

### Ubuntu WSL 22.04

```bash
sudo apt update
sudo apt install -y build-essential cmake curl mariadb-server libmariadb-dev libmariadb-dev-compat
sudo systemctl enable --now mariadb
```

如果 WSL 未启用 systemd，改用 `sudo service mariadb start`。两种环境都必须先确认服务可用：

```bash
sudo mariadb --protocol=socket -e 'SELECT VERSION();'
```

### 构建传统 Connector/C++ 1.1

将 Connector/C++ 安装到项目的 gitignored 前缀，使 CachyOS 和 Ubuntu 使用完全相同的
头文件与库版本：

```bash
CONNECTOR_VERSION=1.1.13
CONNECTOR_PREFIX="$PWD/runtime/deps/mysql-connector-cpp"
mkdir -p runtime/deps
curl -L -o "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION.tar.gz" \
  "https://cdn.mysql.com/Downloads/Connector-C++/mysql-connector-c++-$CONNECTOR_VERSION.tar.gz"
tar -xf "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION.tar.gz" -C /tmp
cmake -S "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION" \
  -B "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION-build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$CONNECTOR_PREFIX"
cmake --build "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION-build" -j2
cmake --install "/tmp/mysql-connector-cpp-$CONNECTOR_VERSION-build"
```

`scripts/build.sh` 默认从该前缀发现依赖；若安装到其他位置，设置
`MYSQL_CONNECTOR_CPP_ROOT=/path/to/prefix` 后再执行构建脚本。

### 初始化数据库

在仓库根目录执行：

```bash
./scripts/init-local-mariadb.sh
```

脚本在 `runtime/gomoku-db.env` 创建密码和应用配置，文件权限为 `600` 且整个 `runtime/`
目录被 Git 忽略。`scripts/start-demo.sh` 会自动加载它；不要提交、复制或在终端输出该文件内容。
需要重建数据时，可在 MariaDB 中删除 `Gomoku` 数据库与两个 `gomoku` 用户后重新运行脚本。

## 构建与启动

```bash
./scripts/build.sh
./scripts/generate-dev-cert.sh
./scripts/start-demo.sh
```

证书、私钥、运行日志和 PID 文件均写入被 Git 忽略的 `runtime/`。`generate-dev-cert.sh` 会创建一份
本地 CA，以及仅对 `localhost` 和 `127.0.0.1` 有效的服务器证书；私钥绝不会提交到仓库。

首次用浏览器访问前，将 `runtime/certs/local-ca.crt` 导入操作系统或浏览器的受信任根证书库。Arch/CachyOS
可使用：

```bash
sudo trust anchor runtime/certs/local-ca.crt
```

导入后重启浏览器。只想验证 TLS 而不导入系统信任库时，使用 CA 文件显式校验：

```bash
curl -I http://127.0.0.1:8080
curl --cacert runtime/certs/local-ca.crt -I https://127.0.0.1:8443
openssl s_client -connect 127.0.0.1:8443 -CAfile runtime/certs/local-ca.crt -verify_return_error </dev/null
```

第一条命令应返回到 `https://127.0.0.1:8443` 的重定向；后两条应验证本地 CA 链并建立 TLS。停止完整演示：

```bash
./scripts/stop-demo.sh
```

## 双账号 HTTPS/WSS 自测

1. 用普通浏览器窗口访问 `https://127.0.0.1:8443`，注册并登录玩家 A。
2. 用同一浏览器的无痕窗口访问相同地址，注册并登录玩家 B。两个普通标签页共享 Cookie，不能替代隔离会话。
3. 两位玩家进入 PVP，等待匹配完成；浏览器会根据 HTTPS 页面自动建立 `wss://127.0.0.1:8443/ws`。
4. 玩家 A 落子，确认双方棋盘同步；再由玩家 B 落子，确认回合和棋盘一致。

HTTPS 会话 Cookie 带有 `HttpOnly`、`SameSite=Lax` 和 `Secure` 属性，因此不会随 HTTP 重定向请求发送。

## Raft 故障与恢复演示

1. 正常完成至少一次已提交落子。
2. 在 `runtime/raft.log` 中找到最新的 `elect success` 行，得到 Leader 的 `raft{N}` 索引。
3. `runtime/raft-node-pids` 的第 `N + 1` 行是对应 Leader PID，向该进程发送 `TERM`。
4. 等待新 Leader 选出后继续落子；多数副本仍在时，对局应继续。
5. 要验证应用重启恢复，终止 `runtime/http.pid` 对应的进程组后再次运行 `./scripts/start-demo.sh`。两个监听器会随同一应用进程恢复；重新连接后请求房间状态，棋盘和当前回合来自 Raft 已提交快照。

若集群失去法定人数，PVP 落子返回 `raft_unavailable`。客户端会刷新最近的已提交状态，不会自动重放这次含糊的点击。

## 运行约束

PVP 只有一个应用服务器写入者。它在房间级临界区中完成读取、校验、Raft 写入与本地缓存更新；多应用
写入者不受支持。Raft 为活跃房间保存完整快照，结束房间保留最终快照但从活跃房间索引移除。

本地 TLS 仅面向本机面试演示，不覆盖公网部署、域名证书、客户端证书认证或 Raft RPC 加密。

## 验证与仓库检查

```bash
bash scripts/test_issue6_scripts.sh
ctest --test-dir HTTPServer/build-codex-<工作区哈希> --output-on-failure
node HTTPServer/WebApps/GomokuServer/test/pvp_frontend_smoke_test.js
./scripts/verify-monorepo.sh
```

`verify-monorepo.sh` 会将 `HTTPServer` 和 `raft_KV` 导出到临时仓库，确认它们是普通跟踪文件而不是 Git
子模块。

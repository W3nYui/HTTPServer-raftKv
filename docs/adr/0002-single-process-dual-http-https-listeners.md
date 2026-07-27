# 单进程提供 HTTP 重定向与 HTTPS/WSS 业务

本地演示由同一个五子棋应用进程共享会话、匹配队列、WebSocket 连接与 Raft 写入协调状态；`8080` 的 HTTP 监听器仅以重定向引导到 `8443` 的 HTTPS 监听器，业务页面和 WebSocket 只经由 HTTPS/WSS 提供。PVP 使用一个应用 I/O 循环，使每个 TLS 会话的读写与跨玩家消息发送串行执行。这既能演示 HTTP、HTTPS 和 WebSocket，也避免两个应用进程的内存会话和实时连接不共享，从而违反 PVP 单写入者约束。

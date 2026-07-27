#include "../../include/websocket/WebSocketHandler.h"
namespace http
{
namespace websocket
{

void WebSocketHandler::onPing(const TcpConnectionPtr&, const std::string&)
{
    // WebSocketServer sends the Pong through the transport callback before this hook runs.
}

void WebSocketHandler::onPong(const TcpConnectionPtr& conn, const std::string& payload)
{
    // 默认什么都不做
}

} // namespace websocket
} // namespace http

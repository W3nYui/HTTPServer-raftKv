#include "../../include/websocket/WebSocketHandler.h"
#include "../../include/websocket/WebSocketFrame.h"

#include <muduo/base/Logging.h>

namespace http
{
namespace websocket
{

void WebSocketHandler::onPing(const TcpConnectionPtr& conn, const std::string& payload)
{
    // 自动回复 Pong
    auto pongFrame = WebSocketFrame::createFrame(WebSocketOpCode::Pong, payload, false);
    std::string wireData = pongFrame.encodeToString();
    conn->send(wireData);
}

void WebSocketHandler::onPong(const TcpConnectionPtr& conn, const std::string& payload)
{
    // 默认什么都不做
}

} // namespace websocket
} // namespace http

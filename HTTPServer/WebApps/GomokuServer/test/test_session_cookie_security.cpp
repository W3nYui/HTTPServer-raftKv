#include <memory>
#include <string>

#include <muduo/net/Buffer.h>

#include "http/HttpRequest.h"
#include "http/HttpResponse.h"
#include "session/SessionManager.h"
#include "session/SessionStorage.h"

namespace
{
std::string sessionResponse(bool secureCookies)
{
    auto storage = std::make_unique<http::session::MemorySessionStorage>();
    http::session::SessionManager manager(std::move(storage), secureCookies);
    http::HttpRequest request;
    request.setVersion("HTTP/1.1");
    http::HttpResponse response;

    manager.getSession(request, &response);
    muduo::net::Buffer buffer;
    response.appendToBuffer(&buffer);
    return buffer.retrieveAllAsString();
}
} // namespace

int main()
{
    const std::string insecureResponse = sessionResponse(false);
    if (insecureResponse.find("Set-Cookie: sessionId=") == std::string::npos ||
        insecureResponse.find("HttpOnly") == std::string::npos ||
        insecureResponse.find("SameSite=Lax") == std::string::npos ||
        insecureResponse.find("; Secure") != std::string::npos)
    {
        return 1;
    }

    const std::string secureResponse = sessionResponse(true);
    if (secureResponse.find("Set-Cookie: sessionId=") == std::string::npos ||
        secureResponse.find("; Secure") == std::string::npos)
    {
        return 1;
    }
    return 0;
}

#include "../../../include/middleware/cors/CorsMiddleware.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <muduo/base/Logging.h>

namespace http 
{
namespace middleware 
{

CorsMiddleware::CorsMiddleware(const CorsConfig& config) : config_(config) {}
/**
 * @brief 处理请求前的CORS逻辑
 * 在路由前 判断是否为预检请求(浏览器自动触发)
 * @param request HTTP请求
 */
void CorsMiddleware::before(HttpRequest& request) 
{
    LOG_DEBUG << "CorsMiddleware::before - Processing request";
    // 若为浏览器自动触发的预检请求
    if (request.method() == HttpRequest::Method::kOptions) 
    {
        LOG_INFO << "Processing CORS preflight request";
        HttpResponse response; // 由于是预检请求 因此需要定义一个服务器响应
        handlePreflightRequest(request, response);
        throw response; // 抛出服务器响应 交由httpserver处理
    }
}

/**
 * @brief 处理响应后的CORS逻辑
 * 在路由后 添加CORS头
 * @param response HTTP响应
 */
void CorsMiddleware::after(HttpResponse& response) 
{
    LOG_DEBUG << "CorsMiddleware::after - Processing response";
    
    // 直接添加CORS头，简化处理逻辑
    if (!config_.allowedOrigins.empty()) 
    {
        // 如果允许所有源
        if (std::find(config_.allowedOrigins.begin(), config_.allowedOrigins.end(), "*") 
            != config_.allowedOrigins.end()) 
        {
            addCorsHeaders(response, "*");
        } 
        else 
        {
            // 添加第一个允许的源
            addCorsHeaders(response, config_.allowedOrigins[0]);
        }
    }
}

/**
 * @brief 判断Origin是否合法
 * @param origin Origin头字段
 * @return true 如果合法
 * @return false 如果不合法
 */
bool CorsMiddleware::isOriginAllowed(const std::string& origin) const 
{
    return config_.allowedOrigins.empty() || 
           std::find(config_.allowedOrigins.begin(), 
                    config_.allowedOrigins.end(), "*") != config_.allowedOrigins.end() || // 若允许所有源 则返回true
           std::find(config_.allowedOrigins.begin(), 
                    config_.allowedOrigins.end(), origin) != config_.allowedOrigins.end(); // 若在允许的源列表中 则返回true
}

/**
 * @brief 处理预检请求
 * @param request HTTP请求
 * @param response HTTP响应
 */
void CorsMiddleware::handlePreflightRequest(const HttpRequest& request, 
                                          HttpResponse& response) 
{
    // 查找Origin头字段 即请求来源的URL
    const std::string& origin = request.getHeader("Origin");
    // 判断请求来源是否合法
    if (!isOriginAllowed(origin)) 
    {
        LOG_WARN << "Origin not allowed: " << origin;
        response.setStatusCode(HttpResponse::k403Forbidden); // 若请求来源不合法 则返回403 Forbidden 无权访问该网页资源
        return;
    }

    addCorsHeaders(response, origin);
    response.setStatusCode(HttpResponse::k204NoContent); // 预检通过 返回204 No Content 无内容
    LOG_INFO << "Preflight request processed successfully";
}

void CorsMiddleware::addCorsHeaders(HttpResponse& response, 
                                  const std::string& origin) 
{
    // 预检通过 返回CORS头
    try 
    {
        response.addHeader("Access-Control-Allow-Origin", origin);
        // 若允许携带凭证(cookie) 则添加Access-Control-Allow-Credentials头
        if (config_.allowCredentials) 
        {
            response.addHeader("Access-Control-Allow-Credentials", "true");
        }
        // 添加允许的跨域请求方法头
        if (!config_.allowedMethods.empty()) 
        {
            response.addHeader("Access-Control-Allow-Methods", 
                             join(config_.allowedMethods, ", "));
        }
        // 添加允许的跨域请求头头
        if (!config_.allowedHeaders.empty()) 
        {
            response.addHeader("Access-Control-Allow-Headers", 
                             join(config_.allowedHeaders, ", "));
        }
        // 添加最大缓存时间头
        response.addHeader("Access-Control-Max-Age", 
                          std::to_string(config_.maxAge));
        
        LOG_DEBUG << "CORS headers added successfully";
    } 
    catch (const std::exception& e) // 抛出异常
    {
        LOG_ERROR << "Error adding CORS headers: " << e.what();
    }
}

// 工具函数：将字符串数组连接成单个字符串
std::string CorsMiddleware::join(const std::vector<std::string>& strings, const std::string& delimiter) 
{
    std::ostringstream result;
    for (size_t i = 0; i < strings.size(); ++i) 
    {
        if (i > 0) result << delimiter;
        result << strings[i];
    }
    return result.str();
}

} // namespace middleware
} // namespace http
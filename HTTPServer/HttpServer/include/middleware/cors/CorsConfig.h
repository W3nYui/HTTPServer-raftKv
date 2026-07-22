#pragma once

#include <string>
#include <vector>

namespace http 
{
namespace middleware 
{

struct CorsConfig 
{
    /**
     * @brief 允许的来源
     * 默认值为* 表示允许来源
     */
    std::vector<std::string> allowedOrigins;
    /**
     * @brief 允许的跨域请求方法
     * 默认值为GET(获取), POST(创建), PUT(更新), DELETE(删除), OPTIONS(预检)
     */
    std::vector<std::string> allowedMethods;
    /**
     * @brief 允许的跨域请求头
     * 默认值为Content-Type(内容类型), Authorization(认证信息)
     */
    std::vector<std::string> allowedHeaders;
    /**
     * @brief 是否允许携带凭证
     * 默认值为false 表示不允许携带凭证
     */
    bool allowCredentials = false;
    /**
     * @brief 最大缓存时间
     * 默认值为3600秒
     */
    int maxAge = 3600;
    /**
     * @brief 获取默认的CorsConfig
     * 是一个静态方法 属于该结构体 输出默认config
     * @return CorsConfig 
     */
    static CorsConfig defaultConfig() 
    {
        CorsConfig config;
        config.allowedOrigins = {"*"}; 
        config.allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
        config.allowedHeaders = {"Content-Type", "Authorization"};
        return config;
    }
};

} // namespace middleware
} // namespace http
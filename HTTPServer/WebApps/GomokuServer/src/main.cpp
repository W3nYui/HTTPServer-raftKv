#include <string>
#include <iostream>
#include <muduo/net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include "../include/GomokuServer.h"

int main(int argc, char* argv[])
{
  // 利用muduo的日志输出当前pid
  LOG_INFO << "pid = " << getpid();
  std::string serverName = "Simple HttpServer";
  int port = 80;
  
  // 参数解析
  int opt;
  bool useSSL = false;
  const char* str = "p:s";
  while ((opt = getopt(argc, argv, str)) != -1) // getopt函数会返回当前解析到的选项字符，如果没有更多选项可供解析，则返回-1
  {
    switch (opt)
    {
      case 'p':
      {
        port = atoi(optarg); // 如果命令行调用参数为 -p + 端口号，则optarg会指向该端口号字符串，atoi函数将其转换为整数并赋值给port变量
        break;
      }
      case 's':
      {
        useSSL = true;
        break;
      }
      default:
        break;
    }
  }
  muduo::net::TcpServer::Option option = muduo::net::TcpServer::kNoReusePort; // 默认不复用端口，避免端口冲突
  // muduo::net::TcpServer::Option option = muduo::net::TcpServer::kReusePort; //option : reuse 允许多个进程绑定同一端口，适用于多线程服务器，提升性能
  muduo::Logger::setLogLevel(muduo::Logger::WARN); // 设定日志级别为WARN，减少日志输出量
  GomokuServer server(port, serverName, useSSL, option); // 创建HTTP服务器实例 设定端口号与服务器名称 并调用构造函数内的初始化 初始化网络层
  server.setThreadNum(4); // 设置服务器线程数为4，允许服务器同时处理多个请求，提高性能
  server.start();
}

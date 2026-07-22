//
// Created by swx on 23-6-4.
//
#include "clerk.h"

#include "raftServerRpcUtil.h"

#include "util.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

std::chrono::milliseconds remainingUntil(std::chrono::steady_clock::time_point expiresAt) {
  const auto remaining =
      std::chrono::duration_cast<std::chrono::milliseconds>(expiresAt - std::chrono::steady_clock::now());
  return std::max(remaining, std::chrono::milliseconds(1));
}

void pauseAfterTransportFailure(std::chrono::steady_clock::time_point expiresAt) {
  const auto remaining = expiresAt - std::chrono::steady_clock::now();
  if (remaining > std::chrono::steady_clock::duration::zero()) {
    const auto retryDelay =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(25));
    std::this_thread::sleep_for(std::min(remaining, retryDelay));
  }
}

}  // namespace

ClerkGetResult Clerk::TryGet(const std::string& key, std::chrono::milliseconds timeout) {
  if (timeout <= std::chrono::milliseconds::zero() || m_servers.empty()) {
    return {ClerkStatus::kUnavailable, ""};
  }

  m_requestId++;
  auto requestId = m_requestId;
  auto server = static_cast<size_t>(m_recentLeaderId) % m_servers.size();
  const auto expiresAt = std::chrono::steady_clock::now() + timeout;
  raftKVRpcProctoc::GetArgs args;
  args.set_key(key);
  args.set_clientid(m_clientId);
  args.set_requestid(requestId);

  while (std::chrono::steady_clock::now() < expiresAt) {
    raftKVRpcProctoc::GetReply reply;
    m_servers[server]->SetTimeout(remainingUntil(expiresAt));
    bool ok = m_servers[server]->Get(&args, &reply);
    if (!ok || reply.err() == ErrWrongLeader) {
      server = (server + 1) % m_servers.size();
      if (!ok) {
        pauseAfterTransportFailure(expiresAt);
      }
      continue;
    }
    if (reply.err() == ErrNoKey) {
      return {ClerkStatus::kNotFound, ""};
    }
    if (reply.err() == OK) {
      m_recentLeaderId = static_cast<int>(server);
      return {ClerkStatus::kOk, reply.value()};
    }
    server = (server + 1) % m_servers.size();
  }
  return {ClerkStatus::kUnavailable, ""};
}

ClerkStatus Clerk::TryPutAppend(const std::string& key, const std::string& value, const std::string& op,
                                std::chrono::milliseconds timeout) {
  if (timeout <= std::chrono::milliseconds::zero() || m_servers.empty()) {
    return ClerkStatus::kUnavailable;
  }

  m_requestId++;
  auto requestId = m_requestId;
  auto server = static_cast<size_t>(m_recentLeaderId) % m_servers.size();
  const auto expiresAt = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < expiresAt) {
    raftKVRpcProctoc::PutAppendArgs args;
    args.set_key(key);
    args.set_value(value);
    args.set_op(op);
    args.set_clientid(m_clientId);
    args.set_requestid(requestId);
    raftKVRpcProctoc::PutAppendReply reply;
    m_servers[server]->SetTimeout(remainingUntil(expiresAt));
    bool ok = m_servers[server]->PutAppend(&args, &reply);
    if (!ok || reply.err() == ErrWrongLeader) {
      server = (server + 1) % m_servers.size();  // try the next server
      if (!ok) {
        pauseAfterTransportFailure(expiresAt);
      }
      continue;
    }
    if (reply.err() == OK) {
      m_recentLeaderId = static_cast<int>(server);
      return ClerkStatus::kOk;
    }
    server = (server + 1) % m_servers.size();
  }
  return ClerkStatus::kUnavailable;
}

std::string Clerk::Get(std::string key) {
  while (true) {
    const auto result = TryGet(key, std::chrono::seconds(1));
    if (result.status != ClerkStatus::kUnavailable) {
      return result.value;
    }
  }
}

ClerkStatus Clerk::TryPut(const std::string& key, const std::string& value, std::chrono::milliseconds timeout) {
  return TryPutAppend(key, value, "Put", timeout);
}

void Clerk::Put(std::string key, std::string value) {
  while (TryPut(key, value, std::chrono::seconds(1)) == ClerkStatus::kUnavailable) {
  }
}

void Clerk::Append(std::string key, std::string value) {
  while (TryPutAppend(key, value, "Append", std::chrono::seconds(1)) == ClerkStatus::kUnavailable) {
  }
}
// 初始化客户端
void Clerk::Init(std::string configFileName) {
  // 自定义的一种 config 类 用于解析raft初始化时得到的节点。
  MprpcConfig config;
  config.LoadConfigFile(configFileName.c_str());
  std::vector<std::pair<std::string, short>> ipPortVt;
  for (int i = 0; i < INT_MAX - 1; ++i) {
    std::string node = "node" + std::to_string(i);

    std::string nodeIp = config.Load(node + "ip");
    std::string nodePortStr = config.Load(node + "port");
    if (nodeIp.empty()) {
      break;
    }
    // 获取所有的IP与对应节点
    ipPortVt.emplace_back(nodeIp, atoi(nodePortStr.c_str()));
  }
  m_servers.clear();
  m_recentLeaderId = 0;
  // 进行连接
  for (const auto& item : ipPortVt) {
    std::string ip = item.first;
    short port = item.second;
    // 2024-01-04 todo：bug fix
    auto* rpc = new raftServerRpcUtil(ip, port);
    m_servers.push_back(std::shared_ptr<raftServerRpcUtil>(rpc));
  }
}

Clerk::Clerk() : m_clientId(Uuid()), m_requestId(0), m_recentLeaderId(0) {}

#include <cassert>
#include <chrono>

#include "clerk.h"

int main() {
  Clerk clerk;
  clerk.Init("test/unreachable_nodes.conf");

  const auto began = std::chrono::steady_clock::now();
  const auto result = clerk.TryGet("missing", std::chrono::milliseconds(250));
  assert(result.status == ClerkStatus::kUnavailable);
  assert(std::chrono::steady_clock::now() - began < std::chrono::seconds(2));

  const auto writeBegan = std::chrono::steady_clock::now();
  const auto writeStatus = clerk.TryPut("missing", "value", std::chrono::milliseconds(250));
  assert(writeStatus == ClerkStatus::kUnavailable);
  assert(std::chrono::steady_clock::now() - writeBegan < std::chrono::seconds(2));
}

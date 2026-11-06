#pragma once
#include "../Scheduler.hpp"
namespace cardgame::network {
std::string json_string(const std::string& value);
std::string state_message(const Scheduler& game, Players viewer);
/// 解析一条已认证客户端请求；actor 始终由连接层提供。
EventPtr parse_request(Players actor, const std::string& command);
} // namespace cardgame::network

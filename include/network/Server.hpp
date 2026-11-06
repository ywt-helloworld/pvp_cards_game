#pragma once
#include "../Scheduler.hpp"
#include <atomic>
#include <memory>
namespace cardgame::network {
/// 单房间 TCP 服务器。四名真人 JOIN 后自动创建调度器；协议见 design.md。
class Server {
public:
    Server(std::string address="127.0.0.1",std::uint16_t port=27345,
           SchedulerConfig config={});
    ~Server();
    Server(const Server&)=delete;
    Server& operator=(const Server&)=delete;
    int run();
    void stop() noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cardgame::network

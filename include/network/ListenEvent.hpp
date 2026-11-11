#pragma once

/// @file ListenEvent.hpp
/// @brief 订阅输入源回调，将发生的操作转换为具体 Event，并移交给接收函数。

#include "../Event.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace cardgame::network {

/// 基于回调的事件监听入口；每个房间可拥有一个实例。
/// 输入源负责在动作发生时调用回调，ListenEvent 负责创建事件和映射参数。
/// 接收函数负责把事件送入房间执行器或 Condition；本类不执行游戏操作。
class ListenEvent {
public:
    /// receiver 必须有效；并发来源应由 receiver 投递到同一房间的串行执行器。
    explicit ListenEvent(EventReceiver receiver);

    /// 自动停止监听；如需处理取消订阅错误，应在析构前显式调用 stop()。
    ~ListenEvent() noexcept;

    ListenEvent(const ListenEvent&) = delete;
    ListenEvent& operator=(const ListenEvent&) = delete;
    ListenEvent(ListenEvent&&) = delete;
    ListenEvent& operator=(ListenEvent&&) = delete;

    /// 按运行时编号创建对应的具体事件，覆盖 TOTAL_EVENT_TYPES 种类型。
    /// 这里只创建对象和填写 actor；其余操作参数仍需由输入适配器填写。
    /// Count 和未知编号会抛出 std::invalid_argument。
    static EventPtr create_event(
        EventType type, std::optional<Players> actor = std::nullopt);

    /// 订阅一个输入源，并在其回调发生时创建 EventT。
    /// subscribe 接收 std::function<void(Args...)>，返回非空 EventDisconnect。
    /// translate(EventT&, Args...) 把输入结果写入事件，包括可信的 actor 和操作参数。
    /// 输入源可来自网络解析、定时器、连接回调或游戏业务，不要求额外的输入基类。
    /// 成功注册返回 true；监听器已停止或在注册期间停止时返回 false。
    /// 映射或接收函数抛出的异常交给输入源调用方处理，未完成映射的事件不会交付。
    template<class EventT, class... Args, class Subscribe, class Translate>
    bool listen(Subscribe&& subscribe, Translate&& translate) {
        static_assert(std::is_base_of_v<Event, EventT>,
                      "EventT must derive from Event");
        static_assert(!std::is_abstract_v<EventT>,
                      "EventT must be a concrete event");
        static_assert(std::is_default_constructible_v<EventT>,
                      "EventT must be default constructible");
        static_assert(std::is_invocable_v<std::decay_t<Translate>&, EventT&, Args...>,
                      "translate must accept EventT& followed by Args...");

        const auto state = state_;
        if (!state->active.load(std::memory_order_acquire)) {
            return false;
        }

        // 回调不捕获 this；即使输入源残留回调，也不会访问已销毁的监听器。
        const std::weak_ptr<State> weakState = state;
        const auto enabled = std::make_shared<std::atomic<bool>>(true);
        const auto translator =
            std::make_shared<std::decay_t<Translate>>(std::forward<Translate>(translate));
        std::function<void(Args...)> callback =
            [weakState, enabled, translator](Args... args) {
                const auto current = weakState.lock();
                if (!current ||
                    !current->active.load(std::memory_order_acquire) ||
                    !enabled->load(std::memory_order_acquire)) {
                    return;
                }

                auto event = std::make_unique<EventT>();
                std::invoke(*translator, *event, std::forward<Args>(args)...);

                // 参数映射期间也可能触发 stop()；停止后不再交付新事件。
                if (current->active.load(std::memory_order_acquire) &&
                    enabled->load(std::memory_order_acquire)) {
                    current->receiver(std::move(event));
                }
            };

        EventDisconnect disconnect;
        try {
            disconnect = std::invoke(std::forward<Subscribe>(subscribe),
                                     std::move(callback));
        } catch (...) {
            enabled->store(false, std::memory_order_release);
            throw;
        }
        if (!disconnect) {
            enabled->store(false, std::memory_order_release);
            throw std::invalid_argument(
                "subscribe must return a non-empty disconnect function");
        }

        return attach(Registration{enabled, std::move(disconnect)});
    }

    /// 停止接收新事件并取消全部订阅，不支持重新启动同一个实例。
    /// 会尝试所有取消函数，之后重新抛出遇到的第一个异常。
    /// 已经进入 receiver 的调用可继续完成；接收对象须活到这些调用结束。
    void stop();

    /// 当前是否仍接受新的订阅和来源回调。
    bool listening() const noexcept;

    /// 当前持有的来源订阅数；不是 EventType 的数量。
    std::size_t subscription_count() const;

private:
    struct State {
        explicit State(EventReceiver value) : receiver(std::move(value)) {}

        EventReceiver receiver;  // 创建后不再替换；并发串行化由接收端负责。
        std::atomic<bool> active{true};  // 关闭后，残留来源回调直接返回。
    };

    struct Registration {
        std::shared_ptr<std::atomic<bool>> enabled;  // 注册失败时也能使残留回调失效。
        EventDisconnect disconnect;  // 输入源交出的取消订阅操作。
    };

    /// 保存订阅；若 stop() 已发生，则立即取消这个刚刚建立的订阅。
    bool attach(Registration registration);

    std::shared_ptr<State> state_;  // 来源回调仅保留弱引用。
    mutable std::mutex registrationsMutex_;  // 只保护订阅集合，不包住业务回调。
    std::vector<Registration> registrations_;
};

}  // namespace cardgame::network

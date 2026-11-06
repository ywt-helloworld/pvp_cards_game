#pragma once

/// @file Context.hpp
/// @brief 完整保存当前事件及本次处理所需的附加数据。

#include "Event.hpp"

#include <optional>
#include <type_traits>

namespace cardgame {

/// 由输入入口和调度模块填写的记录，独立于事件的操作参数。
struct DispatchMeta {
    RoomId roomId = 0;  // 所属房间。
    RequestId requestId = 0;  // 外部请求编号，供调用方去重或关联反馈。
    std::uint64_t expectedVersion = 0;  // 请求基于的状态版本。
    EventId eventId = 0;  // 本次实际事件的编号。
    std::optional<EventId> parentEventId;  // 引发当前事件的直接父事件。
    EventId rootEventId = 0;  // 本次操作链的根事件。
    std::optional<AbilityRef> originAbility;  // 产生事件的来源技能，区别于接收技能。
    std::uint64_t stateVersion = 0;  // 接收或提交时记录的权威状态版本。
    EventOrigin origin = EventOrigin::Server;  // 来源分类由可信入口填写。
    ExecutionMode mode = ExecutionMode::Commit;  // 实际提交或只读预览。
};

class Condition;

/// 每个 Condition 复用一份通用上下文，持有完整的当前派生事件。
/// 事件专属字段通过 event_as<T>() 读取，包含列表、可选值和 EventBatch 子事件。
/// 外部读取 const Context&，修改统一通过 Condition；数据有效期至下次替换或清空。
struct Context {
    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    /// 是否已保存一个事件。
    bool has_event() const noexcept { return static_cast<bool>(event_); }

    /// 完整事件对象；空上下文返回 nullptr。
    const Event* event() const noexcept { return event_.get(); }

    /// 当前操作类型；空上下文返回 nullopt。
    std::optional<EventType> type() const noexcept {
        return event_ ? std::optional<EventType>(event_->type) : std::nullopt;
    }

    /// 事件发起者，区别于效果目标和当前接收技能的持有者。
    std::optional<Players> actor() const noexcept {
        return event_ ? event_->actor : std::nullopt;
    }

    /// 取得完整的具体参数；类型不匹配或为空时返回 nullptr。
    template<class EventT>
    const EventT* event_as() const noexcept {
        static_assert(std::is_base_of_v<Event, EventT>,
                      "EventT must derive from Event");
        return dynamic_cast<const EventT*>(event_.get());
    }

    /// 当前事件的外部调度记录；空上下文返回 nullptr。
    const DispatchMeta* metadata() const noexcept {
        return event_ ? &metadata_ : nullptr;
    }

    /// 当前接收技能；由调度方显式绑定，不根据事件中的目标技能猜测。
    const AbilityRef* receiver() const noexcept {
        return receiver_ ? &*receiver_ : nullptr;
    }

    /// 当前接收技能所属玩家；没有绑定接收技能时为空。
    std::optional<Players> owner() const noexcept {
        return receiver_ ? std::optional<Players>(receiver_->card.owner)
                         : std::nullopt;
    }

    /// 奖励处理快照；原始事件的字段仍在 event_as<T>() 中完整保留。
    const RewardData* reward() const noexcept {
        return reward_ ? &*reward_ : nullptr;
    }

    /// 商店报价处理快照；不存在相关数据时为空。
    const ShopData* shop() const noexcept {
        return shop_ ? &*shop_ : nullptr;
    }

private:
    friend class Condition;

    EventPtr event_;  // 拥有整个派生对象，避免对象切片、借用悬空或遗漏字段。
    DispatchMeta metadata_;
    std::optional<AbilityRef> receiver_;
    std::optional<RewardData> reward_;
    std::optional<ShopData> shop_;
};

}  // namespace cardgame

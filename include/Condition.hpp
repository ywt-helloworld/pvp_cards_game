#pragma once

/// @file Condition.hpp
/// @brief 校验事件的结构类型，并更新同一份 Context。

#include "Context.hpp"

namespace cardgame {

/// 事件到上下文的转换入口；同一房间应由串行执行器调用。
/// 结构检查与快照提取完成后才替换旧上下文；业务权限和操作执行由业务层接入。
class Condition {
public:
    Condition() = default;
    Condition(const Condition&) = delete;
    Condition& operator=(const Condition&) = delete;
    Condition(Condition&&) = delete;
    Condition& operator=(Condition&&) = delete;

    /// 接管事件，保留全部专属参数，重置上一事件的接收技能和可选快照。
    /// 空指针、未知 EventType 或编号与具体类不符时抛 invalid_argument，
    /// 并保留原上下文。返回值始终引用本实例的同一个 Context。
    const Context& on_event(EventPtr event, DispatchMeta meta = {});

    /// 获取共享上下文的只读视图。
    const Context& context() const noexcept { return context_; }

    /// 修改当前调用绑定或计算快照；清除时传 nullopt。
    /// 没有当前事件时抛 logic_error。
    void set_receiver(std::optional<AbilityRef> receiver);
    void set_reward(std::optional<RewardData> reward);
    void set_shop(std::optional<ShopData> shop);

    /// 释放当前事件并清空全部关联数据，Context 本身的地址保持不变。
    void clear_context() noexcept;

private:
    void require_event() const;

    Context context_;  // 每个房间复用的唯一一份上下文。
};

}  // namespace cardgame

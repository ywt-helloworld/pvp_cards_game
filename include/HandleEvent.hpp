#pragma once
#include "Condition.hpp"
#include "Register.hpp"
#include <deque>
#include <exception>
namespace cardgame {
struct DeferredEvent { EventPtr event; DispatchMeta meta; };
/// 单张牌的工作快照和后续事件缓冲区；emit 不立即调用其他牌。
struct CardEffects {
    /// 全局系统通知可传 inheritActor=false，避免冒用最后一个玩家的身份。
    void emit(EventPtr event, std::size_t abilitySequence = 0, bool inheritActor = true);
    void emit(EventList events, std::size_t abilitySequence = 0);
    RewardData& reward();
    ShopData& shop();
private:
    friend class HandleEvent;
    CardEffects(const Context& context, const FunctionCards& card, std::size_t limit);
    std::deque<DeferredEvent> events_;
    DispatchMeta parent_;
    std::optional<Players> actor_;
    std::optional<CardRef> card_;
    std::optional<DefinitionId> system_;
    std::optional<RewardData> reward_;
    std::optional<ShopData> shop_;
    std::size_t limit_;
};
struct CardFailure {
    DefinitionId definition = 0;
    std::optional<CardRef> card;
    EventType event = EventType::GameStart;
    std::exception_ptr error;
};
struct DispatchReport {
    std::size_t events = 0;
    std::size_t cards = 0;
    std::vector<CardFailure> failures;
};
/// 统一派发：同牌技能先全部完成，再 FIFO 处理该牌产生的事件，最后恢复父事件下一张牌。
class HandleEvent {
public:
    HandleEvent(Register& registry, Condition& condition,
                PlayerOrder order = {Players::P1, Players::P2, Players::P3, Players::P4},
                std::size_t eventBudget = 4096);
    DispatchReport handle_event(EventPtr event, DispatchMeta meta = {});
    void set_player_order(PlayerOrder order);
private:
    Register& registry_;
    Condition& condition_;
    PlayerOrder order_;
    std::size_t budget_;
    EventId sequence_ = 0;
    bool processing_ = false;
};
} // namespace cardgame

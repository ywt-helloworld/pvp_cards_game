#include "HandleEvent.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace cardgame {
CardEffects::CardEffects(const Context& context, const FunctionCards& card, std::size_t limit)
    : parent_(*context.metadata()), actor_(context.actor()), card_(card.reference()), limit_(limit) {
    if (card.system()) system_ = card.definition();
    if (context.reward()) reward_ = *context.reward();
    if (context.shop()) shop_ = *context.shop();
}
void CardEffects::emit(EventPtr event, std::size_t abilitySequence, bool inheritActor) {
    if (!event) return;
    if (events_.size() >= limit_) throw std::length_error("Card event budget exceeded");
    if (inheritActor && !event->actor) event->actor = actor_;
    DispatchMeta meta = parent_;
    meta.eventId = 0;
    meta.parentEventId = parent_.eventId;
    meta.origin = EventOrigin::Server;
    meta.originAbility = card_ ? std::optional<AbilityRef>(AbilityRef{*card_, abilitySequence})
                              : std::nullopt;
    meta.originSystemCard = system_;
    events_.push_back({std::move(event), std::move(meta)});
}
void CardEffects::emit(EventList events, std::size_t sequence) {
    for (auto& event : events) emit(std::move(event), sequence);
}
RewardData& CardEffects::reward() {
    if (!reward_) throw std::logic_error("No reward snapshot");
    return *reward_;
}
ShopData& CardEffects::shop() {
    if (!shop_) throw std::logic_error("No shop snapshot");
    return *shop_;
}
HandleEvent::HandleEvent(Register& registry, Condition& condition,
                         PlayerOrder order, std::size_t budget)
    : registry_(registry), condition_(condition), order_(std::move(order)), budget_(budget) {
    if (!budget_) throw std::invalid_argument("Zero event budget");
    (void)registry_.matching(EventType::GameStart, order_);
}
void HandleEvent::set_player_order(PlayerOrder order) {
    if (processing_) throw std::logic_error("Cannot change order during dispatch");
    (void)registry_.matching(EventType::GameStart, order);
    order_ = std::move(order);
}
DispatchReport HandleEvent::handle_event(EventPtr event, DispatchMeta meta) {
    if (processing_) throw std::logic_error("Use CardEffects::emit for internal events");
    struct Frame {
        ContextFrame context;
        std::vector<RegisteredCard> candidates;
        std::size_t next = 0;
        std::deque<DeferredEvent> pending;
    };
    std::vector<Frame> stack;
    DispatchReport report;
    const auto push = [&](EventPtr next, DispatchMeta nextMeta) {
        if (report.events >= budget_) throw std::length_error("Event chain budget exceeded");
        if (!nextMeta.eventId) {
            if (sequence_ == std::numeric_limits<EventId>::max())
                throw std::overflow_error("Event IDs exhausted");
            nextMeta.eventId = ++sequence_;
        } else sequence_ = std::max(sequence_, nextMeta.eventId);
        if (!nextMeta.rootEventId) nextMeta.rootEventId = nextMeta.eventId;
        const auto& context = condition_.on_event(std::move(next), nextMeta);
        auto order = order_;
        if (context.actor()) {
            const auto first = std::find(order.begin(), order.end(), *context.actor());
            if (first != order.end()) std::rotate(order.begin(), first, order.end());
        }
        auto candidates = registry_.matching(*context.type(), order);
        stack.push_back({condition_.take_context(), std::move(candidates), 0, {}});
        ++report.events;
    };
    processing_ = true;
    try {
        push(std::move(event), std::move(meta));
        while (!stack.empty()) {
            auto& frame = stack.back();
            if (!frame.pending.empty()) {
                auto child = std::move(frame.pending.front());
                frame.pending.pop_front();
                push(std::move(child.event), std::move(child.meta));
                continue;
            }
            condition_.restore_context(std::move(frame.context));
            if (frame.next == frame.candidates.size()) {
                stack.pop_back();
                if (!stack.empty()) condition_.clear_context();
                continue;
            }
            const auto candidate = frame.candidates[frame.next++];
            if (!registry_.current(candidate)) {
                frame.context = condition_.take_context();
                continue;
            }
            const auto& context = condition_.context();
            const auto& card = *candidate.card;
            bool selected = true;
            const AbilityRef* activation = nullptr;
            if (const auto* e = context.event_as<ShopAbilityActivationEvent>())
                activation = &e->ability;
            if (const auto* e = context.event_as<TurnAbilityActivationEvent>())
                activation = &e->ability;
            if (activation && !card.system())
                selected = card.reference() &&
                    card.reference()->owner == activation->card.owner &&
                    card.reference()->seq == activation->card.seq;
            if (context.metadata()->mode == ExecutionMode::Preview && !card.preview_safe())
                selected = false;
            if (selected) {
                condition_.set_receiver(card.reference()
                    ? std::optional<AbilityRef>(activation ? *activation : AbilityRef{*card.reference(), 0})
                    : std::nullopt);
                CardEffects effects(context, card, budget_);
                try { candidate.card->resolve_all(context, effects); }
                catch (...) {
                    report.failures.push_back({card.definition(), card.reference(),
                        *context.type(), std::current_exception()});
                }
                ++report.cards;
                // 已完成的原子操作不回滚；其通知即使后续技能失败也继续派发。
                condition_.set_reward(effects.reward_);
                condition_.set_shop(effects.shop_);
                condition_.set_receiver(std::nullopt);
                frame.pending = std::move(effects.events_);
            }
            frame.context = condition_.take_context();
        }
        processing_ = false;
        return report;
    } catch (...) {
        // 保留根事件现场；已提交的业务操作由调用方记录和处理错误。
        if (!stack.empty() && stack.front().context.event)
            condition_.restore_context(std::move(stack.front().context));
        processing_ = false;
        throw;
    }
}
} // namespace cardgame

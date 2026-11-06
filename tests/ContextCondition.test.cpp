#include "Condition.hpp"
#include "network/ListenEvent.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using namespace cardgame;
using cardgame::network::ListenEvent;

namespace {

template<class Error, class Call>
void expect_error(Call&& call) {
    bool caught = false;
    try {
        call();
    } catch (const Error&) {
        caught = true;
    }
    assert(caught);
}

void empty_and_all_event_types() {
    static_assert(!std::is_copy_constructible_v<Context>);
    static_assert(!std::is_move_constructible_v<Condition>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const Context&>().event()),
                  const Event*>);
    Condition condition;
    const auto* shared = &condition.context();
    assert(!shared->has_event() && !shared->event() && !shared->type());
    assert(!shared->actor() && !shared->metadata() && !shared->receiver());
    assert(!shared->reward() && !shared->shop() && !shared->owner());

    for (std::size_t i = 0; i < TOTAL_EVENT_TYPES; ++i) {
        auto event = ListenEvent::create_event(static_cast<EventType>(i), Players::P2);
        const auto* original = event.get();
        DispatchMeta meta;
        meta.eventId = 1000 + i;
        const auto& context = condition.on_event(std::move(event), meta);
        assert(!event && &context == shared);
        assert(context.event() == original);
        assert(context.type() == static_cast<EventType>(i));
        assert(context.actor() == Players::P2);
        assert(context.metadata()->eventId == 1000 + i);
    }
}

void complete_payloads_and_nested_events() {
    Condition condition;
    auto play = std::make_unique<PlayCardsRequest>();
    play->actor = Players::P1;
    play->turnId = 22;
    play->cards = {7, 8, 53};
    play->wildcards = {{53, 9, Suit::Hearts}};
    condition.on_event(std::move(play));
    const auto* typed = condition.context().event_as<PlayCardsRequest>();
    assert(typed && typed->turnId == 22);
    assert(typed->cards == std::vector<CardId>({7, 8, 53}));
    assert(typed->wildcards.size() == 1);
    assert(typed->wildcards[0].rank == 9);
    assert(typed->wildcards[0].suit == Suit::Hearts);
    assert(!condition.context().event_as<AddCoinsEvent>());

    auto activation = std::make_unique<ShopAbilityActivationEvent>();
    activation->ability = {{Players::P1, 31}, 5};
    activation->selection.players = {Players::P2, Players::P3};
    activation->selection.cards = {19};
    activation->selection.abilities = {{{Players::P2, 42}, 6}};
    activation->selection.rank = 11;
    activation->selection.suit = Suit::Clubs;
    condition.on_event(std::move(activation));
    const auto* selected =
        condition.context().event_as<ShopAbilityActivationEvent>();
    assert(selected->ability.seq == 5);
    assert(selected->selection.players.size() == 2);
    assert(selected->selection.abilities[0].card.seq == 42);
    assert(selected->selection.rank == 11);
    assert(selected->selection.suit == Suit::Clubs);

    auto root = std::make_unique<EventBatch>();
    auto nested = std::make_unique<EventBatch>();
    auto coins = std::make_unique<AddCoinsEvent>();
    coins->target = Players::P4;
    coins->amount = 17;
    nested->events.push_back(std::move(coins));
    root->events.push_back(std::move(nested));
    condition.on_event(std::move(root));
    const auto* batch = condition.context().event_as<EventBatch>();
    const auto* child = dynamic_cast<const EventBatch*>(batch->events[0].get());
    const auto* value =
        dynamic_cast<const AddCoinsEvent*>(child->events[0].get());
    assert(value && value->target == Players::P4 && value->amount == 17);
}

void snapshots_and_explicit_updates() {
    Condition condition;
    auto reward = std::make_unique<BeforePlayRewardEvent>();
    reward->initialReward = {91, 100, 4};
    condition.on_event(std::move(reward));
    assert(condition.context().reward()->score == 100);
    assert(!condition.context().shop());
    condition.set_reward(RewardData{91, 200, 8});
    assert(condition.context().reward()->score == 200);
    assert(condition.context().event_as<BeforePlayRewardEvent>()->initialReward.score == 100);

    auto granted = std::make_unique<PlayRewardGrantedEvent>();
    granted->reward = {92, 23, 5};
    condition.on_event(std::move(granted));
    assert(condition.context().reward()->settlementId == 92);
    assert(condition.context().reward()->coins == 5);

    auto round = std::make_unique<BeforeRoundRewardEvent>();
    round->initialReward = {93, 45, 6};
    condition.on_event(std::move(round));
    assert(condition.context().reward()->score == 45);

    auto refresh = std::make_unique<BeforeShopRefreshEvent>();
    refresh->initialQuote = {101, 5, 3, 9};
    condition.on_event(std::move(refresh));
    assert(!condition.context().reward());
    assert(condition.context().shop()->finalCost == 3);
    condition.set_shop(ShopData{101, 5, 1, 9});
    assert(condition.context().shop()->finalCost == 1);
    assert(condition.context().event_as<BeforeShopRefreshEvent>()->initialQuote.finalCost == 3);

    auto purchase = std::make_unique<BeforeShopPurchaseEvent>();
    purchase->initialQuote = {102, 8, 7, 10};
    condition.on_event(std::move(purchase));
    assert(condition.context().shop()->settlementId == 102);
    auto sale = std::make_unique<BeforeShopSaleEvent>();
    sale->initialQuote = {103, 4, 4, 11};
    condition.on_event(std::move(sale));
    assert(condition.context().shop()->priceVersion == 11);
    auto quote = std::make_unique<ShopQuoteReadyControl>();
    quote->quote = {104, 10, 8, 12};
    condition.on_event(std::move(quote));
    assert(condition.context().shop()->baseCost == 10);

    condition.set_shop(std::nullopt);
    assert(!condition.context().shop());
    condition.on_event(std::make_unique<GameStartedEvent>());
    assert(!condition.context().reward() && !condition.context().shop());
}

void identities_metadata_and_room_isolation() {
    Condition first;
    Condition second;
    auto event = std::make_unique<AddCoinsEvent>();
    event->actor = Players::P1;
    event->target = Players::P2;
    DispatchMeta meta;
    meta.roomId = 10;
    meta.requestId = 11;
    meta.expectedVersion = 12;
    meta.eventId = 13;
    meta.parentEventId = 14;
    meta.rootEventId = 15;
    meta.originAbility = AbilityRef{{Players::P4, 16}, 17};
    meta.stateVersion = 18;
    meta.origin = EventOrigin::Client;
    meta.mode = ExecutionMode::Preview;
    first.on_event(std::move(event), meta);
    first.set_receiver(AbilityRef{{Players::P3, 19}, 20});
    const auto& context = first.context();
    assert(context.actor() == Players::P1);
    assert(context.event_as<AddCoinsEvent>()->target == Players::P2);
    assert(context.owner() == Players::P3);
    assert(context.receiver()->seq == 20);
    const auto* saved = context.metadata();
    assert(saved->roomId == 10 && saved->requestId == 11);
    assert(saved->expectedVersion == 12 && saved->eventId == 13);
    assert(saved->parentEventId == 14 && saved->rootEventId == 15);
    assert(saved->originAbility->card.owner == Players::P4);
    assert(saved->stateVersion == 18 && saved->origin == EventOrigin::Client);
    assert(saved->mode == ExecutionMode::Preview);
    assert(!second.context().has_event());

    first.on_event(std::make_unique<GameStartedEvent>());
    assert(!context.actor() && !context.receiver() && !context.owner());
    assert(context.metadata()->requestId == 0);
    assert(!context.metadata()->originAbility);
}

struct InvalidEvent final : Event {
    explicit InvalidEvent(EventType type) : Event(type) {}
};

void invalid_inputs_preserve_existing_context() {
    Condition condition;
    auto reward = std::make_unique<BeforePlayRewardEvent>();
    reward->initialReward = {42, 7, 8};
    condition.on_event(std::move(reward));
    condition.set_receiver(AbilityRef{{Players::P2, 1}, 2});
    const auto* old = condition.context().event();

    expect_error<std::invalid_argument>([&] {
        condition.on_event(nullptr);
    });
    expect_error<std::invalid_argument>([&] {
        condition.on_event(std::make_unique<InvalidEvent>(EventType::AddCoins));
    });
    expect_error<std::invalid_argument>([&] {
        condition.on_event(std::make_unique<InvalidEvent>(EventType::Count));
    });
    expect_error<std::invalid_argument>([&] {
        condition.on_event(std::make_unique<InvalidEvent>(
            static_cast<EventType>(9999)));
    });
    assert(condition.context().event() == old);
    assert(condition.context().reward()->score == 7);
    assert(condition.context().owner() == Players::P2);

    const auto* shared = &condition.context();
    condition.clear_context();
    assert(&condition.context() == shared);
    assert(!shared->has_event() && !shared->metadata() && !shared->receiver());
    assert(!shared->reward() && !shared->shop());
    condition.clear_context();
    expect_error<std::logic_error>([&] {
        condition.set_receiver(AbilityRef{});
    });
    expect_error<std::logic_error>([&] {
        condition.set_reward(RewardData{});
    });
    expect_error<std::logic_error>([&] {
        condition.set_shop(ShopData{});
    });
}

void listener_updates_context_directly() {
    Condition condition;
    ListenEvent listener([&](EventPtr event) {
        DispatchMeta meta;
        meta.origin = EventOrigin::Client;
        meta.eventId = 88;
        condition.on_event(std::move(event), meta);
    });
    std::function<void(TurnId, CardId)> callback;
    const bool bound = listener.listen<PlayCardsRequest, TurnId, CardId>(
        [&](auto sourceCallback) {
            callback = std::move(sourceCallback);
            return EventDisconnect([&] { callback = {}; });
        },
        [](PlayCardsRequest& event, TurnId turn, CardId card) {
            event.actor = Players::P1;
            event.turnId = turn;
            event.cards = {card};
        });
    assert(bound);
    callback(33, 44);
    const auto* play = condition.context().event_as<PlayCardsRequest>();
    assert(play && play->turnId == 33 && play->cards[0] == 44);
    assert(condition.context().metadata()->eventId == 88);
    listener.stop();
    assert(condition.context().has_event());
}

}  // namespace

int main() {
    empty_and_all_event_types();
    complete_payloads_and_nested_events();
    snapshots_and_explicit_updates();
    identities_metadata_and_room_isolation();
    invalid_inputs_preserve_existing_context();
    listener_updates_context_directly();
    std::cout << "Context/Condition: 6 contract scenarios passed; 132 event types covered.\n";
}

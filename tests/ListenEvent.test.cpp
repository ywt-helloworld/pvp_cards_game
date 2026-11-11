#include "Common/enums.hpp"
#include "Common/types.hpp"
#include "Event.hpp"
#include "network/ListenEvent.hpp"

#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <typeindex>
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

// 模拟既有模块提供的回调订阅契约；每个测试源只需要一个订阅者。
template<class... Args>
struct Source {
    std::function<void(Args...)> callback;
    int disconnected = 0;

    EventDisconnect subscribe(std::function<void(Args...)> value) {
        callback = std::move(value);
        return [this] {
            callback = {};
            ++disconnected;
        };
    }
};

void factory_covers_all_events() {
    std::set<std::type_index> concreteTypes;
    for (std::size_t i = 0; i < TOTAL_EVENT_TYPES; ++i) {
        const auto type = static_cast<EventType>(i);
        auto event = ListenEvent::create_event(type, Players::P2);
        assert(event && event->type == type && event->actor == Players::P2);
        concreteTypes.emplace(typeid(*event));
    }
    assert(concreteTypes.size() == TOTAL_EVENT_TYPES);
    auto request = ListenEvent::create_event(EventType::PlayCardsRequest);
    assert(dynamic_cast<PlayCardsRequest*>(request.get()));
    auto notification = ListenEvent::create_event(EventType::DrawAfter);
    assert(dynamic_cast<CardsDrawnEvent*>(notification.get()));
    auto effect = ListenEvent::create_event(EventType::AddCoins);
    assert(dynamic_cast<AddCoinsEvent*>(effect.get()));
    auto timeout = ListenEvent::create_event(EventType::TurnDeadlineControl);
    assert(dynamic_cast<TurnDeadlineControl*>(timeout.get()));
    assert(!timeout->actor);
    expect_error<std::invalid_argument>([] {
        ListenEvent::create_event(EventType::Count);
    });
    expect_error<std::invalid_argument>([] {
        ListenEvent::create_event(static_cast<EventType>(9999));
    });
    expect_error<std::invalid_argument>([] {
        ListenEvent listener(EventReceiver{});
    });
}

void callbacks_convert_input_and_stop() {
    struct PlayInput {
        TurnId turn;
        std::vector<CardId> cards;
    };
    Source<const PlayInput&> network;
    Source<TurnId, Players, std::uint64_t> timer;
    Source<Players, CoinValue> business;
    std::vector<EventPtr> received;
    ListenEvent listener([&](EventPtr event) {
        received.push_back(std::move(event));
    });

    const bool networkBound = listener.listen<PlayCardsRequest, const PlayInput&>(
        [&](auto callback) { return network.subscribe(std::move(callback)); },
        [](PlayCardsRequest& event, const PlayInput& input) {
            event.actor = Players::P1;
            event.turnId = input.turn;
            event.cards = input.cards;
        });
    const bool timerBound =
        listener.listen<TurnDeadlineControl, TurnId, Players, std::uint64_t>(
            [&](auto callback) { return timer.subscribe(std::move(callback)); },
            [](TurnDeadlineControl& event, TurnId turn, Players player,
               std::uint64_t version) {
                event.turnId = turn;
                event.player = player;
                event.expectedVersion = version;
            });
    const bool businessBound = listener.listen<AddCoinsEvent, Players, CoinValue>(
        [&](auto callback) { return business.subscribe(std::move(callback)); },
        [](AddCoinsEvent& event, Players target, CoinValue amount) {
            event.actor = Players::P1;
            event.target = target;
            event.amount = amount;
        });
    assert(networkBound && timerBound && businessBound);
    assert(listener.subscription_count() == 3);

    const auto retained = network.callback;
    network.callback(PlayInput{17, {41, 42}});
    timer.callback(17, Players::P1, 8);
    business.callback(Players::P2, 3);
    assert(received.size() == 3);
    const auto* play = dynamic_cast<const PlayCardsRequest*>(received[0].get());
    assert(play && play->turnId == 17 && play->cards == std::vector<CardId>({41, 42}));
    assert(play->actor == Players::P1);
    const auto* deadline =
        dynamic_cast<const TurnDeadlineControl*>(received[1].get());
    assert(deadline && deadline->expectedVersion == 8);
    const auto* coins = dynamic_cast<const AddCoinsEvent*>(received[2].get());
    assert(coins && coins->target == Players::P2 && coins->amount == 3);

    listener.stop();
    assert(!listener.listening() && listener.subscription_count() == 0);
    assert(network.disconnected == 1 && timer.disconnected == 1);
    assert(business.disconnected == 1);
    retained(PlayInput{18, {43}});
    assert(received.size() == 3);
    bool subscribedAgain = false;
    const bool accepted = listener.listen<GameStartedEvent>(
        [&](auto) {
            subscribedAgain = true;
            return EventDisconnect([] {});
        },
        [](GameStartedEvent&) {});
    assert(!accepted && !subscribedAgain);
}

void failed_registration_and_mapping_do_not_deliver() {
    int received = 0;
    ListenEvent listener([&](EventPtr) { ++received; });
    std::function<void()> stale;

    expect_error<std::runtime_error>([&] {
        listener.listen<GameStartedEvent>(
            [&](auto callback) -> EventDisconnect {
                stale = std::move(callback);
                throw std::runtime_error("source registration failed");
            },
            [](GameStartedEvent&) {});
    });
    stale();
    assert(received == 0 && listener.subscription_count() == 0);

    expect_error<std::invalid_argument>([&] {
        listener.listen<GameStartedEvent>(
            [&](auto callback) {
                stale = std::move(callback);
                return EventDisconnect{};
            },
            [](GameStartedEvent&) {});
    });
    stale();
    assert(received == 0);

    Source<int> source;
    const bool bound = listener.listen<AddScoreEvent, int>(
        [&](auto callback) { return source.subscribe(std::move(callback)); },
        [](AddScoreEvent& event, int amount) {
            if (amount < 0) {
                throw std::invalid_argument("invalid source payload");
            }
            event.target = Players::P1;
            event.amount = amount;
        });
    assert(bound);
    expect_error<std::invalid_argument>([&] { source.callback(-1); });
    assert(received == 0);
    source.callback(4);
    assert(received == 1);
    listener.stop();
}

void retained_callbacks_do_not_access_destroyed_listener() {
    Source<> source;
    std::function<void()> retained;
    int received = 0;
    {
        ListenEvent listener([&](EventPtr) { ++received; });
        const bool bound = listener.listen<GameStartedEvent>(
            [&](auto callback) { return source.subscribe(std::move(callback)); },
            [](GameStartedEvent&) {});
        assert(bound);
        retained = source.callback;
        retained();
        assert(received == 1);
    }
    retained();
    assert(received == 1 && source.disconnected == 1);
}

void immediate_callback_can_stop_during_subscription() {
    int disconnected = 0;
    ListenEvent* current = nullptr;
    ListenEvent listener([&](EventPtr) { current->stop(); });
    current = &listener;
    const bool accepted = listener.listen<GameStartedEvent>(
        [&](auto callback) {
            callback();
            return EventDisconnect([&] { ++disconnected; });
        },
        [](GameStartedEvent&) {});
    assert(!accepted && disconnected == 1);
    assert(listener.subscription_count() == 0);
}

void all_unsubscribers_run_even_if_one_throws() {
    int disconnected = 0;
    ListenEvent listener([](EventPtr) {});
    const bool first = listener.listen<GameStartedEvent>(
        [&](auto) {
            return EventDisconnect([&] {
                ++disconnected;
                throw std::runtime_error("unsubscribe failed");
            });
        },
        [](GameStartedEvent&) {});
    const bool second = listener.listen<GameStartedEvent>(
        [&](auto) { return EventDisconnect([&] { ++disconnected; }); },
        [](GameStartedEvent&) {});
    assert(first && second);
    expect_error<std::runtime_error>([&] { listener.stop(); });
    assert(disconnected == 2 && !listener.listening());
    assert(listener.subscription_count() == 0);
    listener.stop();
}

void concurrent_sources_and_move_only_data() {
    std::atomic<int> received{0};
    Source<int> source;
    ListenEvent listener([&](EventPtr event) {
        assert(dynamic_cast<AddCoinsEvent*>(event.get()));
        received.fetch_add(1);
    });
    const bool bound = listener.listen<AddCoinsEvent, int>(
        [&](auto callback) { return source.subscribe(std::move(callback)); },
        [](AddCoinsEvent& event, int amount) {
            event.target = Players::P2;
            event.amount = amount;
        });
    assert(bound);
    const auto callback = source.callback;
    auto produce = [callback] {
        for (int i = 0; i < 100; ++i) callback(i);
    };
    std::thread first(produce);
    std::thread second(produce);
    first.join();
    second.join();
    assert(received.load() == 200);
    listener.stop();

    Source<EventPtr> batches;
    std::vector<EventPtr> output;
    ListenEvent batchListener([&](EventPtr event) {
        output.push_back(std::move(event));
    });
    const bool batchBound = batchListener.listen<EventBatch, EventPtr>(
        [&](auto handler) { return batches.subscribe(std::move(handler)); },
        [marker = std::make_unique<int>(3)](EventBatch& event, EventPtr child) {
            assert(*marker == 3);
            event.events.push_back(std::move(child));
        });
    assert(batchBound);
    batches.callback(std::make_unique<AddCoinsEvent>());
    assert(output.size() == 1);
    const auto* batch = dynamic_cast<const EventBatch*>(output[0].get());
    assert(batch && batch->events.size() == 1);
    batchListener.stop();
}

}  // namespace

int main() {
    factory_covers_all_events();
    callbacks_convert_input_and_stop();
    failed_registration_and_mapping_do_not_deliver();
    retained_callbacks_do_not_access_destroyed_listener();
    immediate_callback_can_stop_during_subscription();
    all_unsubscribers_run_even_if_one_throws();
    concurrent_sources_and_move_only_data();
    std::cout << "ListenEvent: 7 contract scenarios passed; " << TOTAL_EVENT_TYPES << " event types covered.\n";
}

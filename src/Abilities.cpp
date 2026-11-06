#include "Abilities.hpp"
#include <array>
namespace cardgame::abilities {
namespace {
template<class T> T add(T a, T b) {
    if ((b > 0 && a > std::numeric_limits<T>::max() - b) ||
        (b < 0 && a < std::numeric_limits<T>::min() - b))
        throw std::overflow_error("Resource overflow");
    return a + b;
}
void nonnegative(std::int64_t value) {
    if (value < 0) throw std::invalid_argument("Negative amount");
}
}
EventPtr set_score(Player& p, ScoreValue value) {
    if (p.score == value) return {};
    auto e = std::make_unique<ScoreChangedEvent>();
    e->player = p.id; e->before = p.score; e->after = value;
    p.score = value; return e;
}
EventPtr add_score(Player& p, ScoreValue amount) {
    nonnegative(amount); return set_score(p, add(p.score, amount));
}
EventPtr subtract_score(Player& p, ScoreValue amount) {
    nonnegative(amount);
    if (p.score < std::numeric_limits<ScoreValue>::min() + amount)
        throw std::overflow_error("Score overflow");
    return set_score(p, p.score - amount);
}
EventPtr set_coins(Player& p, CoinValue value, std::string reason) {
    nonnegative(value);
    if (p.coins == value) return {};
    auto e = std::make_unique<CoinsChangedEvent>();
    e->player = p.id; e->before = p.coins; e->after = value; e->reason = std::move(reason);
    p.coins = value; return e;
}
EventPtr gain_coins(Player& p, CoinValue amount, std::string reason) {
    nonnegative(p.coins); nonnegative(amount);
    return set_coins(p, add(p.coins, amount), std::move(reason));
}
EventPtr spend_coins(Player& p, CoinValue amount, std::string reason) {
    nonnegative(p.coins); nonnegative(amount);
    if (p.coins < amount) throw std::domain_error("Insufficient coins");
    return set_coins(p, p.coins - amount, std::move(reason));
}
bool add_pending_score(RewardData& r, ScoreValue delta) {
    const auto value = add(r.score, delta); const bool changed = value != r.score;
    r.score = value; return changed;
}
bool multiply_pending_score(RewardData& r, ScoreValue n, ScoreValue d, RoundingRule rule) {
    if (n < 0 || d <= 0) throw std::invalid_argument("Invalid score multiplier");
    if (n && (r.score > std::numeric_limits<ScoreValue>::max() / n ||
              r.score < std::numeric_limits<ScoreValue>::min() / n))
        throw std::overflow_error("Score product overflow");
    const auto product = r.score * n;
    auto q = product / d;
    const auto rem = product % d;
    if (rule == RoundingRule::Floor) { if (rem < 0) --q; }
    else if (rule == RoundingRule::Ceil) { if (rem > 0) ++q; }
    else if (rule == RoundingRule::Nearest) {
        const auto magnitude = rem < 0 ? -rem : rem;
        if (magnitude && magnitude >= d - magnitude) q += product < 0 ? -1 : 1;
    } else throw std::invalid_argument("Invalid rounding rule");
    const bool changed = r.score != q; r.score = q; return changed;
}
bool add_pending_coins(CoinValue& component, CoinValue delta) {
    const auto value = add(component, delta); nonnegative(value);
    const bool changed = component != value; component = value; return changed;
}
bool reduce_shop_cost(ShopData& q, CoinValue amount, CoinValue minimum) {
    nonnegative(q.finalCost); nonnegative(amount); nonnegative(minimum);
    const auto floor = std::min(minimum, q.finalCost);
    const auto value = std::max(floor, q.finalCost - std::min(amount, q.finalCost));
    const bool changed = value != q.finalCost; q.finalCost = value; return changed;
}
bool grant_free_refresh(std::size_t& credits, std::size_t count) {
    if (credits > std::numeric_limits<std::size_t>::max() - count)
        throw std::overflow_error("Refresh credit overflow");
    credits += count; return count != 0;
}
bool grant_function_slot(Player& p) {
    if (p.functionSlots >= MAX_FUNCTION_SLOTS) return false;
    ++p.functionSlots; return true;
}
EventPtr set_ability_counter(AbilityRef ability, CounterId counter, int& value, int replacement) {
    if (value == replacement) return {};
    auto e = std::make_unique<AbilityStateChangedEvent>();
    e->ability = ability; e->counterId = counter; e->before = value; e->after = replacement;
    value = replacement; return e;
}
EventPtr add_ability_counter(AbilityRef ability, CounterId counter, int& value, int delta) {
    return set_ability_counter(ability, counter, value, add(value, delta));
}
EventPtr set_ability_usable(AbilityRef ability, bool& usable, bool replacement) {
    if (usable == replacement) return {};
    auto e = std::make_unique<AbilityUsabilityChangedEvent>();
    e->ability = ability; e->usable = replacement; usable = replacement; return e;
}
EventPtr set_function_card_usable(FunctionCards& card, bool usable) {
    if (card.usable() == usable) return {};
    if (!card.reference()) throw std::invalid_argument("System card has no player reference");
    auto e = std::make_unique<FunctionCardUsabilityChangedEvent>();
    e->card = *card.reference(); e->usable = usable; card.set_usable(usable); return e;
}
EventPtr abolish_function_card(FunctionCards& card) {
    if (!card.available()) return {};
    if (!card.reference()) throw std::invalid_argument("System card has no player reference");
    auto e = std::make_unique<FunctionCardAbolishedEvent>();
    e->card = *card.reference(); card.abolish(); return e;
}
EventPtr equip_function_card(Register& r, Player& player, std::size_t slot, CardPtr card) {
    if (slot >= player.functionSlots || slot >= MAX_FUNCTION_SLOTS)
        throw std::out_of_range("Function-card slot is not unlocked");
    const auto p = player.id;
    auto e = std::make_unique<FunctionCardEquippedEvent>();
    if (!card || !card->reference()) throw std::invalid_argument("Invalid player card");
    e->card = *card->reference(); e->slot = slot; r.equip(p, slot, std::move(card)); return e;
}
EventPtr remove_function_card(Register& r, Players p, std::size_t slot, RemovalReason reason) {
    const auto card = r.at(p, slot);
    if (!card) return {};
    auto e = std::make_unique<FunctionCardRemovedEvent>();
    e->card = *card->reference(); e->reason = reason; r.unequip(p, slot); return e;
}
namespace detail {
void validate_players(const PlayerOrder& players) {
    if (players.empty()) throw std::invalid_argument("Empty player list");
    std::array<bool, MAX_PLAYERS> seen{};
    for (auto player : players) {
        const auto p = static_cast<std::size_t>(player);
        if (p >= MAX_PLAYERS || seen[p]) throw std::invalid_argument("Invalid player list");
        seen[p] = true;
    }
}
}
EventPtr deal_initial_cards(RoundId round, const PlayerOrder& players, int count,
                           const InitialDealOperation& deal) {
    detail::validate_players(players);
    if (!deal || count <= 0) throw std::invalid_argument("Invalid initial deal");
    auto e = std::make_unique<InitialCardsDealtEvent>(); e->roundId = round;
    if (!deal(round, players, count)) return {};
    return e;
}
} // namespace cardgame::abilities

#include "Register.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace cardgame {
Register::Register() : entries_(PLAYER_CARD_ROWS), matrix_(PLAYER_CARD_ROWS) {}
std::size_t Register::row_of(Players player, std::size_t slot) {
    const auto p = static_cast<std::size_t>(player);
    if (p >= MAX_PLAYERS || slot >= MAX_FUNCTION_SLOTS)
        throw std::out_of_range("Invalid player card slot");
    return p * MAX_FUNCTION_SLOTS + slot;
}
std::uint64_t Register::next_token() {
    if (sequence_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Registration token exhausted");
    return ++sequence_;
}
void Register::validate_card(Players player, const CardPtr& card,
                            std::optional<std::size_t> replacing) {
    if (!card || !card->reference() || card->reference()->owner != player)
        throw std::invalid_argument("Card owner does not match slot");
    const auto key = std::make_pair(player, card->reference()->seq);
    auto known = identities_.find(key);
    if (known != identities_.end() && known->second.lock().get() != card.get())
        throw std::invalid_argument("Card identity cannot be reused");
    for (std::size_t i = 0; i < entries_.size(); ++i)
        if ((!replacing || i != *replacing) && entries_[i].card == card)
            throw std::invalid_argument("Card is already registered");
    identities_.emplace(key, card);
}
void Register::equip(Players player, std::size_t slot, CardPtr card) {
    const auto row = row_of(player, slot);
    if (entries_[row].card) throw std::logic_error("Slot occupied");
    validate_card(player, card);
    const auto token = next_token();
    entries_[row] = {row, token, std::move(card)};
    matrix_[row] = entries_[row].card->subscriptions();
}
CardPtr Register::replace(Players player, std::size_t slot, CardPtr card) {
    const auto row = row_of(player, slot);
    if (entries_[row].card == card && card) return card;
    validate_card(player, card, row);
    const auto token = next_token();
    auto old = entries_[row].card;
    entries_[row] = {row, token, std::move(card)};
    matrix_[row] = entries_[row].card->subscriptions();
    return old;
}
CardPtr Register::unequip(Players player, std::size_t slot) {
    const auto row = row_of(player, slot);
    auto old = std::move(entries_[row].card);
    entries_[row] = {};
    matrix_[row].reset();
    return old;
}
void Register::move(Players player, std::size_t from, std::size_t to) {
    const auto source = row_of(player, from), target = row_of(player, to);
    if (source == target) return;
    if (!entries_[source].card || entries_[target].card)
        throw std::logic_error("Invalid card move");
    entries_[target] = std::move(entries_[source]);
    entries_[target].row = target;
    entries_[source] = {};
    matrix_[target] = matrix_[source]; matrix_[source].reset();
}
std::size_t Register::add_system(CardPtr card) {
    if (!card || !card->system()) throw std::invalid_argument("Expected a system card");
    for (const auto& entry : entries_)
        if (entry.card && entry.card->system() &&
            entry.card->definition() == card->definition())
            throw std::invalid_argument("System card already registered");
    const auto token = next_token();
    std::size_t row = PLAYER_CARD_ROWS;
    while (row < entries_.size() && entries_[row].card) ++row;
    if (row == entries_.size()) {
        // 先扩展两个容器，失败时不留下半行。
        entries_.push_back({});
        try { matrix_.push_back({}); }
        catch (...) { entries_.pop_back(); throw; }
    }
    entries_[row] = {row, token, std::move(card)};
    matrix_[row] = entries_[row].card->subscriptions();
    return row;
}
CardPtr Register::remove_system(std::size_t row) {
    if (row < PLAYER_CARD_ROWS || row >= entries_.size())
        throw std::out_of_range("Invalid system row");
    auto old = std::move(entries_[row].card);
    entries_[row] = {}; matrix_[row].reset();
    return old; // 保留空行，其他系统牌行号不变；后续注册可复用。
}
CardPtr Register::at(Players player, std::size_t slot) const {
    return entries_[row_of(player, slot)].card;
}
std::vector<CardPtr> Register::player_cards(Players player) const {
    std::vector<CardPtr> cards;
    for (std::size_t slot = 0; slot < MAX_FUNCTION_SLOTS; ++slot)
        if (auto card = at(player, slot)) cards.push_back(std::move(card));
    return cards;
}
bool Register::current(const RegisteredCard& candidate) const noexcept {
    for (const auto& entry : entries_)
        if (entry.token == candidate.token && entry.card == candidate.card &&
            entry.card && entry.card->available() && entry.card->usable()) return true;
    return false;
}
std::vector<RegisteredCard> Register::matching(EventType event, const PlayerOrder& order) const {
    const auto column = static_cast<std::size_t>(event);
    if (column >= TOTAL_EVENT_TYPES) throw std::invalid_argument("Invalid event column");
    std::array<bool, MAX_PLAYERS> seen{};
    for (auto player : order) {
        const auto p = static_cast<std::size_t>(player);
        if (p >= MAX_PLAYERS || seen[p]) throw std::invalid_argument("Invalid player order");
        seen[p] = true;
    }
    std::vector<RegisteredCard> result;
    const auto append = [&](std::size_t row) {
        const auto& e = entries_[row];
        if (matrix_[row].test(column) && e.card && e.card->available() && e.card->usable())
            result.push_back(e);
    };
    for (std::size_t row = PLAYER_CARD_ROWS; row < entries_.size(); ++row)
        if (entries_[row].card &&
            entries_[row].card->system_order() == SystemOrder::BeforePlayers) append(row);
    for (auto player : order)
        for (std::size_t slot = 0; slot < MAX_FUNCTION_SLOTS; ++slot)
            append(row_of(player, slot));
    for (std::size_t row = PLAYER_CARD_ROWS; row < entries_.size(); ++row)
        if (entries_[row].card &&
            entries_[row].card->system_order() == SystemOrder::AfterPlayers) append(row);
    return result;
}
} // namespace cardgame

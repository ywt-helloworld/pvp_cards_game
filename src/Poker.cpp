#include "Poker.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <set>
#include <stdexcept>
namespace cardgame {
std::vector<Poker> standard_deck() {
    std::vector<Poker> deck;
    for (int rank = 3; rank <= 15; ++rank)
        for (unsigned suit = 0; suit < 4; ++suit) {
            Poker card;
            card.id = deck.size() + 1; card.rank = rank == 15 ? 2 : rank;
            card.suit = static_cast<Suit>(suit); deck.push_back(card);
        }
    for (int i = 0; i < 2; ++i) {
        Poker card; card.id = deck.size() + 1; card.isJoker = true; deck.push_back(card);
    }
    return deck;
}
namespace {
int power(int rank) { return rank == 2 ? 15 : rank == 1 ? 14 : rank; }
bool straight(HandPattern p) {
    return p == HandPattern::Straight || p == HandPattern::StraightFlush;
}
bool beats(const MoveOption& move, const MoveOption& old) {
    if (move.pattern == HandPattern::Bomb) {
        if (old.pattern != HandPattern::Bomb) return true;
        return move.count > old.count || (move.count == old.count && move.rank > old.rank);
    }
    if (old.pattern == HandPattern::Bomb || move.count != old.count) return false;
    if (straight(move.pattern) && straight(old.pattern)) return move.rank == old.rank + 1;
    if (move.pattern != old.pattern || old.rank == 15) return false;
    return move.rank == 15 || move.rank == old.rank + 1;
}
}
std::vector<MoveOption> legal_moves(const std::vector<Poker>& cards,
    const std::optional<MoveOption>& previous, const std::vector<WildcardAssignment>* specified) {
    if (cards.empty()) return {};
    std::set<CardId> identities;
    std::vector<int> ranks(cards.size());
    std::vector<Suit> suits(cards.size());
    std::vector<std::size_t> jokers;
    for (std::size_t i = 0; i < cards.size(); ++i) {
        if (!identities.insert(cards[i].id).second) return {};
        if (cards[i].isJoker) jokers.push_back(i);
        else {
            if (!cards[i].rank || !cards[i].suit) return {};
            ranks[i] = *cards[i].rank; suits[i] = *cards[i].suit;
            if (ranks[i] < 2 || ranks[i] > 14 ||
                static_cast<unsigned>(suits[i]) >= 4) return {};
        }
    }
    if (jokers.size() > 2) throw std::invalid_argument("Too many jokers");
    std::vector<WildcardAssignment> assigned;
    if (specified) {
        if (specified->size() != jokers.size()) return {};
        std::set<CardId> used;
        for (const auto& value : *specified) {
            auto it = std::find_if(jokers.begin(), jokers.end(),
                [&](std::size_t i) { return cards[i].id == value.cardId; });
            if (it == jokers.end() || !used.insert(value.cardId).second ||
                value.rank < 1 || value.rank > 14 || static_cast<unsigned>(value.suit) >= 4)
                return {};
            const auto i = *it;
            ranks[i] = value.rank == 1 ? 14 : value.rank; suits[i] = value.suit;
            assigned.push_back(value);
        }
    }
    std::vector<MoveOption> result;
    const auto evaluate = [&] {
        const auto append = [&](HandPattern pattern, int rank) {
            MoveOption m{pattern, rank, cards.size(), assigned};
            if (!previous || beats(m, *previous)) result.push_back(std::move(m));
        };
        if (cards.size() == 1) { append(HandPattern::Single, power(ranks[0])); return; }
        const bool same = std::all_of(ranks.begin(), ranks.end(),
            [&](int r) { return power(r) == power(ranks[0]); });
        if (same) {
            if (cards.size() == 2) append(HandPattern::Pair, power(ranks[0]));
            else append(HandPattern::Bomb, power(ranks[0]));
            return;
        }
        if (cards.size() < 3 || cards.size() > 13) return;
        const bool suited = std::all_of(suits.begin(), suits.end(),
            [&](Suit s) { return s == suits[0]; });
        std::set<int> starts;
        for (bool lowAce : {false, true}) {
            auto values = ranks;
            if (lowAce) for (auto& rank : values) if (rank == 14) rank = 1;
            std::sort(values.begin(), values.end());
            bool ok = true;
            for (std::size_t i = 1; i < values.size(); ++i)
                if (values[i] != values[i - 1] + 1) ok = false;
            if (ok && starts.insert(values.front()).second)
                append(suited ? HandPattern::StraightFlush : HandPattern::Straight, values.front());
        }
    };
    if (specified || jokers.empty()) { evaluate(); return result; }
    std::function<void(std::size_t)> expand = [&](std::size_t j) {
        if (j == jokers.size()) { evaluate(); return; }
        const auto i = jokers[j];
        for (int rank = 2; rank <= 14; ++rank)
            for (unsigned suit = 0; suit < 4; ++suit) {
                ranks[i] = rank; suits[i] = static_cast<Suit>(suit);
                assigned.push_back({cards[i].id, rank, suits[i]});
                expand(j + 1); assigned.pop_back();
            }
    };
    expand(0);
    return result;
}
} // namespace cardgame

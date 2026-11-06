#pragma once
#include "Event.hpp"
#include <optional>
namespace cardgame {
/// 同一副公共牌组中的稳定实例。A 使用 14，2 使用 2，王没有固定点数与花色。
struct Poker {
    CardId id = 0;
    bool isJoker = false;
    std::optional<Suit> suit;
    std::optional<int> rank;
    int multiplier = 1;
    int baseValue = 0;
};
struct MoveOption {
    HandPattern pattern = HandPattern::Single;
    int rank = 0; // 单张/对子/炸弹为强度 3..15；顺子为起点 1..12。
    std::size_t count = 0;
    std::vector<WildcardAssignment> wildcards;
};
/// 只计算合法方案，不修改状态或推进随机数。
std::vector<MoveOption> legal_moves(const std::vector<Poker>& cards,
    const std::optional<MoveOption>& previous = std::nullopt,
    const std::vector<WildcardAssignment>* specified = nullptr);
std::vector<Poker> standard_deck();
} // namespace cardgame

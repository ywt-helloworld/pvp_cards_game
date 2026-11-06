#pragma once
#include "FunctionCards.hpp"
#include <array>
#include <map>
#include <utility>
namespace cardgame {
struct RegisteredCard {
    std::size_t row = 0;
    std::uint64_t token = 0; // 装备世代；移槽保持，卸下重装则改变。
    CardPtr card;            // 冻结候选时保持对象存活。
};
/// 前 24 行是 P1～P4 各 6 个槽位，之后是系统牌；每行都有 132 列。
class Register {
public:
    Register();
    void equip(Players player, std::size_t slot, CardPtr card);
    CardPtr replace(Players player, std::size_t slot, CardPtr card);
    CardPtr unequip(Players player, std::size_t slot);
    void move(Players player, std::size_t from, std::size_t to);
    std::size_t add_system(CardPtr card);
    CardPtr remove_system(std::size_t row);
    CardPtr at(Players player, std::size_t slot) const;
    std::vector<CardPtr> player_cards(Players player) const;
    const TriggerMatrix& matrix() const noexcept { return matrix_; }
    std::vector<RegisteredCard> matching(EventType event, const PlayerOrder& order) const;
    bool current(const RegisteredCard& candidate) const noexcept;
    static std::size_t row_of(Players player, std::size_t slot);
private:
    void validate_card(Players player, const CardPtr& card,
                       std::optional<std::size_t> replacing = std::nullopt);
    std::uint64_t next_token();
    std::vector<RegisteredCard> entries_;
    TriggerMatrix matrix_;
    std::map<std::pair<Players, std::size_t>, std::weak_ptr<FunctionCards>> identities_;
    std::uint64_t sequence_ = 0;
};
} // namespace cardgame

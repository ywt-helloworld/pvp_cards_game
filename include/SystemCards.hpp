#pragma once
#include "Abilities.hpp"
#include "HandleEvent.hpp"
#include <array>
namespace cardgame {
/// 不可见的资源操作牌：将资源请求转换为已提交的资源变化通知。
class ResourceSystemCard final : public FunctionCards {
public:
    explicit ResourceSystemCard(std::array<Player, 4>& players);
    void resolve(const Context& context, CardEffects& effects) override;
private:
    std::array<Player, 4>& players_;
};
/// 不可见的整批发牌牌；业务回调负责共享牌堆和全体手牌的原子更新。
class InitialDealSystemCard final : public FunctionCards {
public:
    InitialDealSystemCard(PlayerOrder players, InitialDealOperation operation);
    void resolve(const Context& context, CardEffects& effects) override;
private:
    PlayerOrder players_;
    InitialDealOperation operation_;
};
/// 演示玩家牌：得分与金币调用两个独立原子操作，不合成一个技能。
class ResourceBonusCard final : public FunctionCards {
public:
    ResourceBonusCard(Player& owner, std::size_t sequence, DefinitionId definition,
                      std::string name, EventType trigger, ScoreValue score, CoinValue coins);
    void resolve(const Context& context, CardEffects& effects) override;
    std::string description() const override;
    ScoreValue score_bonus() const noexcept { return score_; }
    CoinValue coin_bonus() const noexcept { return coins_; }
protected:
    void validate_bonus_upgrade(ScoreValue score, CoinValue coins) const override;
    void apply_bonus_upgrade(ScoreValue score, CoinValue coins) noexcept override;
private:
    EventType trigger_;
    Player& owner_;
    ScoreValue score_;
    CoinValue coins_;
};
} // namespace cardgame

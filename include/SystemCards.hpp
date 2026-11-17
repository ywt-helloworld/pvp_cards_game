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
class SpecialtyCard final : public FunctionCards {
public:
    SpecialtyCard(CardRef ref, DefinitionId id, std::string name, std::string text)
        : FunctionCards(ref,id,std::move(name),{}), text_(std::move(text)) {}
    void resolve(const Context&,CardEffects&) override {}
    std::string description() const override {
        return text_ + (extra_ ? " 强化额外增加 " + std::to_string(extra_) + " 糖度。" : "");
    }
    ScoreValue bonus() const noexcept { return extra_; }
protected:
    void validate_bonus_upgrade(ScoreValue score,CoinValue coins) const override {
        const auto id=definition();
        if (coins || (score && id!=2001 && id!=3016 && id!=3017 && id!=3018))
            throw std::invalid_argument("Unsupported numerical enhancement");
        if (score>std::numeric_limits<ScoreValue>::max()-5-extra_)
            throw std::overflow_error("Bonus overflow");
    }
    void apply_bonus_upgrade(ScoreValue score,CoinValue) noexcept override { extra_+=score; }
private:
    std::string text_;
    ScoreValue extra_=0;
};

class ResourceBonusCard final : public FunctionCards {
public:
    ResourceBonusCard(Player& owner, std::size_t sequence, DefinitionId definition,
                      std::string name, EventType trigger, ScoreValue score, CoinValue coins, unsigned limit = 0,
                      std::optional<HandPattern> pattern = {}, bool anyPlayer = false);
    void resolve(const Context& context, CardEffects& effects) override;
    std::string description() const override;
    ScoreValue score_bonus() const noexcept { return score_; }
    CoinValue coin_bonus() const noexcept { return coins_; }
protected:
    void validate_bonus_upgrade(ScoreValue score, CoinValue coins) const override;
    void apply_bonus_upgrade(ScoreValue score, CoinValue coins) noexcept override;
private:
    unsigned limit_, remaining_;
    std::optional<HandPattern> pattern_;
    bool anyPlayer_;
    EventType trigger_;
    Player& owner_;
    ScoreValue score_;
    CoinValue coins_;
};
} // namespace cardgame

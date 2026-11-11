#pragma once
#include "Context.hpp"
#include <initializer_list>
#include <string>
#include <utility>
namespace cardgame {
struct CardEffects;
/// 附加技能仍由若干原子操作组成；只订阅自身需要的事件。
struct AddedCardSkill {
    EventMask subscriptions;
    CardHandler handler;
    std::string description;
    bool previewSafe = false;
};
/// 强化参数。数值的具体含义由目标卡实现解释；技能可引入新订阅。
struct FunctionCardEnhancement {
    ScoreValue scoreDelta = 0;
    CoinValue coinsDelta = 0;
    std::vector<AddedCardSkill> skills;
};
/// 功能牌是一组原子技能的组合。玩家牌和不可见系统牌共用此接口。
class FunctionCards {
public:
    FunctionCards(CardRef reference, DefinitionId definition, std::string name,
                  EventMask subscriptions, bool previewSafe = false);
    FunctionCards(DefinitionId definition, std::string name, EventMask subscriptions,
                  SystemOrder order = SystemOrder::AfterPlayers, bool previewSafe = false);
    virtual ~FunctionCards() = default;
    virtual void resolve(const Context& context, CardEffects& effects) = 0;
    void resolve_all(const Context& context, CardEffects& effects);
    virtual std::string description() const { return {}; }
    std::string full_description() const;
    unsigned enhancement_level() const noexcept { return enhancementLevel_; }
    void validate_enhancement(const FunctionCardEnhancement& enhancement) const;
    const std::optional<CardRef>& reference() const noexcept { return reference_; }
    DefinitionId definition() const noexcept { return definition_; }
    const std::string& name() const noexcept { return name_; }
    const EventMask& subscriptions() const noexcept { return subscriptions_; }
    bool system() const noexcept { return !reference_; }
    bool visible_to_players() const noexcept { return !system(); }
    SystemOrder system_order() const noexcept { return order_; }
    bool available() const noexcept { return available_; }
    bool usable() const noexcept { return usable_; }
    bool preview_safe() const noexcept { return previewSafe_; }
    bool set_usable(bool usable) noexcept {
        const bool changed = usable_ != usable; usable_ = usable; return changed;
    }
    bool abolish() noexcept {
        const bool changed = available_; available_ = false; return changed;
    }
    static EventMask listens_to(std::initializer_list<EventType> events);
protected:
    virtual void validate_bonus_upgrade(ScoreValue score, CoinValue coins) const;
    virtual void apply_bonus_upgrade(ScoreValue score, CoinValue coins) noexcept;
private:
    friend class Register;
    void enhance(const FunctionCardEnhancement& enhancement);
    std::optional<CardRef> reference_;
    DefinitionId definition_;
    std::string name_;
    const EventMask baseSubscriptions_;
    EventMask subscriptions_; // 强化附加技能后由 Register 同步到订阅矩阵。
    std::vector<std::shared_ptr<AddedCardSkill>> addedSkills_;
    unsigned enhancementLevel_ = 0;
    SystemOrder order_ = SystemOrder::AfterPlayers;
    bool previewSafe_ = false;
    bool available_ = true;
    bool usable_ = true;
};
/// 可通过一个卡牌级回调组合任意签名的原子技能，无需新增调度分支。
class CallbackFunctionCard final : public FunctionCards {
public:
    CallbackFunctionCard(CardRef reference, DefinitionId definition, std::string name,
                         EventMask mask, CardHandler handler, bool previewSafe = false);
    CallbackFunctionCard(DefinitionId definition, std::string name, EventMask mask,
                         CardHandler handler, SystemOrder order = SystemOrder::AfterPlayers,
                         bool previewSafe = false);
    void resolve(const Context& context, CardEffects& effects) override;
private:
    CardHandler handler_;
};
} // namespace cardgame

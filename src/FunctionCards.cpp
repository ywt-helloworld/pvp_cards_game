#include "FunctionCards.hpp"
#include <stdexcept>
#include <limits>
namespace cardgame {
namespace {
void validate(DefinitionId definition, const std::string& name) {
    if (!definition || name.empty()) throw std::invalid_argument("Invalid card metadata");
}
}
FunctionCards::FunctionCards(CardRef reference, DefinitionId definition, std::string name,
                             EventMask mask, bool previewSafe)
    : reference_(reference), definition_(definition), name_(std::move(name)),
      baseSubscriptions_(mask), subscriptions_(mask), previewSafe_(previewSafe) {
    validate(definition_, name_);
    if (static_cast<std::size_t>(reference.owner) >= MAX_PLAYERS || !reference.seq)
        throw std::invalid_argument("Invalid card reference");
}
FunctionCards::FunctionCards(DefinitionId definition, std::string name, EventMask mask,
                             SystemOrder order, bool previewSafe)
    : definition_(definition), name_(std::move(name)), baseSubscriptions_(mask), subscriptions_(mask),
      order_(order), previewSafe_(previewSafe) {
    validate(definition_, name_);
    if (order != SystemOrder::BeforePlayers && order != SystemOrder::AfterPlayers)
        throw std::invalid_argument("Invalid system card order");
}
EventMask FunctionCards::listens_to(std::initializer_list<EventType> events) {
    EventMask mask;
    for (auto event : events) {
        const auto index = static_cast<std::size_t>(event);
        if (index >= TOTAL_EVENT_TYPES) throw std::invalid_argument("Invalid trigger");
        mask.set(index);
    }
    return mask;
}
void FunctionCards::resolve_all(const Context& c, CardEffects& out) {
    const auto column=static_cast<std::size_t>(*c.type());
    const auto skills=addedSkills_; // 冻结技能列表；技能中强化自身不会破坏迭代或回溯触发。
    if(baseSubscriptions_.test(column)) resolve(c,out);
    // 原有技能和附加技能共享同一 CardEffects，整张牌完成后才处理后续事件。
    for(const auto& skill:skills)
        if(skill->subscriptions.test(column) &&
           (c.metadata()->mode!=ExecutionMode::Preview || skill->previewSafe)) skill->handler(c,out);
}
std::string FunctionCards::full_description() const {
    auto text=description();
    for(const auto& skill:addedSkills_) { if(!text.empty())text+="; ";text+=skill->description; }
    return text;
}
void FunctionCards::validate_bonus_upgrade(ScoreValue score,CoinValue coins) const {
    if(score||coins) throw std::invalid_argument("Card does not support numeric enhancement");
}
void FunctionCards::apply_bonus_upgrade(ScoreValue,CoinValue) noexcept {}
void FunctionCards::validate_enhancement(const FunctionCardEnhancement& e) const {
    if(system()||!available()) throw std::invalid_argument("Card cannot be enhanced");
    if(e.scoreDelta<0||e.coinsDelta<0||(!e.scoreDelta&&!e.coinsDelta&&e.skills.empty()))
        throw std::invalid_argument("Empty or negative enhancement");
    if(enhancementLevel_==std::numeric_limits<unsigned>::max()) throw std::overflow_error("Enhancement level overflow");
    validate_bonus_upgrade(e.scoreDelta,e.coinsDelta);
    for(const auto& skill:e.skills)
        if(skill.subscriptions.none()||!skill.handler||skill.description.empty())
            throw std::invalid_argument("Invalid added skill");
}
void FunctionCards::enhance(const FunctionCardEnhancement& e) {
    validate_enhancement(e);
    auto skills=addedSkills_; // 所有校验和分配完成后，才提交数值与新技能。
    for(const auto& skill:e.skills)skills.push_back(std::make_shared<AddedCardSkill>(skill));
    auto mask=subscriptions_;
    for(const auto& skill:e.skills)mask|=skill.subscriptions;
    apply_bonus_upgrade(e.scoreDelta,e.coinsDelta);
    addedSkills_.swap(skills);subscriptions_=mask;++enhancementLevel_;
}
CallbackFunctionCard::CallbackFunctionCard(CardRef reference, DefinitionId definition,
    std::string name, EventMask mask, CardHandler handler, bool previewSafe)
    : FunctionCards(reference, definition, std::move(name), mask, previewSafe),
      handler_(std::move(handler)) {
    if (!handler_) throw std::invalid_argument("Empty card handler");
}
CallbackFunctionCard::CallbackFunctionCard(DefinitionId definition, std::string name,
    EventMask mask, CardHandler handler, SystemOrder order, bool previewSafe)
    : FunctionCards(definition, std::move(name), mask, order, previewSafe),
      handler_(std::move(handler)) {
    if (!handler_) throw std::invalid_argument("Empty card handler");
}
void CallbackFunctionCard::resolve(const Context& context, CardEffects& effects) {
    handler_(context, effects);
}
} // namespace cardgame

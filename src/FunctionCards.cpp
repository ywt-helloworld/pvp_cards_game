#include "FunctionCards.hpp"
#include <stdexcept>
namespace cardgame {
namespace {
void validate(DefinitionId definition, const std::string& name) {
    if (!definition || name.empty()) throw std::invalid_argument("Invalid card metadata");
}
}
FunctionCards::FunctionCards(CardRef reference, DefinitionId definition, std::string name,
                             EventMask mask, bool previewSafe)
    : reference_(reference), definition_(definition), name_(std::move(name)),
      subscriptions_(mask), previewSafe_(previewSafe) {
    validate(definition_, name_);
    if (static_cast<std::size_t>(reference.owner) >= MAX_PLAYERS || !reference.seq)
        throw std::invalid_argument("Invalid card reference");
}
FunctionCards::FunctionCards(DefinitionId definition, std::string name, EventMask mask,
                             SystemOrder order, bool previewSafe)
    : definition_(definition), name_(std::move(name)), subscriptions_(mask),
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

#pragma once
#include "FunctionCards.hpp"
#include <variant>
namespace cardgame {
/// 每项效果是一个不可再拆分的状态操作。新消耗牌可复用效果，调整数值。
struct ExpandShopSlots { std::size_t amount = 1; };
struct UpgradeShop { unsigned amount = 1; };
struct EnhanceFunctionCard { FunctionCardEnhancement enhancement; };
using ConsumableEffect = std::variant<ExpandShopSlots, UpgradeShop, EnhanceFunctionCard>;
/// 消耗牌只是一份即时商品定义，不继承持续功能牌，也不进入 Register。
struct ConsumableCard {
    DefinitionId id = 0;
    std::string name;
    CoinValue price = 0;
    std::string description;
    ConsumableEffect effect;
    unsigned minimumShopLevel = 1;
    bool needs_target() const noexcept { return std::holds_alternative<EnhanceFunctionCard>(effect); }
};
} // namespace cardgame

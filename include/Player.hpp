#pragma once
#include "Common/enums.hpp"
#include "Common/types.hpp"
#include <array>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace cardgame {
/// 服务器拥有的玩家数据；功能牌槽位和订阅由 Register 统一维护。
class Player {
public:
    Player(Players seat, std::string playerName)
        : id(seat), name(std::move(playerName)) {
        if (static_cast<std::size_t>(id) >= 4 || name.empty())
            throw std::invalid_argument("Invalid player identity");
    }
    const Players id;              // 稳定座位；客户端不能更改。
    std::string name;
    CoinValue coins = 6;
    ScoreValue score = 0;
    std::vector<CardId> hand;       // 引用公共牌组中的实例，不复制扑克牌。
    bool connected = true;
    std::uint64_t connectionGeneration = 0;
    std::size_t functionSlots = 4;  // 最大 6，购买设施后增加。
    std::size_t extraShopSlots = 0; // 消耗牌增加的普通货位，跨局保留。
    unsigned shopLevel = 1;        // 商店等级，影响可生成商品池。
    std::array<unsigned, 5> upgrades{}; // 各牌型已付费加强次数。
    std::set<DefinitionId> facilities;
    ScoreValue roundPlayScore = 0;
    CoinValue roundPlayCoins = 0;
};
} // namespace cardgame

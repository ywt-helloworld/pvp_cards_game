#pragma once
#include "Poker.hpp"
#include "SystemCards.hpp"
#include "ConsumableCards.hpp"
#include <chrono>
#include <map>
#include <random>
namespace cardgame {
struct ShopOffer {
    ProductId id = 0;
    DefinitionId definition = 0;
    CoinValue price = 0;
    std::string name;
    ShopProductKind kind = ShopProductKind::FunctionCard;
    std::string description;
    bool needsTarget = false;
};
struct ShopState {
    ShopId id = 0;
    std::vector<ShopOffer> offers;
    std::optional<std::size_t> locked;
    bool done = false;
    std::size_t refreshes = 0, paidRefreshes = 0, freeRefreshes = 0;
    bool upgradeDiscountUsed = false, refreshDiscountUsed = false;
    DefinitionId facility = 0;
};
struct PendingPlayChoice {
    ChoiceId id = 0;
    Players player = Players::P1;
    TurnId turn = 0;
    std::vector<CardId> cards;
    std::vector<MoveOption> options;
};
struct SchedulerConfig {
    struct CardDefinition {
        DefinitionId id = 0;
        std::string name;
        CoinValue price = 0;
        FunctionCardFactory create;
        unsigned minimumShopLevel = 1;
    };
    std::vector<CardDefinition> catalog; // 为空时使用明确标识为 Demo 的测试卡池。
    std::vector<ConsumableCard> consumables; // 为空时使用 Demo 消耗牌。
    double consumableChance = 0.3; // 前两个货位保留功能牌，其余为混合货位。
    std::uint32_t seed = std::random_device{}();
    std::size_t rounds = 8;
    std::chrono::seconds shopTime{60}, turnTime{30}, choiceTime{30};
};
/// 四人服务器调度器：玩家牌和系统牌使用同一注册表和结算路径。
/// 客户端仅上传请求；submit 的 actor 必须由会话层绑定。
class Scheduler {
public:
    explicit Scheduler(std::array<Player, 4> players, SchedulerConfig config = {});
    DispatchReport submit(EventPtr request, RequestId requestId = 0);
    void tick(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void timeout(); // 服务器定时器入口；不是机器人决策。
    void disconnected(Players player);
    void reconnected(Players player);
    GamePhase phase() const noexcept { return phase_; }
    RoundId round() const noexcept { return round_; }
    TurnId turn() const noexcept { return turn_; }
    Players acting_player() const noexcept { return acting_; }
    const Player& player(Players player) const;
    Player& player(Players player);
    const ShopState& shop(Players player) const;
    const std::optional<PendingPlayChoice>& choice() const noexcept { return choice_; }
    const std::vector<MoveOption>& preview(Players player) const;
    const Poker& poker(CardId card) const;
    std::size_t deck_remaining() const noexcept { return drawPile_.size(); }
    const std::vector<CardId>& discard() const noexcept { return discarded_; }
    const std::vector<CardId>& table_cards() const noexcept { return tableCards_; }
    const std::optional<MoveOption>& table_move() const noexcept { return table_; }
    const std::vector<Players>& winners() const noexcept { return winners_; }
    std::uint64_t version() const noexcept { return version_; }
    /// 当前服务器截止时间的剩余毫秒；客户端只展示，不决定是否超时。
    std::int64_t remaining_ms() const noexcept {
        if (phase_ != GamePhase::Shopping && phase_ != GamePhase::Playing) return 0;
        const auto value = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline_ - std::chrono::steady_clock::now()).count();
        return value > 0 ? value : 0;
    }
    Register& registry() noexcept { return registry_; }
    const Register& registry() const noexcept { return registry_; }
    const Condition& condition() const noexcept { return condition_; }
    std::size_t next_card_sequence(Players player);
    std::vector<CardRef> consumable_targets(Players player, DefinitionId definition) const;
private:
    DispatchReport dispatch(EventPtr event, DispatchMeta meta = {});
    void process(const Context& context, CardEffects& effects);
    void open_shops(CardEffects& effects);
    void fill_shop(Players player, bool refreshing, std::size_t firstSlot = 0);
    void fill_offers(Players player, ShopState& shop, bool refreshing, std::size_t firstSlot,
                     std::mt19937& random, std::uint64_t& sequence);
    std::size_t shop_slots(const Player& player) const;
    const ConsumableCard& consumable(DefinitionId definition) const;
    void validate_consumable(Players player, const ConsumableCard& card,
                             std::optional<CardRef> target) const;
    void use_consumable(Players player, const ConsumableCard& card,
                        std::optional<CardRef> target, CardEffects& out);
    void start_round(CardEffects& effects);
    bool initial_deal(RoundId round, const PlayerOrder& order, int count);
    void next_turn(Players player, CardEffects& effects);
    void play(Players player, const std::vector<CardId>& cards,
              const MoveOption& option, CardEffects& effects);
    std::vector<Poker> selected_cards(Players player, const std::vector<CardId>& cards) const;
    void require_shop(Players player, ShopId shop) const;
    void require_turn(Players player, TurnId turn) const;
    CoinValue upgrade_cost(Players player, HandPattern pattern) const;
    CoinValue sale_value(Players player, CardRef card) const;
    std::size_t card_slot(CardRef card) const;
    CardPtr make_card(Player& player, DefinitionId definition);
    PlayerOrder starting_at(Players player) const;
    static std::size_t index(Players player);
    std::array<Player, 4> players_;
    SchedulerConfig config_;
    Register registry_;
    Condition condition_;
    HandleEvent handler_;
    std::mt19937 random_;
    GamePhase phase_ = GamePhase::NotStarted;
    std::array<ShopState, 4> shops_;
    std::array<std::map<std::size_t, CoinValue>, 4> paidCosts_;
    std::array<std::size_t, 4> cardSequences_{};
    std::array<std::vector<MoveOption>, 4> previews_;
    std::array<std::optional<ContinueMode>, 4> continueVotes_;
    std::array<bool, 4> newGameVotes_{};
    std::vector<Poker> cards_;
    std::vector<CardId> drawPile_, discarded_, tableCards_;
    std::optional<MoveOption> table_;
    std::optional<PendingPlayChoice> choice_;
    std::array<std::size_t, 4> lockedHands_{};
    std::array<bool, 4> roundPaid_{};
    std::vector<Players> winners_;
    RoundId round_ = 1;
    TurnId turn_ = 0;
    TrickId trick_ = 0;
    Players first_ = Players::P1, acting_ = Players::P1, lastPlayed_ = Players::P1;
    bool awaiting_ = false, roundLocked_ = false;
    std::array<bool, 4> drawAcknowledged_{};
    PlayerOrder drawOrder_;
    std::uint64_t sequence_ = 0, version_ = 0;
    std::chrono::steady_clock::time_point now_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point deadline_ = now_;
};
} // namespace cardgame

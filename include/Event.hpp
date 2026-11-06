#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// 玩家编号；P1、P2、P3、P4 分别对应房间内的四个玩家。
enum class Players : std::uint8_t { P1, P2, P3, P4 };

// 全部 132 种具体事件的类型编号；原有 0～62 保持不变。
enum class EventType : std::size_t {
    GameStart = 0,
    ShopEnter = 1,
    ShopActionWindow = 2,
    ShopRefreshQuote = 3,
    ShopRefreshAfter = 4,
    ShopBuyQuote = 5,
    ShopBuyAfter = 6,
    ShopSellQuote = 7,
    ShopSellAfter = 8,
    ShopLeave = 9,
    RoundStart = 10,
    InitialDealBefore = 11,
    InitialDealAfter = 12,
    TrickStart = 13,
    TurnStart = 14,
    PlayActionWindow = 15,
    PlayValidate = 16,
    PlayCommitted = 17,
    PlayRewardBefore = 18,
    PlayRewardAfter = 19,
    PassCommitted = 20,
    TurnEnd = 21,
    TrickEnd = 22,
    DrawBefore = 23,
    DrawAfter = 24,
    TrickDrawsAfter = 25,
    RoundEnd = 26,
    RoundRewardBefore = 27,
    RoundRewardAfter = 28,
    GameEnd = 29,
    CardChanged = 30,
    AbilityResolved = 31,
    FunctionCardEquipped = 32,
    FunctionCardRemoved = 33,
    ShopSlotLocked = 34,
    ShopSlotUnlocked = 35,
    BoosterPackOpened = 36,
    BoosterPackChoiceCompleted = 37,
    BoosterPackSkipped = 38,
    ExpansionCardUsed = 39,
    HandPatternUpgraded = 40,
    FacilityPurchased = 41,
    CoinsChanged = 42,
    ScoreChanged = 43,
    PlayerStatusChanged = 44,
    AbilityAdded = 45,
    AbilityRemoved = 46,
    AbilityAbolished = 47,
    AbilityUsabilityChanged = 48,
    ChoiceOpened = 49,
    ChoiceResolved = 50,
    ChoiceExpired = 51,
    RoundWinnerLocked = 52,
    DrawPileExhausted = 53,
    AllShopsClosed = 54,
    FunctionCardUsabilityChanged = 55,
    FunctionCardAbolished = 56,
    AbilityStateChanged = 57,
    DelayedEffectScheduled = 58,
    DelayedEffectTriggered = 59,
    DelayedEffectExpired = 60,
    CoatingApplied = 61,
    CardVisibilityChanged = 62,

    // 操作请求、效果和控制事件沿用同一套类型编号。
    BuyShopItemRequest = 63,
    BuyBoosterPackRequest = 64,
    SellFunctionCardRequest = 65,
    RefreshShopRequest = 66,
    LockShopSlotRequest = 67,
    UnlockShopSlotRequest = 68,
    ChooseBoosterCardRequest = 69,
    SkipBoosterPackRequest = 70,
    UpgradeHandPatternRequest = 71,
    BuyFacilityRequest = 72,
    FinishShoppingRequest = 73,
    PlayCardsRequest = 74,
    PassRequest = 75,
    ActivateAbilityRequest = 76,
    SubmitChoiceRequest = 77,
    ResolveWildcardRequest = 78,
    ContinueGameRequest = 79,
    StartNewGameRequest = 80,
    StateSyncRequest = 81,
    PreviewPlayRequest = 82,
    QuoteShopOperationRequest = 83,
    AddScore = 84,
    AddCoins = 85,
    SpendCoins = 86,
    AddPendingScore = 87,
    MultiplyPendingScore = 88,
    AddPendingCoins = 89,
    ReduceShopCost = 90,
    GrantFreeRefresh = 91,
    RefreshShop = 92,
    OpenBoosterPack = 93,
    LockShopSlot = 94,
    UnlockShopSlot = 95,
    GrantFunctionCard = 96,
    RemoveFunctionCard = 97,
    ReplaceFunctionCard = 98,
    AddAbility = 99,
    RemoveAbility = 100,
    AbolishAbility = 101,
    SetAbilityUsable = 102,
    SetFunctionCardUsable = 103,
    AbolishFunctionCard = 104,
    ApplyCoating = 105,
    DrawCards = 106,
    SwapCardsWithDeck = 107,
    ChangeCardRank = 108,
    ChangeCardSuit = 109,
    MakeCardJoker = 110,
    RevealCards = 111,
    ApplyPlayerStatus = 112,
    RemovePlayerStatus = 113,
    UpgradeHandPattern = 114,
    PurchaseFacility = 115,
    UseExpansionCard = 116,
    AddDelayedEffect = 117,
    OpenChoice = 118,
    ResolveChoice = 119,
    ChangeAbilityCounter = 120,
    EventBatch = 121,
    PlayerJoinedControl = 122,
    PlayerDisconnectedControl = 123,
    PlayerReconnectedControl = 124,
    ShoppingDeadlineControl = 125,
    TurnDeadlineControl = 126,
    ChoiceDeadlineControl = 127,
    RequestRejectedControl = 128,
    StateSnapshotReadyControl = 129,
    PlayOptionsReadyControl = 130,
    ShopQuoteReadyControl = 131,
    Count = 132
};

inline constexpr std::size_t TOTAL_EVENT_TYPES =
    static_cast<std::size_t>(EventType::Count);

using EventId = std::uint64_t;
using RequestId = std::uint64_t;
using RoomId = std::uint64_t;
using ShopId = std::uint64_t;
using SlotId = std::uint64_t;
using ProductId = std::uint64_t;
using PackId = std::uint64_t;
using ChoiceId = std::uint64_t;
using ChoiceOptionId = std::uint64_t;
using CardId = std::uint64_t;
using DefinitionId = std::uint64_t;
using StatusId = std::uint64_t;
using CounterId = std::uint64_t;
using DelayedEffectId = std::uint64_t;
using RoundId = std::uint64_t;
using TrickId = std::uint64_t;
using TurnId = std::uint64_t;
using PlayedHandId = std::uint64_t;
using SettlementId = std::uint64_t;
using RoundResultId = std::uint64_t;
using SnapshotId = std::uint64_t;
using PreviewId = std::uint64_t;
using ScoreValue = std::int64_t;
using CoinValue = std::int64_t;

enum class HandPattern : std::uint8_t { Single, Pair, Straight, Bomb, StraightFlush };
enum class Suit : std::uint8_t { Spades, Hearts, Clubs, Diamonds };
enum class PassReason : std::uint8_t { Chosen, Timeout, Disconnected };
enum class DrawReason : std::uint8_t { TrickEnd, Skill, Swap };
enum class RemovalReason : std::uint8_t { Sold, Replaced, Consumed, Deleted };
enum class ChoiceKind : std::uint8_t { Booster, Wildcard, Target, Option };
enum class ChoiceReason : std::uint8_t { Manual, Timeout, Disconnected };
enum class CoatingType : std::uint8_t { Gold, Laser };
enum class ContinueMode : std::uint8_t { ExtraThree, Endless };
enum class ShopOperation : std::uint8_t { Purchase, Sale, Refresh, HandUpgrade, Facility };
enum class RefreshMode : std::uint8_t { Paid, Free };
enum class Lifetime : std::uint8_t { Event, Turn, Trick, Round, Shop, Game };
enum class RewardComponent : std::uint8_t { Base, Extra };
enum class RoundingRule : std::uint8_t { Floor, Ceil, Nearest };
enum class CounterOperation : std::uint8_t { Add, Set };

struct CardRef {
    Players owner = Players::P1;
    std::size_t seq = 0;
};

struct AbilityRef {
    CardRef card;
    std::size_t seq = 0;  // 牌内稳定技能编号，不是 vector 下标
};

struct WildcardAssignment {
    CardId cardId = 0;
    int rank = 0;
    Suit suit = Suit::Spades;
};

struct TargetSpec {
    std::vector<Players> players;
    std::vector<CardId> cards;
    std::vector<AbilityRef> abilities;
    std::optional<int> rank;
    std::optional<Suit> suit;
};

struct RewardData {
    SettlementId settlementId = 0;
    ScoreValue score = 0;
    CoinValue coins = 0;
};

struct ShopData {
    SettlementId settlementId = 0;
    CoinValue baseCost = 0;
    CoinValue finalCost = 0;
    std::uint64_t priceVersion = 0;
};

class Event {
public:
    virtual ~Event() = 0;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    const EventType type;  // 唯一确定一种操作或通知
    std::optional<Players> actor;  // 行为发起者；全局事件可以为空

protected:
    explicit Event(EventType type) : type(type) {}
};  // 事件只描述操作标识和参数；执行与调度由外部模块负责。

inline Event::~Event() = default;

using EventPtr = std::unique_ptr<Event>;

// 操作请求（21 个）：表达玩家意图或只读查询。

class BuyShopItemRequest final : public Event {
public:
    BuyShopItemRequest()
        : Event(EventType::BuyShopItemRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
    ProductId productId{};  // 商品
};  // 玩家请求：购买普通货架商品；商店队列；价格由服务端计算。

class BuyBoosterPackRequest final : public Event {
public:
    BuyBoosterPackRequest()
        : Event(EventType::BuyBoosterPackRequest) {}
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 展示中的卡包实例
};  // 玩家请求：购买并立即打开卡包；商店队列；验证空槽与包限购，扣款后立即开包。

class SellFunctionCardRequest final : public Event {
public:
    SellFunctionCardRequest()
        : Event(EventType::SellFunctionCardRequest) {}
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
};  // 玩家请求：出售功能卡；商店队列；出售成本和成长清理按既有规则。

class RefreshShopRequest final : public Event {
public:
    RefreshShopRequest()
        : Event(EventType::RefreshShopRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：刷新普通货架；商店队列；出队时检查刷新上限及当前报价。

class LockShopSlotRequest final : public Event {
public:
    LockShopSlotRequest()
        : Event(EventType::LockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：锁定货位；由 Condition 校验当前状态、权限和参数后处理。

class UnlockShopSlotRequest final : public Event {
public:
    UnlockShopSlotRequest()
        : Event(EventType::UnlockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：解锁货位；由 Condition 校验当前状态、权限和参数后处理。

class ChooseBoosterCardRequest final : public Event {
public:
    ChooseBoosterCardRequest()
        : Event(EventType::ChooseBoosterCardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    ChoiceOptionId optionId{};  // 选中的候选
};  // 玩家请求：选择卡包候选；授权选择回复，不作为下一次普通购物排队。

class SkipBoosterPackRequest final : public Event {
public:
    SkipBoosterPackRequest()
        : Event(EventType::SkipBoosterPackRequest) {}
    ChoiceId choiceId{};  // 待处理交互
};  // 玩家请求：放弃已打开卡包；授权选择回复；不退已支付包价。

class UpgradeHandPatternRequest final : public Event {
public:
    UpgradeHandPatternRequest()
        : Event(EventType::UpgradeHandPatternRequest) {}
    ShopId shopId{};  // 商店会话
    HandPattern pattern{};  // 牌型
};  // 玩家请求：付费加强牌型；由 Condition 校验当前状态、权限和参数后处理。

class BuyFacilityRequest final : public Event {
public:
    BuyFacilityRequest()
        : Event(EventType::BuyFacilityRequest) {}
    ShopId shopId{};  // 商店会话
    DefinitionId facilityId{};  // 设施定义
};  // 玩家请求：购买设施；由 Condition 校验当前状态、权限和参数后处理。

class FinishShoppingRequest final : public Event {
public:
    FinishShoppingRequest()
        : Event(EventType::FinishShoppingRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：完成本次购物；由 Condition 校验当前状态、权限和参数后处理。

class PlayCardsRequest final : public Event {
public:
    PlayCardsRequest()
        : Event(EventType::PlayCardsRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 赖子指定方案
};  // 玩家请求：确认出牌；局内直接；合法提交后才产生出牌成功通知。

class PassRequest final : public Event {
public:
    PassRequest()
        : Event(EventType::PassRequest) {}
    TurnId turnId{};  // 所属回合
};  // 玩家请求：主动不出牌；局内直接；先手不能因此跳过领出。

class ActivateAbilityRequest final : public Event {
public:
    ActivateAbilityRequest()
        : Event(EventType::ActivateAbilityRequest) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 玩家请求：发动主动技能；校验技能归属、窗口、费用与次数；商店和局内依窗口处理。

class SubmitChoiceRequest final : public Event {
public:
    SubmitChoiceRequest()
        : Event(EventType::SubmitChoiceRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 选中的合法选项
};  // 玩家请求：提交一般目标或方案；按交互授权接受，允许非当前行动玩家回复。

class ResolveWildcardRequest final : public Event {
public:
    ResolveWildcardRequest()
        : Event(EventType::ResolveWildcardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 玩家请求：确认赖子指定；按赖子交互授权接受，确认后仍需校验跟牌。

class ContinueGameRequest final : public Event {
public:
    ContinueGameRequest()
        : Event(EventType::ContinueGameRequest) {}
    ContinueMode mode{};  // 续玩模式
};  // 玩家请求：请求加赛或无尽模式；游戏结束后的模式请求；多人一致性规则待既有房间模块确定。

class StartNewGameRequest final : public Event {
public:
    StartNewGameRequest()
        : Event(EventType::StartNewGameRequest) {}
};  // 玩家请求：请求开始新一场；按房间流程验证，新游戏重新初始化资源。

class StateSyncRequest final : public Event {
public:
    StateSyncRequest()
        : Event(EventType::StateSyncRequest) {}
    std::uint64_t lastSeenVersion{};  // 客户端已知版本
};  // 玩家请求：请求状态同步；只读同步通道，不受出牌请求忙碌门禁限制。

class PreviewPlayRequest final : public Event {
public:
    PreviewPlayRequest()
        : Event(EventType::PreviewPlayRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 玩家请求：查询选牌的合法方案；只读预览；不提交出牌、不扣次数、不推进实际随机序列。

class QuoteShopOperationRequest final : public Event {
public:
    QuoteShopOperationRequest()
        : Event(EventType::QuoteShopOperationRequest) {}
    ShopId shopId{};  // 商店会话
    ShopOperation operation{};  // 报价业务
    std::optional<ProductId> productId{};  // 购买商品
    std::optional<CardRef> card{};  // 出售实例
    std::optional<HandPattern> pattern{};  // 加强牌型
    std::optional<DefinitionId> facilityId{};  // 设施
};  // 玩家请求：查询商店操作报价；只读预览；不因重复查询消耗免费额度。

// 业务通知（63 个）：记录已确认的事实或获准的主动调用。

class GameStartedEvent final : public Event {
public:
    GameStartedEvent()
        : Event(EventType::GameStart) {}
    std::vector<Players> participants{};  // 实际参局玩家
    int roundCount{};  // 标准局数
};  // 业务通知：整场开始，由 Condition 在对应时机发布，供被动技能响应。

class ShopEnteredEvent final : public Event {
public:
    ShopEnteredEvent()
        : Event(EventType::ShopEnter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
};  // 业务通知：进入局前商店，由 Condition 在对应时机发布，供被动技能响应。

class ShopAbilityActivationEvent final : public Event {
public:
    ShopAbilityActivationEvent()
        : Event(EventType::ShopActionWindow) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：商店窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class BeforeShopRefreshEvent final : public Event {
public:
    BeforeShopRefreshEvent()
        : Event(EventType::ShopRefreshQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：刷新报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopRefreshedEvent final : public Event {
public:
    ShopRefreshedEvent()
        : Event(EventType::ShopRefreshAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    std::vector<SlotId> slots{};  // 实际替换的货位
    CoinValue paid{};  // 实际费用
};  // 业务通知：刷新成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopPurchaseEvent final : public Event {
public:
    BeforeShopPurchaseEvent()
        : Event(EventType::ShopBuyQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ProductId productId{};  // 商品
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：购买报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopPurchasedEvent final : public Event {
public:
    ShopPurchasedEvent()
        : Event(EventType::ShopBuyAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    ProductId productId{};  // 商品
    CoinValue paid{};  // 实际费用
};  // 业务通知：购买交付成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopSaleEvent final : public Event {
public:
    BeforeShopSaleEvent()
        : Event(EventType::ShopSellQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：出售报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopSoldEvent final : public Event {
public:
    ShopSoldEvent()
        : Event(EventType::ShopSellAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    CoinValue received{};  // 实际收入
};  // 业务通知：出售成功后，由 Condition 在对应时机发布，供被动技能响应。

class ShopLeftEvent final : public Event {
public:
    ShopLeftEvent()
        : Event(EventType::ShopLeave) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    bool timedOut{};  // 是否商店超时
};  // 业务通知：离店，由 Condition 在对应时机发布，供被动技能响应。

class RoundStartedEvent final : public Event {
public:
    RoundStartedEvent()
        : Event(EventType::RoundStart) {}
    RoundId roundId{};  // 所属局
    Players firstPlayer{};  // 本局先手
};  // 业务通知：本局开始，由 Condition 在对应时机发布，供被动技能响应。

class BeforeInitialDealEvent final : public Event {
public:
    BeforeInitialDealEvent()
        : Event(EventType::InitialDealBefore) {}
    RoundId roundId{};  // 所属局
    int cardsPerPlayer = 7;  // 既有规则起手数量
};  // 业务通知：初始发牌前，由 Condition 在对应时机发布，供被动技能响应。

class InitialCardsDealtEvent final : public Event {
public:
    InitialCardsDealtEvent()
        : Event(EventType::InitialDealAfter) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体初始发牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickStartedEvent final : public Event {
public:
    TrickStartedEvent()
        : Event(EventType::TrickStart) {}
    RoundId roundId{};  // 所属局
    TrickId trickId{};  // 所属巡
    Players leader{};  // 领出玩家
};  // 业务通知：本巡开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnStartedEvent final : public Event {
public:
    TurnStartedEvent()
        : Event(EventType::TurnStart) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnAbilityActivationEvent final : public Event {
public:
    TurnAbilityActivationEvent()
        : Event(EventType::PlayActionWindow) {}
    TurnId turnId{};  // 所属回合
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：出牌窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class PlayValidationEvent final : public Event {
public:
    PlayValidationEvent()
        : Event(EventType::PlayValidate) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 业务通知：验证选牌及跟牌，由 Condition 在对应时机发布，供被动技能响应。

class CardsPlayedEvent final : public Event {
public:
    CardsPlayedEvent()
        : Event(EventType::PlayCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 已提交的一手牌
    std::vector<CardId> cards{};  // 实际扑克牌实例
    HandPattern pattern{};  // 最终牌型
};  // 业务通知：成功提交一手牌，由 Condition 在对应时机发布，供被动技能响应。

class BeforePlayRewardEvent final : public Event {
public:
    BeforePlayRewardEvent()
        : Event(EventType::PlayRewardBefore) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 原始奖励
};  // 业务通知：计算本手出牌奖励前，由 Condition 在对应时机发布，供被动技能响应。

class PlayRewardGrantedEvent final : public Event {
public:
    PlayRewardGrantedEvent()
        : Event(EventType::PlayRewardAfter) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    RewardData reward{};  // 实际到账
};  // 业务通知：本手奖励到账后，由 Condition 在对应时机发布，供被动技能响应。

class PlayerPassedEvent final : public Event {
public:
    PlayerPassedEvent()
        : Event(EventType::PassCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PassReason reason{};  // 主动、超时或掉线
};  // 业务通知：确认不出牌，由 Condition 在对应时机发布，供被动技能响应。

class TurnEndedEvent final : public Event {
public:
    TurnEndedEvent()
        : Event(EventType::TurnEnd) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合结算结束，由 Condition 在对应时机发布，供被动技能响应。

class TrickEndedEvent final : public Event {
public:
    TrickEndedEvent()
        : Event(EventType::TrickEnd) {}
    TrickId trickId{};  // 所属巡
    Players leader{};  // 牌权获得者
};  // 业务通知：获得牌权并结束本巡，由 Condition 在对应时机发布，供被动技能响应。

class BeforeCardsDrawEvent final : public Event {
public:
    BeforeCardsDrawEvent()
        : Event(EventType::DrawBefore) {}
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    int requestedCount{};  // 请求摸牌数
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：非初始摸牌前，由 Condition 在对应时机发布，供被动技能响应。

class CardsDrawnEvent final : public Event {
public:
    CardsDrawnEvent()
        : Event(EventType::DrawAfter) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：实际摸牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickDrawingCompletedEvent final : public Event {
public:
    TrickDrawingCompletedEvent()
        : Event(EventType::TrickDrawsAfter) {}
    TrickId trickId{};  // 所属巡
    int actualTotal{};  // 全体实际摸牌总数
};  // 业务通知：巡末摸牌步骤完成，由 Condition 在对应时机发布，供被动技能响应。

class RoundEndedEvent final : public Event {
public:
    RoundEndedEvent()
        : Event(EventType::RoundEnd) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 已锁定结果
};  // 业务通知：本局结束且最后回合结算完成，由 Condition 在对应时机发布，供被动技能响应。

class BeforeRoundRewardEvent final : public Event {
public:
    BeforeRoundRewardEvent()
        : Event(EventType::RoundRewardBefore) {}
    RoundId roundId{};  // 所属局
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 基础待结算奖励
};  // 业务通知：计算玩家局末奖励前，由 Condition 在对应时机发布，供被动技能响应。

class RoundRewardsGrantedEvent final : public Event {
public:
    RoundRewardsGrantedEvent()
        : Event(EventType::RoundRewardAfter) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 实际结算记录
};  // 业务通知：全体局末奖励与扣款完成，由 Condition 在对应时机发布，供被动技能响应。

class GameEndedEvent final : public Event {
public:
    GameEndedEvent()
        : Event(EventType::GameEnd) {}
    std::vector<Players> winners{};  // 并列冠军
    std::uint64_t finalVersion{};  // 最终状态版本
};  // 业务通知：整场结束与冠军确认，由 Condition 在对应时机发布，供被动技能响应。

class PlayingCardsChangedEvent final : public Event {
public:
    PlayingCardsChangedEvent()
        : Event(EventType::CardChanged) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::string reason{};  // 变化原因
};  // 业务通知：技能实际改变牌面或手牌后，由 Condition 在对应时机发布，供被动技能响应。

class AbilityResolvedEvent final : public Event {
public:
    AbilityResolvedEvent()
        : Event(EventType::AbilityResolved) {}
    AbilityRef ability{};  // 目标技能
    EventId triggerEventId{};  // 触发事件
};  // 业务通知：技能效果成功结算后，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardEquippedEvent final : public Event {
public:
    FunctionCardEquippedEvent()
        : Event(EventType::FunctionCardEquipped) {}
    CardRef card{};  // 功能卡实例
    std::size_t slot{};  // 实际槽位
};  // 业务通知：功能卡进入槽位，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardRemovedEvent final : public Event {
public:
    FunctionCardRemovedEvent()
        : Event(EventType::FunctionCardRemoved) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 业务通知：功能卡离开槽位，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotLockedEvent final : public Event {
public:
    ShopSlotLockedEvent()
        : Event(EventType::ShopSlotLocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位锁定成功，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotUnlockedEvent final : public Event {
public:
    ShopSlotUnlockedEvent()
        : Event(EventType::ShopSlotUnlocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位解锁成功，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackOpenedEvent final : public Event {
public:
    BoosterPackOpenedEvent()
        : Event(EventType::BoosterPackOpened) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：卡包已付费打开，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackChoiceCompletedEvent final : public Event {
public:
    BoosterPackChoiceCompletedEvent()
        : Event(EventType::BoosterPackChoiceCompleted) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    std::vector<ChoiceOptionId> options{};  // 实际选项
};  // 业务通知：卡包候选选取完成，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackSkippedEvent final : public Event {
public:
    BoosterPackSkippedEvent()
        : Event(EventType::BoosterPackSkipped) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
};  // 业务通知：卡包已放弃，由 Condition 在对应时机发布，供被动技能响应。

class ExpansionCardUsedEvent final : public Event {
public:
    ExpansionCardUsedEvent()
        : Event(EventType::ExpansionCardUsed) {}
    Players player{};  // 相关玩家
    DefinitionId expansionId{};  // 拓展牌定义
    TargetSpec selection{};  // 目标与方案
};  // 业务通知：拓展牌获得即使用完成，由 Condition 在对应时机发布，供被动技能响应。

class HandPatternUpgradedEvent final : public Event {
public:
    HandPatternUpgradedEvent()
        : Event(EventType::HandPatternUpgraded) {}
    Players player{};  // 相关玩家
    HandPattern pattern{};  // 牌型
    int beforeLevel{};  // 原等级
    int afterLevel{};  // 新等级
};  // 业务通知：牌型加强完成，由 Condition 在对应时机发布，供被动技能响应。

class FacilityPurchasedEvent final : public Event {
public:
    FacilityPurchasedEvent()
        : Event(EventType::FacilityPurchased) {}
    Players player{};  // 相关玩家
    DefinitionId facilityId{};  // 设施定义
};  // 业务通知：设施购买生效，由 Condition 在对应时机发布，供被动技能响应。

class CoinsChangedEvent final : public Event {
public:
    CoinsChangedEvent()
        : Event(EventType::CoinsChanged) {}
    Players player{};  // 相关玩家
    CoinValue before{};  // 变化前
    CoinValue after{};  // 变化后
    std::string reason{};  // 收入或支出原因
};  // 业务通知：金币余额实际变化，由 Condition 在对应时机发布，供被动技能响应。

class ScoreChangedEvent final : public Event {
public:
    ScoreChangedEvent()
        : Event(EventType::ScoreChanged) {}
    Players player{};  // 相关玩家
    ScoreValue before{};  // 变化前
    ScoreValue after{};  // 变化后
};  // 业务通知：累计分数实际变化，由 Condition 在对应时机发布，供被动技能响应。

class PlayerStatusChangedEvent final : public Event {
public:
    PlayerStatusChangedEvent()
        : Event(EventType::PlayerStatusChanged) {}
    Players player{};  // 相关玩家
    StatusId statusId{};  // 状态实例
    std::string reason{};  // 增加、移除或层数变化
};  // 业务通知：玩家状态实际变化，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAddedEvent final : public Event {
public:
    AbilityAddedEvent()
        : Event(EventType::AbilityAdded) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能添加完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityRemovedEvent final : public Event {
public:
    AbilityRemovedEvent()
        : Event(EventType::AbilityRemoved) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能逻辑移除完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAbolishedEvent final : public Event {
public:
    AbilityAbolishedEvent()
        : Event(EventType::AbilityAbolished) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能被永久废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityUsabilityChangedEvent final : public Event {
public:
    AbilityUsabilityChangedEvent()
        : Event(EventType::AbilityUsabilityChanged) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 当前开关
};  // 业务通知：技能临时可用开关变化，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceOpenedEvent final : public Event {
public:
    ChoiceOpenedEvent()
        : Event(EventType::ChoiceOpened) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    ChoiceKind choiceKind{};  // 交互类型
};  // 业务通知：交互选择窗口建立，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceResolvedEvent final : public Event {
public:
    ChoiceResolvedEvent()
        : Event(EventType::ChoiceResolved) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选择
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 业务通知：交互选择处理完成，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceExpiredEvent final : public Event {
public:
    ChoiceExpiredEvent()
        : Event(EventType::ChoiceExpired) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：交互期限已到并完成默认处理，由 Condition 在对应时机发布，供被动技能响应。

class RoundWinnerLockedEvent final : public Event {
public:
    RoundWinnerLockedEvent()
        : Event(EventType::RoundWinnerLocked) {}
    RoundId roundId{};  // 所属局
    Players winner{};  // 赢家
    RoundResultId resultId{};  // 冻结结果
};  // 业务通知：出空时锁定赢家与手牌快照，由 Condition 在对应时机发布，供被动技能响应。

class DrawPileExhaustedEvent final : public Event {
public:
    DrawPileExhaustedEvent()
        : Event(EventType::DrawPileExhausted) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：牌堆由非空变为空，由 Condition 在对应时机发布，供被动技能响应。

class AllShopsClosedEvent final : public Event {
public:
    AllShopsClosedEvent()
        : Event(EventType::AllShopsClosed) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体商店会话关闭且交易结清，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardUsabilityChangedEvent final : public Event {
public:
    FunctionCardUsabilityChangedEvent()
        : Event(EventType::FunctionCardUsabilityChanged) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 当前开关
};  // 业务通知：整张功能卡临时开关变化，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardAbolishedEvent final : public Event {
public:
    FunctionCardAbolishedEvent()
        : Event(EventType::FunctionCardAbolished) {}
    CardRef card{};  // 功能卡实例
};  // 业务通知：整张功能卡被废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityStateChangedEvent final : public Event {
public:
    AbilityStateChangedEvent()
        : Event(EventType::AbilityStateChanged) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 已定义的计数状态
    int before{};  // 变化前
    int after{};  // 变化后
};  // 业务通知：技能成长或蓄力状态变化，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectScheduledEvent final : public Event {
public:
    DelayedEffectScheduledEvent()
        : Event(EventType::DelayedEffectScheduled) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟效果标记建立，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectTriggeredEvent final : public Event {
public:
    DelayedEffectTriggeredEvent()
        : Event(EventType::DelayedEffectTriggered) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记实际执行完成，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectExpiredEvent final : public Event {
public:
    DelayedEffectExpiredEvent()
        : Event(EventType::DelayedEffectExpired) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记结束或失效，由 Condition 在对应时机发布，供被动技能响应。

class CoatingAppliedEvent final : public Event {
public:
    CoatingAppliedEvent()
        : Event(EventType::CoatingApplied) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 业务通知：镀层施加完成，由 Condition 在对应时机发布，供被动技能响应。

class CardVisibilityChangedEvent final : public Event {
public:
    CardVisibilityChangedEvent()
        : Event(EventType::CardVisibilityChanged) {}
    Players viewer{};  // 获得信息的玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 业务通知：查看权限或公开信息变化，由 Condition 在对应时机发布，供被动技能响应。

// 效果请求（38 个）：表达需要执行的效果。

class AddScoreEvent final : public Event {
public:
    AddScoreEvent()
        : Event(EventType::AddScore) {}
    Players target{};  // 效果目标玩家
    ScoreValue amount{};  // 增加数量
};  // 效果请求：增加实际分数；由 Condition 校验并执行。

class AddCoinsEvent final : public Event {
public:
    AddCoinsEvent()
        : Event(EventType::AddCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 增加数量
};  // 效果请求：增加实际金币；由 Condition 校验并执行。

class SpendCoinsEvent final : public Event {
public:
    SpendCoinsEvent()
        : Event(EventType::SpendCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 扣款数量
    std::string reason{};  // 扣款用途
};  // 效果请求：扣除实际金币；由 Condition 校验并执行。

class AddPendingScoreEvent final : public Event {
public:
    AddPendingScoreEvent()
        : Event(EventType::AddPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    ScoreValue delta{};  // 有符号增减值
};  // 效果请求：修正待结算分数；由 Condition 校验并执行。

class MultiplyPendingScoreEvent final : public Event {
public:
    MultiplyPendingScoreEvent()
        : Event(EventType::MultiplyPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    int numerator = 1;  // 倍率分子
    int denominator = 1;  // 倍率分母
    RoundingRule rounding{};  // 取整规则
};  // 效果请求：缩放待结算分数；由 Condition 校验并执行。

class AddPendingCoinsEvent final : public Event {
public:
    AddPendingCoinsEvent()
        : Event(EventType::AddPendingCoins) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue delta{};  // 有符号增减值
    RewardComponent component{};  // 基础或额外奖励
    CoinValue minimum{};  // 该部分最低金额
};  // 效果请求：修正待结算金币；由 Condition 校验并执行。

class ReduceShopCostEvent final : public Event {
public:
    ReduceShopCostEvent()
        : Event(EventType::ReduceShopCost) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue amount{};  // 减费数
    CoinValue minimum{};  // 卡牌规定的最低价
};  // 效果请求：降低本次商店报价；由 Condition 校验并执行。

class GrantFreeRefreshEvent final : public Event {
public:
    GrantFreeRefreshEvent()
        : Event(EventType::GrantFreeRefresh) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    int uses{};  // 额度数量
};  // 效果请求：增加本店免费刷新额度；由 Condition 校验并执行。

class RefreshShopEvent final : public Event {
public:
    RefreshShopEvent()
        : Event(EventType::RefreshShop) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    RefreshMode mode{};  // 付费或免费
};  // 效果请求：执行一次货架刷新；由 Condition 校验并执行，仍检查本店刷新总上限，免费不推进付费价格序号。

class OpenBoosterPackEvent final : public Event {
public:
    OpenBoosterPackEvent()
        : Event(EventType::OpenBoosterPack) {}
    Players target{};  // 开包玩家
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 卡包实例
    SettlementId settlementId{};  // 已验证的购买或授予事务
};  // 效果请求：执行已获准的开包；由 Condition 校验并执行，与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择。

class LockShopSlotEvent final : public Event {
public:
    LockShopSlotEvent()
        : Event(EventType::LockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位锁定；由 Condition 校验并执行。

class UnlockShopSlotEvent final : public Event {
public:
    UnlockShopSlotEvent()
        : Event(EventType::UnlockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位解锁；由 Condition 校验并执行。

class GrantFunctionCardEvent final : public Event {
public:
    GrantFunctionCardEvent()
        : Event(EventType::GrantFunctionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId definitionId{};  // 牌种
    CoinValue purchaseCost{};  // 实例出售成本
};  // 效果请求：生成并装备功能卡；由 Condition 校验并执行，检查空槽；免费生成成本为零。

class RemoveFunctionCardEvent final : public Event {
public:
    RemoveFunctionCardEvent()
        : Event(EventType::RemoveFunctionCard) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 效果请求：移除功能卡；由 Condition 校验并执行。

class ReplaceFunctionCardEvent final : public Event {
public:
    ReplaceFunctionCardEvent()
        : Event(EventType::ReplaceFunctionCard) {}
    CardRef card{};  // 功能卡实例
    DefinitionId newDefinitionId{};  // 新牌种
    bool preservePurchaseCost = true;  // 是否沿用出售成本
};  // 效果请求：替换功能卡；由 Condition 校验并执行。

class AddAbilityEvent final : public Event {
public:
    AddAbilityEvent()
        : Event(EventType::AddAbility) {}
    CardRef card{};  // 功能卡实例
    DefinitionId abilityDefinitionId{};  // 技能定义
};  // 效果请求：给功能卡添加技能；由 Condition 校验并执行。

class RemoveAbilityEvent final : public Event {
public:
    RemoveAbilityEvent()
        : Event(EventType::RemoveAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：逻辑删除技能；由 Condition 校验并执行。

class AbolishAbilityEvent final : public Event {
public:
    AbolishAbilityEvent()
        : Event(EventType::AbolishAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：永久废除技能；由 Condition 校验并执行。

class SetAbilityUsableEvent final : public Event {
public:
    SetAbilityUsableEvent()
        : Event(EventType::SetAbilityUsable) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 新开关
};  // 效果请求：设置技能临时开关；由 Condition 校验并执行。

class SetFunctionCardUsableEvent final : public Event {
public:
    SetFunctionCardUsableEvent()
        : Event(EventType::SetFunctionCardUsable) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 新开关
};  // 效果请求：设置整牌临时开关；由 Condition 校验并执行。

class AbolishFunctionCardEvent final : public Event {
public:
    AbolishFunctionCardEvent()
        : Event(EventType::AbolishFunctionCard) {}
    CardRef card{};  // 功能卡实例
};  // 效果请求：永久废除整张牌；由 Condition 校验并执行。

class ApplyCoatingEvent final : public Event {
public:
    ApplyCoatingEvent()
        : Event(EventType::ApplyCoating) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 效果请求：给功能卡施加镀层；由 Condition 校验并执行。

class DrawCardsEvent final : public Event {
public:
    DrawCardsEvent()
        : Event(EventType::DrawCards) {}
    Players target{};  // 效果目标玩家
    int count{};  // 数量
    DrawReason reason{};  // 摸牌原因
};  // 效果请求：按规则执行非初始摸牌；由 Condition 校验并执行，锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理。

class SwapCardsWithDeckEvent final : public Event {
public:
    SwapCardsWithDeckEvent()
        : Event(EventType::SwapCardsWithDeck) {}
    Players target{};  // 效果目标玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：将指定手牌换到牌堆底并等量摸顶；由 Condition 校验并执行，保持原牌身份与公共牌组总数。

class ChangeCardRankEvent final : public Event {
public:
    ChangeCardRankEvent()
        : Event(EventType::ChangeCardRank) {}
    CardId cardId{};  // 扑克牌实例
    int rank{};  // 目标点数
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变普通牌点数；由 Condition 校验并执行。

class ChangeCardSuitEvent final : public Event {
public:
    ChangeCardSuitEvent()
        : Event(EventType::ChangeCardSuit) {}
    CardId cardId{};  // 扑克牌实例
    Suit suit{};  // 目标花色
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变花色；由 Condition 校验并执行。

class MakeCardJokerEvent final : public Event {
public:
    MakeCardJokerEvent()
        : Event(EventType::MakeCardJoker) {}
    CardId cardId{};  // 扑克牌实例
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：将指定牌临时变为王；由 Condition 校验并执行。

class RevealCardsEvent final : public Event {
public:
    RevealCardsEvent()
        : Event(EventType::RevealCards) {}
    Players viewer{};  // 查看者
    Players target{};  // 被查看玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：向指定玩家展示合法信息；由 Condition 校验并执行，只向有权限的客户端同步牌面。

class ApplyPlayerStatusEvent final : public Event {
public:
    ApplyPlayerStatusEvent()
        : Event(EventType::ApplyPlayerStatus) {}
    Players target{};  // 效果目标玩家
    DefinitionId statusDefinitionId{};  // 状态定义
    int stacks = 1;  // 层数
    Lifetime lifetime{};  // 期限
};  // 效果请求：施加玩家状态；由 Condition 校验并执行。

class RemovePlayerStatusEvent final : public Event {
public:
    RemovePlayerStatusEvent()
        : Event(EventType::RemovePlayerStatus) {}
    Players target{};  // 效果目标玩家
    StatusId statusId{};  // 状态实例
};  // 效果请求：移除玩家状态；由 Condition 校验并执行。

class UpgradeHandPatternEvent final : public Event {
public:
    UpgradeHandPatternEvent()
        : Event(EventType::UpgradeHandPattern) {}
    Players target{};  // 效果目标玩家
    HandPattern pattern{};  // 牌型
    int levels = 1;  // 加强级数
};  // 效果请求：执行牌型加强；由 Condition 校验并执行。

class PurchaseFacilityEvent final : public Event {
public:
    PurchaseFacilityEvent()
        : Event(EventType::PurchaseFacility) {}
    Players target{};  // 效果目标玩家
    DefinitionId facilityId{};  // 设施
};  // 效果请求：执行设施购买与生效；由 Condition 校验并执行。

class UseExpansionCardEvent final : public Event {
public:
    UseExpansionCardEvent()
        : Event(EventType::UseExpansionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId expansionId{};  // 拓展牌
    TargetSpec selection{};  // 目标与方案
};  // 效果请求：执行获得即使用的拓展牌；由 Condition 校验并执行。

class AddDelayedEffectEvent final : public Event {
public:
    AddDelayedEffectEvent()
        : Event(EventType::AddDelayedEffect) {}
    Players target{};  // 效果目标玩家
    DefinitionId effectDefinitionId{};  // 延迟效果定义
    EventType triggerType{};  // 未来触发的通知类型
    TargetSpec selection{};  // 目标与方案
    Lifetime lifetime{};  // 有效期
};  // 效果请求：建立延迟效果标记；由 Condition 校验并执行。

class OpenChoiceEvent final : public Event {
public:
    OpenChoiceEvent()
        : Event(EventType::OpenChoice) {}
    Players target{};  // 效果目标玩家
    ChoiceKind choiceKind{};  // 选择类型
    std::vector<ChoiceOptionId> options{};  // 合法选项
    std::uint32_t timeoutMs{};  // 超时长度
    bool optional{};  // 是否可放弃
};  // 效果请求：建立选择窗口并挂起相关流程；由 Condition 校验并执行。

class ResolveChoiceEvent final : public Event {
public:
    ResolveChoiceEvent()
        : Event(EventType::ResolveChoice) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选项
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 效果请求：执行选择或默认方案；由 Condition 校验并执行。

class ChangeAbilityCounterEvent final : public Event {
public:
    ChangeAbilityCounterEvent()
        : Event(EventType::ChangeAbilityCounter) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 计数项
    CounterOperation operation{};  // 加值或设值
    int value{};  // 操作值
};  // 效果请求：改变已定义的技能成长或蓄力计数；由 Condition 校验并执行。

class EventBatch final : public Event {
public:
    EventBatch()
        : Event(EventType::EventBatch) {}
    std::vector<EventPtr> events{};  // 有序子效果
};  // 效果请求：有序原子组合多个效果；由 Condition 校验并执行，只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待。

// 控制事件（10 个）：连接、计时器和服务端反馈。

class PlayerJoinedControl final : public Event {
public:
    PlayerJoinedControl()
        : Event(EventType::PlayerJoinedControl) {}
    Players player{};  // 相关玩家
};  // 控制事件：玩家加入房间，由 Condition 更新房间流程，满足满员条件后自动开局。

class PlayerDisconnectedControl final : public Event {
public:
    PlayerDisconnectedControl()
        : Event(EventType::PlayerDisconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 连接代次
};  // 控制事件：连接断开，保留原座位与资源，按对应超时规则继续处理。

class PlayerReconnectedControl final : public Event {
public:
    PlayerReconnectedControl()
        : Event(EventType::PlayerReconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 新连接代次
};  // 控制事件：原玩家重连，恢复原座位并同步有权查看的当前状态。

class ShoppingDeadlineControl final : public Event {
public:
    ShoppingDeadlineControl()
        : Event(EventType::ShoppingDeadlineControl) {}
    ShopId shopId{};  // 商店会话
    std::uint64_t expectedVersion{};  // 对应商店版本
};  // 控制事件：商店计时到期，由 Condition 检查商店会话与版本后执行结束流程。

class TurnDeadlineControl final : public Event {
public:
    TurnDeadlineControl()
        : Event(EventType::TurnDeadlineControl) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::uint64_t expectedVersion{};  // 对应回合版本
};  // 控制事件：出牌计时到期，由 Condition 检查回合与版本后执行既定超时操作。

class ChoiceDeadlineControl final : public Event {
public:
    ChoiceDeadlineControl()
        : Event(EventType::ChoiceDeadlineControl) {}
    ChoiceId choiceId{};  // 待处理交互
    std::uint64_t expectedVersion{};  // 对应交互版本
};  // 控制事件：选择计时到期，由 Condition 检查交互与版本后执行默认方案。

class RequestRejectedControl final : public Event {
public:
    RequestRejectedControl()
        : Event(EventType::RequestRejectedControl) {}
    RequestId requestId{};  // 被拒绝请求
    std::string reason{};  // 拒绝原因
    std::uint64_t currentVersion{};  // 当前版本
};  // 控制反馈：操作请求未获接受，只反馈原因，不视为成功行为或成功类收益来源。

class StateSnapshotReadyControl final : public Event {
public:
    StateSnapshotReadyControl()
        : Event(EventType::StateSnapshotReadyControl) {}
    Players player{};  // 相关玩家
    SnapshotId snapshotId{};  // 按权限过滤的快照
};  // 控制反馈：同步快照已经准备，只向指定玩家发送其有权查看的信息。

class PlayOptionsReadyControl final : public Event {
public:
    PlayOptionsReadyControl()
        : Event(EventType::PlayOptionsReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    std::vector<ChoiceOptionId> options{};  // 合法方案
};  // 查询反馈：合法出牌或赖子方案已经计算，表示预览结果，尚未成功出牌。

class ShopQuoteReadyControl final : public Event {
public:
    ShopQuoteReadyControl()
        : Event(EventType::ShopQuoteReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    ShopData quote{};  // 当前报价
};  // 查询反馈：商店报价已经计算，本身不扣款或消耗免费刷新额度。

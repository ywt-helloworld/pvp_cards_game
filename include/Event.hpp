#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// 
enum class Players : std::uint8_t { P1, P2, P3, P4 };
enum class AbilityType : std::uint8_t { Active, Passive };

// 可被技能订阅的 63 种业务通知类型；编号与具体 SignalEvent 一一对应。
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
    Count = 63
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

// 原 Signal/Effect 的数值保留，新增类别追加。
enum class EventKind : std::uint8_t {
    Signal = 0, Effect = 1, Request = 2, Control = 3
};

enum class EventOrigin : std::uint8_t { Server, Client, Timer, Connection };
enum class ExecutionMode : std::uint8_t { Commit, Preview };
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

struct EventMeta {
    RoomId roomId = 0;
    EventId eventId = 0;
    std::optional<EventId> parentEventId;
    EventId rootEventId = 0;
    std::optional<Players> actor;  // 行为发起者；全局事件可以为空
    std::optional<AbilityRef> originAbility;
    std::uint64_t stateVersion = 0;
    EventOrigin origin = EventOrigin::Server;
    ExecutionMode mode = ExecutionMode::Commit;
};

class Event {
public:
    virtual ~Event() = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    virtual EventKind kind() const noexcept = 0;
    virtual std::optional<EventType> event_type() const noexcept {
        return std::nullopt;
    }
    const char* name() const noexcept { return name_; }

    EventMeta meta;  // 由可信入口和 Condition 填写，不信任客户端自报值

protected:
    explicit Event(const char* name) : name_(name) {}

private:
    const char* name_;  // 派生类传入固定字符串
};  // 所有事件的抽象基类，统一提供元数据、事件类别和虚析构接口。

using EventPtr = std::unique_ptr<Event>;

class RequestEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Request; }
    RequestId requestId = 0;
    std::uint64_t expectedVersion = 0;

protected:
    explicit RequestEvent(const char* name) : Event(name) {}
};  // 操作请求的中间基类，携带请求编号和预期版本，由 Condition 校验后处理。

class SignalEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Signal; }
    virtual std::optional<EventType> event_type() const noexcept override = 0;
    AbilityType source() const noexcept { return source_; }

protected:
    explicit SignalEvent(const char* name,
                         AbilityType source = AbilityType::Passive)
        : Event(name), source_(source) {}

private:
    AbilityType source_;
};  // 业务通知的中间基类，提供事件类型与主动、被动来源，供技能匹配。

// 只复用固定事件类型的实现；每个具体通知仍是独立的具名派生类。
template<EventType Type>
class TypedSignalEvent : public SignalEvent {
public:
    std::optional<EventType> event_type() const noexcept final { return Type; }

protected:
    explicit TypedSignalEvent(const char* name,
                              AbilityType source = AbilityType::Passive)
        : SignalEvent(name, source) {}
};  // 固定类型通知模板，复用 event_type() 实现；每种业务通知仍有独立派生类。

class EffectEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Effect; }

protected:
    explicit EffectEvent(const char* name) : Event(name) {}
};  // 效果请求的中间基类，由 Condition 校验并执行，成功后产生对应业务通知。

class ControlEvent : public Event {
public:
    EventKind kind() const noexcept final { return EventKind::Control; }

protected:
    explicit ControlEvent(const char* name) : Event(name) {}
};  // 控制事件的中间基类，承载连接、计时器和服务端反馈，不直接作为技能成功行为。

// 操作请求（21 个）：表达玩家意图或只读查询。

class BuyShopItemRequest final : public RequestEvent {
public:
    BuyShopItemRequest() : RequestEvent("BuyShopItemRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
    ProductId productId{};  // 商品
};  // 玩家请求：购买普通货架商品；商店队列；价格由服务端计算。

class BuyBoosterPackRequest final : public RequestEvent {
public:
    BuyBoosterPackRequest() : RequestEvent("BuyBoosterPackRequest") {}
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 展示中的卡包实例
};  // 玩家请求：购买并立即打开卡包；商店队列；验证空槽与包限购，扣款后立即开包。

class SellFunctionCardRequest final : public RequestEvent {
public:
    SellFunctionCardRequest() : RequestEvent("SellFunctionCardRequest") {}
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
};  // 玩家请求：出售功能卡；商店队列；出售成本和成长清理按既有规则。

class RefreshShopRequest final : public RequestEvent {
public:
    RefreshShopRequest() : RequestEvent("RefreshShopRequest") {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：刷新普通货架；商店队列；出队时检查刷新上限及当前报价。

class LockShopSlotRequest final : public RequestEvent {
public:
    LockShopSlotRequest() : RequestEvent("LockShopSlotRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：锁定货位；由 Condition 校验当前状态、权限和参数后处理。

class UnlockShopSlotRequest final : public RequestEvent {
public:
    UnlockShopSlotRequest() : RequestEvent("UnlockShopSlotRequest") {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：解锁货位；由 Condition 校验当前状态、权限和参数后处理。

class ChooseBoosterCardRequest final : public RequestEvent {
public:
    ChooseBoosterCardRequest() : RequestEvent("ChooseBoosterCardRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    ChoiceOptionId optionId{};  // 选中的候选
};  // 玩家请求：选择卡包候选；授权选择回复，不作为下一次普通购物排队。

class SkipBoosterPackRequest final : public RequestEvent {
public:
    SkipBoosterPackRequest() : RequestEvent("SkipBoosterPackRequest") {}
    ChoiceId choiceId{};  // 待处理交互
};  // 玩家请求：放弃已打开卡包；授权选择回复；不退已支付包价。

class UpgradeHandPatternRequest final : public RequestEvent {
public:
    UpgradeHandPatternRequest() : RequestEvent("UpgradeHandPatternRequest") {}
    ShopId shopId{};  // 商店会话
    HandPattern pattern{};  // 牌型
};  // 玩家请求：付费加强牌型；由 Condition 校验当前状态、权限和参数后处理。

class BuyFacilityRequest final : public RequestEvent {
public:
    BuyFacilityRequest() : RequestEvent("BuyFacilityRequest") {}
    ShopId shopId{};  // 商店会话
    DefinitionId facilityId{};  // 设施定义
};  // 玩家请求：购买设施；由 Condition 校验当前状态、权限和参数后处理。

class FinishShoppingRequest final : public RequestEvent {
public:
    FinishShoppingRequest() : RequestEvent("FinishShoppingRequest") {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：完成本次购物；由 Condition 校验当前状态、权限和参数后处理。

class PlayCardsRequest final : public RequestEvent {
public:
    PlayCardsRequest() : RequestEvent("PlayCardsRequest") {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 赖子指定方案
};  // 玩家请求：确认出牌；局内直接；合法提交后才产生出牌成功通知。

class PassRequest final : public RequestEvent {
public:
    PassRequest() : RequestEvent("PassRequest") {}
    TurnId turnId{};  // 所属回合
};  // 玩家请求：主动不出牌；局内直接；先手不能因此跳过领出。

class ActivateAbilityRequest final : public RequestEvent {
public:
    ActivateAbilityRequest() : RequestEvent("ActivateAbilityRequest") {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 玩家请求：发动主动技能；校验技能归属、窗口、费用与次数；商店和局内依窗口处理。

class SubmitChoiceRequest final : public RequestEvent {
public:
    SubmitChoiceRequest() : RequestEvent("SubmitChoiceRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 选中的合法选项
};  // 玩家请求：提交一般目标或方案；按交互授权接受，允许非当前行动玩家回复。

class ResolveWildcardRequest final : public RequestEvent {
public:
    ResolveWildcardRequest() : RequestEvent("ResolveWildcardRequest") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 玩家请求：确认赖子指定；按赖子交互授权接受，确认后仍需校验跟牌。

class ContinueGameRequest final : public RequestEvent {
public:
    ContinueGameRequest() : RequestEvent("ContinueGameRequest") {}
    ContinueMode mode{};  // 续玩模式
};  // 玩家请求：请求加赛或无尽模式；游戏结束后的模式请求；多人一致性规则待既有房间模块确定。

class StartNewGameRequest final : public RequestEvent {
public:
    StartNewGameRequest() : RequestEvent("StartNewGameRequest") {}
};  // 玩家请求：请求开始新一场；按房间流程验证，新游戏重新初始化资源。

class StateSyncRequest final : public RequestEvent {
public:
    StateSyncRequest() : RequestEvent("StateSyncRequest") {}
    std::uint64_t lastSeenVersion{};  // 客户端已知版本
};  // 玩家请求：请求状态同步；只读同步通道，不受出牌请求忙碌门禁限制。

class PreviewPlayRequest final : public RequestEvent {
public:
    PreviewPlayRequest() : RequestEvent("PreviewPlayRequest") {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 玩家请求：查询选牌的合法方案；只读预览；不提交出牌、不扣次数、不推进实际随机序列。

class QuoteShopOperationRequest final : public RequestEvent {
public:
    QuoteShopOperationRequest() : RequestEvent("QuoteShopOperationRequest") {}
    ShopId shopId{};  // 商店会话
    ShopOperation operation{};  // 报价业务
    std::optional<ProductId> productId{};  // 购买商品
    std::optional<CardRef> card{};  // 出售实例
    std::optional<HandPattern> pattern{};  // 加强牌型
    std::optional<DefinitionId> facilityId{};  // 设施
};  // 玩家请求：查询商店操作报价；只读预览；不因重复查询消耗免费额度。

// 业务通知（63 个）：记录已确认的事实或获准的主动调用。

class GameStartedEvent final : public TypedSignalEvent<EventType::GameStart> {
public:
    GameStartedEvent() : TypedSignalEvent<EventType::GameStart>("GameStartedEvent") {}
    std::vector<Players> participants{};  // 实际参局玩家
    int roundCount{};  // 标准局数
};  // 业务通知：整场开始，由 Condition 在对应时机发布，供被动技能响应。

class ShopEnteredEvent final : public TypedSignalEvent<EventType::ShopEnter> {
public:
    ShopEnteredEvent() : TypedSignalEvent<EventType::ShopEnter>("ShopEnteredEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
};  // 业务通知：进入局前商店，由 Condition 在对应时机发布，供被动技能响应。

class ShopAbilityActivationEvent final : public TypedSignalEvent<EventType::ShopActionWindow> {
public:
    ShopAbilityActivationEvent() : TypedSignalEvent<EventType::ShopActionWindow>("ShopAbilityActivationEvent", AbilityType::Active) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：商店窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class BeforeShopRefreshEvent final : public TypedSignalEvent<EventType::ShopRefreshQuote> {
public:
    BeforeShopRefreshEvent() : TypedSignalEvent<EventType::ShopRefreshQuote>("BeforeShopRefreshEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：刷新报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopRefreshedEvent final : public TypedSignalEvent<EventType::ShopRefreshAfter> {
public:
    ShopRefreshedEvent() : TypedSignalEvent<EventType::ShopRefreshAfter>("ShopRefreshedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    std::vector<SlotId> slots{};  // 实际替换的货位
    CoinValue paid{};  // 实际费用
};  // 业务通知：刷新成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopPurchaseEvent final : public TypedSignalEvent<EventType::ShopBuyQuote> {
public:
    BeforeShopPurchaseEvent() : TypedSignalEvent<EventType::ShopBuyQuote>("BeforeShopPurchaseEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ProductId productId{};  // 商品
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：购买报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopPurchasedEvent final : public TypedSignalEvent<EventType::ShopBuyAfter> {
public:
    ShopPurchasedEvent() : TypedSignalEvent<EventType::ShopBuyAfter>("ShopPurchasedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    ProductId productId{};  // 商品
    CoinValue paid{};  // 实际费用
};  // 业务通知：购买交付成功后，由 Condition 在对应时机发布，供被动技能响应。

class BeforeShopSaleEvent final : public TypedSignalEvent<EventType::ShopSellQuote> {
public:
    BeforeShopSaleEvent() : TypedSignalEvent<EventType::ShopSellQuote>("BeforeShopSaleEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：出售报价前，由 Condition 在对应时机发布，供被动技能响应。

class ShopSoldEvent final : public TypedSignalEvent<EventType::ShopSellAfter> {
public:
    ShopSoldEvent() : TypedSignalEvent<EventType::ShopSellAfter>("ShopSoldEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    CoinValue received{};  // 实际收入
};  // 业务通知：出售成功后，由 Condition 在对应时机发布，供被动技能响应。

class ShopLeftEvent final : public TypedSignalEvent<EventType::ShopLeave> {
public:
    ShopLeftEvent() : TypedSignalEvent<EventType::ShopLeave>("ShopLeftEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    bool timedOut{};  // 是否商店超时
};  // 业务通知：离店，由 Condition 在对应时机发布，供被动技能响应。

class RoundStartedEvent final : public TypedSignalEvent<EventType::RoundStart> {
public:
    RoundStartedEvent() : TypedSignalEvent<EventType::RoundStart>("RoundStartedEvent") {}
    RoundId roundId{};  // 所属局
    Players firstPlayer{};  // 本局先手
};  // 业务通知：本局开始，由 Condition 在对应时机发布，供被动技能响应。

class BeforeInitialDealEvent final : public TypedSignalEvent<EventType::InitialDealBefore> {
public:
    BeforeInitialDealEvent() : TypedSignalEvent<EventType::InitialDealBefore>("BeforeInitialDealEvent") {}
    RoundId roundId{};  // 所属局
    int cardsPerPlayer = 7;  // 既有规则起手数量
};  // 业务通知：初始发牌前，由 Condition 在对应时机发布，供被动技能响应。

class InitialCardsDealtEvent final : public TypedSignalEvent<EventType::InitialDealAfter> {
public:
    InitialCardsDealtEvent() : TypedSignalEvent<EventType::InitialDealAfter>("InitialCardsDealtEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体初始发牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickStartedEvent final : public TypedSignalEvent<EventType::TrickStart> {
public:
    TrickStartedEvent() : TypedSignalEvent<EventType::TrickStart>("TrickStartedEvent") {}
    RoundId roundId{};  // 所属局
    TrickId trickId{};  // 所属巡
    Players leader{};  // 领出玩家
};  // 业务通知：本巡开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnStartedEvent final : public TypedSignalEvent<EventType::TurnStart> {
public:
    TurnStartedEvent() : TypedSignalEvent<EventType::TurnStart>("TurnStartedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合开始，由 Condition 在对应时机发布，供被动技能响应。

class TurnAbilityActivationEvent final : public TypedSignalEvent<EventType::PlayActionWindow> {
public:
    TurnAbilityActivationEvent() : TypedSignalEvent<EventType::PlayActionWindow>("TurnAbilityActivationEvent", AbilityType::Active) {}
    TurnId turnId{};  // 所属回合
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：出牌窗口内的主动技能调用，由 Condition 验证请求后只派发给指定技能。

class PlayValidationEvent final : public TypedSignalEvent<EventType::PlayValidate> {
public:
    PlayValidationEvent() : TypedSignalEvent<EventType::PlayValidate>("PlayValidationEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 业务通知：验证选牌及跟牌，由 Condition 在对应时机发布，供被动技能响应。

class CardsPlayedEvent final : public TypedSignalEvent<EventType::PlayCommitted> {
public:
    CardsPlayedEvent() : TypedSignalEvent<EventType::PlayCommitted>("CardsPlayedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 已提交的一手牌
    std::vector<CardId> cards{};  // 实际扑克牌实例
    HandPattern pattern{};  // 最终牌型
};  // 业务通知：成功提交一手牌，由 Condition 在对应时机发布，供被动技能响应。

class BeforePlayRewardEvent final : public TypedSignalEvent<EventType::PlayRewardBefore> {
public:
    BeforePlayRewardEvent() : TypedSignalEvent<EventType::PlayRewardBefore>("BeforePlayRewardEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 原始奖励
};  // 业务通知：计算本手出牌奖励前，由 Condition 在对应时机发布，供被动技能响应。

class PlayRewardGrantedEvent final : public TypedSignalEvent<EventType::PlayRewardAfter> {
public:
    PlayRewardGrantedEvent() : TypedSignalEvent<EventType::PlayRewardAfter>("PlayRewardGrantedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    RewardData reward{};  // 实际到账
};  // 业务通知：本手奖励到账后，由 Condition 在对应时机发布，供被动技能响应。

class PlayerPassedEvent final : public TypedSignalEvent<EventType::PassCommitted> {
public:
    PlayerPassedEvent() : TypedSignalEvent<EventType::PassCommitted>("PlayerPassedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PassReason reason{};  // 主动、超时或掉线
};  // 业务通知：确认不出牌，由 Condition 在对应时机发布，供被动技能响应。

class TurnEndedEvent final : public TypedSignalEvent<EventType::TurnEnd> {
public:
    TurnEndedEvent() : TypedSignalEvent<EventType::TurnEnd>("TurnEndedEvent") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合结算结束，由 Condition 在对应时机发布，供被动技能响应。

class TrickEndedEvent final : public TypedSignalEvent<EventType::TrickEnd> {
public:
    TrickEndedEvent() : TypedSignalEvent<EventType::TrickEnd>("TrickEndedEvent") {}
    TrickId trickId{};  // 所属巡
    Players leader{};  // 牌权获得者
};  // 业务通知：获得牌权并结束本巡，由 Condition 在对应时机发布，供被动技能响应。

class BeforeCardsDrawEvent final : public TypedSignalEvent<EventType::DrawBefore> {
public:
    BeforeCardsDrawEvent() : TypedSignalEvent<EventType::DrawBefore>("BeforeCardsDrawEvent") {}
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    int requestedCount{};  // 请求摸牌数
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：非初始摸牌前，由 Condition 在对应时机发布，供被动技能响应。

class CardsDrawnEvent final : public TypedSignalEvent<EventType::DrawAfter> {
public:
    CardsDrawnEvent() : TypedSignalEvent<EventType::DrawAfter>("CardsDrawnEvent") {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：实际摸牌完成，由 Condition 在对应时机发布，供被动技能响应。

class TrickDrawingCompletedEvent final : public TypedSignalEvent<EventType::TrickDrawsAfter> {
public:
    TrickDrawingCompletedEvent() : TypedSignalEvent<EventType::TrickDrawsAfter>("TrickDrawingCompletedEvent") {}
    TrickId trickId{};  // 所属巡
    int actualTotal{};  // 全体实际摸牌总数
};  // 业务通知：巡末摸牌步骤完成，由 Condition 在对应时机发布，供被动技能响应。

class RoundEndedEvent final : public TypedSignalEvent<EventType::RoundEnd> {
public:
    RoundEndedEvent() : TypedSignalEvent<EventType::RoundEnd>("RoundEndedEvent") {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 已锁定结果
};  // 业务通知：本局结束且最后回合结算完成，由 Condition 在对应时机发布，供被动技能响应。

class BeforeRoundRewardEvent final : public TypedSignalEvent<EventType::RoundRewardBefore> {
public:
    BeforeRoundRewardEvent() : TypedSignalEvent<EventType::RoundRewardBefore>("BeforeRoundRewardEvent") {}
    RoundId roundId{};  // 所属局
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 基础待结算奖励
};  // 业务通知：计算玩家局末奖励前，由 Condition 在对应时机发布，供被动技能响应。

class RoundRewardsGrantedEvent final : public TypedSignalEvent<EventType::RoundRewardAfter> {
public:
    RoundRewardsGrantedEvent() : TypedSignalEvent<EventType::RoundRewardAfter>("RoundRewardsGrantedEvent") {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 实际结算记录
};  // 业务通知：全体局末奖励与扣款完成，由 Condition 在对应时机发布，供被动技能响应。

class GameEndedEvent final : public TypedSignalEvent<EventType::GameEnd> {
public:
    GameEndedEvent() : TypedSignalEvent<EventType::GameEnd>("GameEndedEvent") {}
    std::vector<Players> winners{};  // 并列冠军
    std::uint64_t finalVersion{};  // 最终状态版本
};  // 业务通知：整场结束与冠军确认，由 Condition 在对应时机发布，供被动技能响应。

class PlayingCardsChangedEvent final : public TypedSignalEvent<EventType::CardChanged> {
public:
    PlayingCardsChangedEvent() : TypedSignalEvent<EventType::CardChanged>("PlayingCardsChangedEvent") {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::string reason{};  // 变化原因
};  // 业务通知：技能实际改变牌面或手牌后，由 Condition 在对应时机发布，供被动技能响应。

class AbilityResolvedEvent final : public TypedSignalEvent<EventType::AbilityResolved> {
public:
    AbilityResolvedEvent() : TypedSignalEvent<EventType::AbilityResolved>("AbilityResolvedEvent") {}
    AbilityRef ability{};  // 目标技能
    EventId triggerEventId{};  // 触发事件
};  // 业务通知：技能效果成功结算后，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardEquippedEvent final : public TypedSignalEvent<EventType::FunctionCardEquipped> {
public:
    FunctionCardEquippedEvent() : TypedSignalEvent<EventType::FunctionCardEquipped>("FunctionCardEquippedEvent") {}
    CardRef card{};  // 功能卡实例
    std::size_t slot{};  // 实际槽位
};  // 业务通知：功能卡进入槽位，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardRemovedEvent final : public TypedSignalEvent<EventType::FunctionCardRemoved> {
public:
    FunctionCardRemovedEvent() : TypedSignalEvent<EventType::FunctionCardRemoved>("FunctionCardRemovedEvent") {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 业务通知：功能卡离开槽位，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotLockedEvent final : public TypedSignalEvent<EventType::ShopSlotLocked> {
public:
    ShopSlotLockedEvent() : TypedSignalEvent<EventType::ShopSlotLocked>("ShopSlotLockedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位锁定成功，由 Condition 在对应时机发布，供被动技能响应。

class ShopSlotUnlockedEvent final : public TypedSignalEvent<EventType::ShopSlotUnlocked> {
public:
    ShopSlotUnlockedEvent() : TypedSignalEvent<EventType::ShopSlotUnlocked>("ShopSlotUnlockedEvent") {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位解锁成功，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackOpenedEvent final : public TypedSignalEvent<EventType::BoosterPackOpened> {
public:
    BoosterPackOpenedEvent() : TypedSignalEvent<EventType::BoosterPackOpened>("BoosterPackOpenedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：卡包已付费打开，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackChoiceCompletedEvent final : public TypedSignalEvent<EventType::BoosterPackChoiceCompleted> {
public:
    BoosterPackChoiceCompletedEvent() : TypedSignalEvent<EventType::BoosterPackChoiceCompleted>("BoosterPackChoiceCompletedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    std::vector<ChoiceOptionId> options{};  // 实际选项
};  // 业务通知：卡包候选选取完成，由 Condition 在对应时机发布，供被动技能响应。

class BoosterPackSkippedEvent final : public TypedSignalEvent<EventType::BoosterPackSkipped> {
public:
    BoosterPackSkippedEvent() : TypedSignalEvent<EventType::BoosterPackSkipped>("BoosterPackSkippedEvent") {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
};  // 业务通知：卡包已放弃，由 Condition 在对应时机发布，供被动技能响应。

class ExpansionCardUsedEvent final : public TypedSignalEvent<EventType::ExpansionCardUsed> {
public:
    ExpansionCardUsedEvent() : TypedSignalEvent<EventType::ExpansionCardUsed>("ExpansionCardUsedEvent") {}
    Players player{};  // 相关玩家
    DefinitionId expansionId{};  // 拓展牌定义
    TargetSpec selection{};  // 目标与方案
};  // 业务通知：拓展牌获得即使用完成，由 Condition 在对应时机发布，供被动技能响应。

class HandPatternUpgradedEvent final : public TypedSignalEvent<EventType::HandPatternUpgraded> {
public:
    HandPatternUpgradedEvent() : TypedSignalEvent<EventType::HandPatternUpgraded>("HandPatternUpgradedEvent") {}
    Players player{};  // 相关玩家
    HandPattern pattern{};  // 牌型
    int beforeLevel{};  // 原等级
    int afterLevel{};  // 新等级
};  // 业务通知：牌型加强完成，由 Condition 在对应时机发布，供被动技能响应。

class FacilityPurchasedEvent final : public TypedSignalEvent<EventType::FacilityPurchased> {
public:
    FacilityPurchasedEvent() : TypedSignalEvent<EventType::FacilityPurchased>("FacilityPurchasedEvent") {}
    Players player{};  // 相关玩家
    DefinitionId facilityId{};  // 设施定义
};  // 业务通知：设施购买生效，由 Condition 在对应时机发布，供被动技能响应。

class CoinsChangedEvent final : public TypedSignalEvent<EventType::CoinsChanged> {
public:
    CoinsChangedEvent() : TypedSignalEvent<EventType::CoinsChanged>("CoinsChangedEvent") {}
    Players player{};  // 相关玩家
    CoinValue before{};  // 变化前
    CoinValue after{};  // 变化后
    std::string reason{};  // 收入或支出原因
};  // 业务通知：金币余额实际变化，由 Condition 在对应时机发布，供被动技能响应。

class ScoreChangedEvent final : public TypedSignalEvent<EventType::ScoreChanged> {
public:
    ScoreChangedEvent() : TypedSignalEvent<EventType::ScoreChanged>("ScoreChangedEvent") {}
    Players player{};  // 相关玩家
    ScoreValue before{};  // 变化前
    ScoreValue after{};  // 变化后
};  // 业务通知：累计分数实际变化，由 Condition 在对应时机发布，供被动技能响应。

class PlayerStatusChangedEvent final : public TypedSignalEvent<EventType::PlayerStatusChanged> {
public:
    PlayerStatusChangedEvent() : TypedSignalEvent<EventType::PlayerStatusChanged>("PlayerStatusChangedEvent") {}
    Players player{};  // 相关玩家
    StatusId statusId{};  // 状态实例
    std::string reason{};  // 增加、移除或层数变化
};  // 业务通知：玩家状态实际变化，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAddedEvent final : public TypedSignalEvent<EventType::AbilityAdded> {
public:
    AbilityAddedEvent() : TypedSignalEvent<EventType::AbilityAdded>("AbilityAddedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能添加完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityRemovedEvent final : public TypedSignalEvent<EventType::AbilityRemoved> {
public:
    AbilityRemovedEvent() : TypedSignalEvent<EventType::AbilityRemoved>("AbilityRemovedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能逻辑移除完成，由 Condition 在对应时机发布，供被动技能响应。

class AbilityAbolishedEvent final : public TypedSignalEvent<EventType::AbilityAbolished> {
public:
    AbilityAbolishedEvent() : TypedSignalEvent<EventType::AbilityAbolished>("AbilityAbolishedEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能被永久废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityUsabilityChangedEvent final : public TypedSignalEvent<EventType::AbilityUsabilityChanged> {
public:
    AbilityUsabilityChangedEvent() : TypedSignalEvent<EventType::AbilityUsabilityChanged>("AbilityUsabilityChangedEvent") {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 当前开关
};  // 业务通知：技能临时可用开关变化，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceOpenedEvent final : public TypedSignalEvent<EventType::ChoiceOpened> {
public:
    ChoiceOpenedEvent() : TypedSignalEvent<EventType::ChoiceOpened>("ChoiceOpenedEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    ChoiceKind choiceKind{};  // 交互类型
};  // 业务通知：交互选择窗口建立，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceResolvedEvent final : public TypedSignalEvent<EventType::ChoiceResolved> {
public:
    ChoiceResolvedEvent() : TypedSignalEvent<EventType::ChoiceResolved>("ChoiceResolvedEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选择
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 业务通知：交互选择处理完成，由 Condition 在对应时机发布，供被动技能响应。

class ChoiceExpiredEvent final : public TypedSignalEvent<EventType::ChoiceExpired> {
public:
    ChoiceExpiredEvent() : TypedSignalEvent<EventType::ChoiceExpired>("ChoiceExpiredEvent") {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：交互期限已到并完成默认处理，由 Condition 在对应时机发布，供被动技能响应。

class RoundWinnerLockedEvent final : public TypedSignalEvent<EventType::RoundWinnerLocked> {
public:
    RoundWinnerLockedEvent() : TypedSignalEvent<EventType::RoundWinnerLocked>("RoundWinnerLockedEvent") {}
    RoundId roundId{};  // 所属局
    Players winner{};  // 赢家
    RoundResultId resultId{};  // 冻结结果
};  // 业务通知：出空时锁定赢家与手牌快照，由 Condition 在对应时机发布，供被动技能响应。

class DrawPileExhaustedEvent final : public TypedSignalEvent<EventType::DrawPileExhausted> {
public:
    DrawPileExhaustedEvent() : TypedSignalEvent<EventType::DrawPileExhausted>("DrawPileExhaustedEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：牌堆由非空变为空，由 Condition 在对应时机发布，供被动技能响应。

class AllShopsClosedEvent final : public TypedSignalEvent<EventType::AllShopsClosed> {
public:
    AllShopsClosedEvent() : TypedSignalEvent<EventType::AllShopsClosed>("AllShopsClosedEvent") {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体商店会话关闭且交易结清，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardUsabilityChangedEvent final : public TypedSignalEvent<EventType::FunctionCardUsabilityChanged> {
public:
    FunctionCardUsabilityChangedEvent() : TypedSignalEvent<EventType::FunctionCardUsabilityChanged>("FunctionCardUsabilityChangedEvent") {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 当前开关
};  // 业务通知：整张功能卡临时开关变化，由 Condition 在对应时机发布，供被动技能响应。

class FunctionCardAbolishedEvent final : public TypedSignalEvent<EventType::FunctionCardAbolished> {
public:
    FunctionCardAbolishedEvent() : TypedSignalEvent<EventType::FunctionCardAbolished>("FunctionCardAbolishedEvent") {}
    CardRef card{};  // 功能卡实例
};  // 业务通知：整张功能卡被废除，由 Condition 在对应时机发布，供被动技能响应。

class AbilityStateChangedEvent final : public TypedSignalEvent<EventType::AbilityStateChanged> {
public:
    AbilityStateChangedEvent() : TypedSignalEvent<EventType::AbilityStateChanged>("AbilityStateChangedEvent") {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 已定义的计数状态
    int before{};  // 变化前
    int after{};  // 变化后
};  // 业务通知：技能成长或蓄力状态变化，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectScheduledEvent final : public TypedSignalEvent<EventType::DelayedEffectScheduled> {
public:
    DelayedEffectScheduledEvent() : TypedSignalEvent<EventType::DelayedEffectScheduled>("DelayedEffectScheduledEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟效果标记建立，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectTriggeredEvent final : public TypedSignalEvent<EventType::DelayedEffectTriggered> {
public:
    DelayedEffectTriggeredEvent() : TypedSignalEvent<EventType::DelayedEffectTriggered>("DelayedEffectTriggeredEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记实际执行完成，由 Condition 在对应时机发布，供被动技能响应。

class DelayedEffectExpiredEvent final : public TypedSignalEvent<EventType::DelayedEffectExpired> {
public:
    DelayedEffectExpiredEvent() : TypedSignalEvent<EventType::DelayedEffectExpired>("DelayedEffectExpiredEvent") {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记结束或失效，由 Condition 在对应时机发布，供被动技能响应。

class CoatingAppliedEvent final : public TypedSignalEvent<EventType::CoatingApplied> {
public:
    CoatingAppliedEvent() : TypedSignalEvent<EventType::CoatingApplied>("CoatingAppliedEvent") {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 业务通知：镀层施加完成，由 Condition 在对应时机发布，供被动技能响应。

class CardVisibilityChangedEvent final : public TypedSignalEvent<EventType::CardVisibilityChanged> {
public:
    CardVisibilityChangedEvent() : TypedSignalEvent<EventType::CardVisibilityChanged>("CardVisibilityChangedEvent") {}
    Players viewer{};  // 获得信息的玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 业务通知：查看权限或公开信息变化，由 Condition 在对应时机发布，供被动技能响应。

// 效果请求（38 个）：表达需要执行的效果。

class AddScoreEvent final : public EffectEvent {
public:
    AddScoreEvent() : EffectEvent("AddScoreEvent") {}
    Players target{};  // 效果目标玩家
    ScoreValue amount{};  // 增加数量
};  // 效果请求：增加实际分数；由 Condition 校验并执行。

class AddCoinsEvent final : public EffectEvent {
public:
    AddCoinsEvent() : EffectEvent("AddCoinsEvent") {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 增加数量
};  // 效果请求：增加实际金币；由 Condition 校验并执行。

class SpendCoinsEvent final : public EffectEvent {
public:
    SpendCoinsEvent() : EffectEvent("SpendCoinsEvent") {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 扣款数量
    std::string reason{};  // 扣款用途
};  // 效果请求：扣除实际金币；由 Condition 校验并执行。

class AddPendingScoreEvent final : public EffectEvent {
public:
    AddPendingScoreEvent() : EffectEvent("AddPendingScoreEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    ScoreValue delta{};  // 有符号增减值
};  // 效果请求：修正待结算分数；由 Condition 校验并执行。

class MultiplyPendingScoreEvent final : public EffectEvent {
public:
    MultiplyPendingScoreEvent() : EffectEvent("MultiplyPendingScoreEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    int numerator = 1;  // 倍率分子
    int denominator = 1;  // 倍率分母
    RoundingRule rounding{};  // 取整规则
};  // 效果请求：缩放待结算分数；由 Condition 校验并执行。

class AddPendingCoinsEvent final : public EffectEvent {
public:
    AddPendingCoinsEvent() : EffectEvent("AddPendingCoinsEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue delta{};  // 有符号增减值
    RewardComponent component{};  // 基础或额外奖励
    CoinValue minimum{};  // 该部分最低金额
};  // 效果请求：修正待结算金币；由 Condition 校验并执行。

class ReduceShopCostEvent final : public EffectEvent {
public:
    ReduceShopCostEvent() : EffectEvent("ReduceShopCostEvent") {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue amount{};  // 减费数
    CoinValue minimum{};  // 卡牌规定的最低价
};  // 效果请求：降低本次商店报价；由 Condition 校验并执行。

class GrantFreeRefreshEvent final : public EffectEvent {
public:
    GrantFreeRefreshEvent() : EffectEvent("GrantFreeRefreshEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    int uses{};  // 额度数量
};  // 效果请求：增加本店免费刷新额度；由 Condition 校验并执行。

class RefreshShopEvent final : public EffectEvent {
public:
    RefreshShopEvent() : EffectEvent("RefreshShopEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    RefreshMode mode{};  // 付费或免费
};  // 效果请求：执行一次货架刷新；由 Condition 校验并执行，仍检查本店刷新总上限，免费不推进付费价格序号。

class OpenBoosterPackEvent final : public EffectEvent {
public:
    OpenBoosterPackEvent() : EffectEvent("OpenBoosterPackEvent") {}
    Players target{};  // 开包玩家
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 卡包实例
    SettlementId settlementId{};  // 已验证的购买或授予事务
};  // 效果请求：执行已获准的开包；由 Condition 校验并执行，与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择。

class LockShopSlotEvent final : public EffectEvent {
public:
    LockShopSlotEvent() : EffectEvent("LockShopSlotEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位锁定；由 Condition 校验并执行。

class UnlockShopSlotEvent final : public EffectEvent {
public:
    UnlockShopSlotEvent() : EffectEvent("UnlockShopSlotEvent") {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位解锁；由 Condition 校验并执行。

class GrantFunctionCardEvent final : public EffectEvent {
public:
    GrantFunctionCardEvent() : EffectEvent("GrantFunctionCardEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId definitionId{};  // 牌种
    CoinValue purchaseCost{};  // 实例出售成本
};  // 效果请求：生成并装备功能卡；由 Condition 校验并执行，检查空槽；免费生成成本为零。

class RemoveFunctionCardEvent final : public EffectEvent {
public:
    RemoveFunctionCardEvent() : EffectEvent("RemoveFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 效果请求：移除功能卡；由 Condition 校验并执行。

class ReplaceFunctionCardEvent final : public EffectEvent {
public:
    ReplaceFunctionCardEvent() : EffectEvent("ReplaceFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
    DefinitionId newDefinitionId{};  // 新牌种
    bool preservePurchaseCost = true;  // 是否沿用出售成本
};  // 效果请求：替换功能卡；由 Condition 校验并执行。

class AddAbilityEvent final : public EffectEvent {
public:
    AddAbilityEvent() : EffectEvent("AddAbilityEvent") {}
    CardRef card{};  // 功能卡实例
    DefinitionId abilityDefinitionId{};  // 技能定义
};  // 效果请求：给功能卡添加技能；由 Condition 校验并执行。

class RemoveAbilityEvent final : public EffectEvent {
public:
    RemoveAbilityEvent() : EffectEvent("RemoveAbilityEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：逻辑删除技能；由 Condition 校验并执行。

class AbolishAbilityEvent final : public EffectEvent {
public:
    AbolishAbilityEvent() : EffectEvent("AbolishAbilityEvent") {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：永久废除技能；由 Condition 校验并执行。

class SetAbilityUsableEvent final : public EffectEvent {
public:
    SetAbilityUsableEvent() : EffectEvent("SetAbilityUsableEvent") {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 新开关
};  // 效果请求：设置技能临时开关；由 Condition 校验并执行。

class SetFunctionCardUsableEvent final : public EffectEvent {
public:
    SetFunctionCardUsableEvent() : EffectEvent("SetFunctionCardUsableEvent") {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 新开关
};  // 效果请求：设置整牌临时开关；由 Condition 校验并执行。

class AbolishFunctionCardEvent final : public EffectEvent {
public:
    AbolishFunctionCardEvent() : EffectEvent("AbolishFunctionCardEvent") {}
    CardRef card{};  // 功能卡实例
};  // 效果请求：永久废除整张牌；由 Condition 校验并执行。

class ApplyCoatingEvent final : public EffectEvent {
public:
    ApplyCoatingEvent() : EffectEvent("ApplyCoatingEvent") {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 效果请求：给功能卡施加镀层；由 Condition 校验并执行。

class DrawCardsEvent final : public EffectEvent {
public:
    DrawCardsEvent() : EffectEvent("DrawCardsEvent") {}
    Players target{};  // 效果目标玩家
    int count{};  // 数量
    DrawReason reason{};  // 摸牌原因
};  // 效果请求：按规则执行非初始摸牌；由 Condition 校验并执行，锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理。

class SwapCardsWithDeckEvent final : public EffectEvent {
public:
    SwapCardsWithDeckEvent() : EffectEvent("SwapCardsWithDeckEvent") {}
    Players target{};  // 效果目标玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：将指定手牌换到牌堆底并等量摸顶；由 Condition 校验并执行，保持原牌身份与公共牌组总数。

class ChangeCardRankEvent final : public EffectEvent {
public:
    ChangeCardRankEvent() : EffectEvent("ChangeCardRankEvent") {}
    CardId cardId{};  // 扑克牌实例
    int rank{};  // 目标点数
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变普通牌点数；由 Condition 校验并执行。

class ChangeCardSuitEvent final : public EffectEvent {
public:
    ChangeCardSuitEvent() : EffectEvent("ChangeCardSuitEvent") {}
    CardId cardId{};  // 扑克牌实例
    Suit suit{};  // 目标花色
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变花色；由 Condition 校验并执行。

class MakeCardJokerEvent final : public EffectEvent {
public:
    MakeCardJokerEvent() : EffectEvent("MakeCardJokerEvent") {}
    CardId cardId{};  // 扑克牌实例
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：将指定牌临时变为王；由 Condition 校验并执行。

class RevealCardsEvent final : public EffectEvent {
public:
    RevealCardsEvent() : EffectEvent("RevealCardsEvent") {}
    Players viewer{};  // 查看者
    Players target{};  // 被查看玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：向指定玩家展示合法信息；由 Condition 校验并执行，只向有权限的客户端同步牌面。

class ApplyPlayerStatusEvent final : public EffectEvent {
public:
    ApplyPlayerStatusEvent() : EffectEvent("ApplyPlayerStatusEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId statusDefinitionId{};  // 状态定义
    int stacks = 1;  // 层数
    Lifetime lifetime{};  // 期限
};  // 效果请求：施加玩家状态；由 Condition 校验并执行。

class RemovePlayerStatusEvent final : public EffectEvent {
public:
    RemovePlayerStatusEvent() : EffectEvent("RemovePlayerStatusEvent") {}
    Players target{};  // 效果目标玩家
    StatusId statusId{};  // 状态实例
};  // 效果请求：移除玩家状态；由 Condition 校验并执行。

class UpgradeHandPatternEvent final : public EffectEvent {
public:
    UpgradeHandPatternEvent() : EffectEvent("UpgradeHandPatternEvent") {}
    Players target{};  // 效果目标玩家
    HandPattern pattern{};  // 牌型
    int levels = 1;  // 加强级数
};  // 效果请求：执行牌型加强；由 Condition 校验并执行。

class PurchaseFacilityEvent final : public EffectEvent {
public:
    PurchaseFacilityEvent() : EffectEvent("PurchaseFacilityEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId facilityId{};  // 设施
};  // 效果请求：执行设施购买与生效；由 Condition 校验并执行。

class UseExpansionCardEvent final : public EffectEvent {
public:
    UseExpansionCardEvent() : EffectEvent("UseExpansionCardEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId expansionId{};  // 拓展牌
    TargetSpec selection{};  // 目标与方案
};  // 效果请求：执行获得即使用的拓展牌；由 Condition 校验并执行。

class AddDelayedEffectEvent final : public EffectEvent {
public:
    AddDelayedEffectEvent() : EffectEvent("AddDelayedEffectEvent") {}
    Players target{};  // 效果目标玩家
    DefinitionId effectDefinitionId{};  // 延迟效果定义
    EventType triggerType{};  // 未来触发的通知类型
    TargetSpec selection{};  // 目标与方案
    Lifetime lifetime{};  // 有效期
};  // 效果请求：建立延迟效果标记；由 Condition 校验并执行。

class OpenChoiceEvent final : public EffectEvent {
public:
    OpenChoiceEvent() : EffectEvent("OpenChoiceEvent") {}
    Players target{};  // 效果目标玩家
    ChoiceKind choiceKind{};  // 选择类型
    std::vector<ChoiceOptionId> options{};  // 合法选项
    std::uint32_t timeoutMs{};  // 超时长度
    bool optional{};  // 是否可放弃
};  // 效果请求：建立选择窗口并挂起相关流程；由 Condition 校验并执行。

class ResolveChoiceEvent final : public EffectEvent {
public:
    ResolveChoiceEvent() : EffectEvent("ResolveChoiceEvent") {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选项
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 效果请求：执行选择或默认方案；由 Condition 校验并执行。

class ChangeAbilityCounterEvent final : public EffectEvent {
public:
    ChangeAbilityCounterEvent() : EffectEvent("ChangeAbilityCounterEvent") {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 计数项
    CounterOperation operation{};  // 加值或设值
    int value{};  // 操作值
};  // 效果请求：改变已定义的技能成长或蓄力计数；由 Condition 校验并执行。

class EventBatch final : public EffectEvent {
public:
    EventBatch() : EffectEvent("EventBatch") {}
    std::vector<EventPtr> events{};  // 有序子效果
};  // 效果请求：有序原子组合多个效果；由 Condition 校验并执行，只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待。

// 控制事件（10 个）：连接、计时器和服务端反馈。

class PlayerJoinedControl final : public ControlEvent {
public:
    PlayerJoinedControl() : ControlEvent("PlayerJoinedControl") {}
    Players player{};  // 相关玩家
};  // 控制事件：玩家加入房间，由 Condition 更新房间流程，满足满员条件后自动开局。

class PlayerDisconnectedControl final : public ControlEvent {
public:
    PlayerDisconnectedControl() : ControlEvent("PlayerDisconnectedControl") {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 连接代次
};  // 控制事件：连接断开，保留原座位与资源，按对应超时规则继续处理。

class PlayerReconnectedControl final : public ControlEvent {
public:
    PlayerReconnectedControl() : ControlEvent("PlayerReconnectedControl") {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 新连接代次
};  // 控制事件：原玩家重连，恢复原座位并同步有权查看的当前状态。

class ShoppingDeadlineControl final : public ControlEvent {
public:
    ShoppingDeadlineControl() : ControlEvent("ShoppingDeadlineControl") {}
    ShopId shopId{};  // 商店会话
    std::uint64_t expectedVersion{};  // 对应商店版本
};  // 控制事件：商店计时到期，由 Condition 检查商店会话与版本后执行结束流程。

class TurnDeadlineControl final : public ControlEvent {
public:
    TurnDeadlineControl() : ControlEvent("TurnDeadlineControl") {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::uint64_t expectedVersion{};  // 对应回合版本
};  // 控制事件：出牌计时到期，由 Condition 检查回合与版本后执行既定超时操作。

class ChoiceDeadlineControl final : public ControlEvent {
public:
    ChoiceDeadlineControl() : ControlEvent("ChoiceDeadlineControl") {}
    ChoiceId choiceId{};  // 待处理交互
    std::uint64_t expectedVersion{};  // 对应交互版本
};  // 控制事件：选择计时到期，由 Condition 检查交互与版本后执行默认方案。

class RequestRejectedControl final : public ControlEvent {
public:
    RequestRejectedControl() : ControlEvent("RequestRejectedControl") {}
    RequestId requestId{};  // 被拒绝请求
    std::string reason{};  // 拒绝原因
    std::uint64_t currentVersion{};  // 当前版本
};  // 控制反馈：操作请求未获接受，只反馈原因，不视为成功行为或成功类收益来源。

class StateSnapshotReadyControl final : public ControlEvent {
public:
    StateSnapshotReadyControl() : ControlEvent("StateSnapshotReadyControl") {}
    Players player{};  // 相关玩家
    SnapshotId snapshotId{};  // 按权限过滤的快照
};  // 控制反馈：同步快照已经准备，只向指定玩家发送其有权查看的信息。

class PlayOptionsReadyControl final : public ControlEvent {
public:
    PlayOptionsReadyControl() : ControlEvent("PlayOptionsReadyControl") {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    std::vector<ChoiceOptionId> options{};  // 合法方案
};  // 查询反馈：合法出牌或赖子方案已经计算，表示预览结果，尚未成功出牌。

class ShopQuoteReadyControl final : public ControlEvent {
public:
    ShopQuoteReadyControl() : ControlEvent("ShopQuoteReadyControl") {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    ShopData quote{};  // 当前报价
};  // 查询反馈：商店报价已经计算，本身不扣款或消耗免费刷新额度。

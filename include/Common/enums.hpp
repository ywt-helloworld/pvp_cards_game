#pragma once

/// @file enums.hpp
/// @brief 公共枚举及其取值；包含全部事件类型编号。

#include <cstddef>
#include <cstdint>

namespace cardgame {

/// 房间中的玩家座位编号；实际参与人数和行动顺序由房间规则维护。
enum class Players : std::uint8_t {
    P1,  // 1 号座位
    P2,  // 2 号座位
    P3,  // 3 号座位
    P4  // 4 号座位
};

/// 唯一的操作类型编号；0～135 对应 136 个具体事件，新增类型应追加在 Count 前。
enum class EventType : std::size_t {
    GameStart = 0,  // 整场开始；参数见 GameStartedEvent。
    ShopEnter = 1,  // 进入局前商店；参数见 ShopEnteredEvent。
    ShopActionWindow = 2,  // 商店窗口内的主动技能调用；参数见 ShopAbilityActivationEvent。
    ShopRefreshQuote = 3,  // 刷新报价前；参数见 BeforeShopRefreshEvent。
    ShopRefreshAfter = 4,  // 刷新成功后；参数见 ShopRefreshedEvent。
    ShopBuyQuote = 5,  // 购买报价前；参数见 BeforeShopPurchaseEvent。
    ShopBuyAfter = 6,  // 购买交付成功后；参数见 ShopPurchasedEvent。
    ShopSellQuote = 7,  // 出售报价前；参数见 BeforeShopSaleEvent。
    ShopSellAfter = 8,  // 出售成功后；参数见 ShopSoldEvent。
    ShopLeave = 9,  // 离店；参数见 ShopLeftEvent。
    RoundStart = 10,  // 本局开始；参数见 RoundStartedEvent。
    InitialDealBefore = 11,  // 初始发牌前；参数见 BeforeInitialDealEvent。
    InitialDealAfter = 12,  // 全体初始发牌完成；参数见 InitialCardsDealtEvent。
    TrickStart = 13,  // 本巡开始；参数见 TrickStartedEvent。
    TurnStart = 14,  // 当前回合开始；参数见 TurnStartedEvent。
    PlayActionWindow = 15,  // 出牌窗口内的主动技能调用；参数见 TurnAbilityActivationEvent。
    PlayValidate = 16,  // 验证选牌及跟牌；参数见 PlayValidationEvent。
    PlayCommitted = 17,  // 成功提交一手牌；参数见 CardsPlayedEvent。
    PlayRewardBefore = 18,  // 计算本手出牌奖励前；参数见 BeforePlayRewardEvent。
    PlayRewardAfter = 19,  // 本手奖励到账后；参数见 PlayRewardGrantedEvent。
    PassCommitted = 20,  // 确认不出牌；参数见 PlayerPassedEvent。
    TurnEnd = 21,  // 当前回合结算结束；参数见 TurnEndedEvent。
    TrickEnd = 22,  // 获得牌权并结束本巡；参数见 TrickEndedEvent。
    DrawBefore = 23,  // 非初始摸牌前；参数见 BeforeCardsDrawEvent。
    DrawAfter = 24,  // 实际摸牌完成；参数见 CardsDrawnEvent。
    TrickDrawsAfter = 25,  // 巡末摸牌步骤完成；参数见 TrickDrawingCompletedEvent。
    RoundEnd = 26,  // 本局结束且最后回合结算完成；参数见 RoundEndedEvent。
    RoundRewardBefore = 27,  // 计算玩家局末奖励前；参数见 BeforeRoundRewardEvent。
    RoundRewardAfter = 28,  // 全体局末奖励与扣款完成；参数见 RoundRewardsGrantedEvent。
    GameEnd = 29,  // 整场结束与冠军确认；参数见 GameEndedEvent。
    CardChanged = 30,  // 技能实际改变牌面或手牌后；参数见 PlayingCardsChangedEvent。
    AbilityResolved = 31,  // 技能效果成功结算后；参数见 AbilityResolvedEvent。
    FunctionCardEquipped = 32,  // 功能卡进入槽位；参数见 FunctionCardEquippedEvent。
    FunctionCardRemoved = 33,  // 功能卡离开槽位；参数见 FunctionCardRemovedEvent。
    ShopSlotLocked = 34,  // 货位锁定成功；参数见 ShopSlotLockedEvent。
    ShopSlotUnlocked = 35,  // 货位解锁成功；参数见 ShopSlotUnlockedEvent。
    BoosterPackOpened = 36,  // 卡包已付费打开；参数见 BoosterPackOpenedEvent。
    BoosterPackChoiceCompleted = 37,  // 卡包候选选取完成；参数见 BoosterPackChoiceCompletedEvent。
    BoosterPackSkipped = 38,  // 卡包已放弃；参数见 BoosterPackSkippedEvent。
    ExpansionCardUsed = 39,  // 拓展牌获得即使用完成；参数见 ExpansionCardUsedEvent。
    HandPatternUpgraded = 40,  // 牌型加强完成；参数见 HandPatternUpgradedEvent。
    FacilityPurchased = 41,  // 设施购买生效；参数见 FacilityPurchasedEvent。
    CoinsChanged = 42,  // 金币余额实际变化；参数见 CoinsChangedEvent。
    ScoreChanged = 43,  // 累计分数实际变化；参数见 ScoreChangedEvent。
    PlayerStatusChanged = 44,  // 玩家状态实际变化；参数见 PlayerStatusChangedEvent。
    AbilityAdded = 45,  // 技能添加完成；参数见 AbilityAddedEvent。
    AbilityRemoved = 46,  // 技能逻辑移除完成；参数见 AbilityRemovedEvent。
    AbilityAbolished = 47,  // 技能被永久废除；参数见 AbilityAbolishedEvent。
    AbilityUsabilityChanged = 48,  // 技能临时可用开关变化；参数见 AbilityUsabilityChangedEvent。
    ChoiceOpened = 49,  // 交互选择窗口建立；参数见 ChoiceOpenedEvent。
    ChoiceResolved = 50,  // 交互选择处理完成；参数见 ChoiceResolvedEvent。
    ChoiceExpired = 51,  // 交互期限已到并完成默认处理；参数见 ChoiceExpiredEvent。
    RoundWinnerLocked = 52,  // 出空时锁定赢家与手牌快照；参数见 RoundWinnerLockedEvent。
    DrawPileExhausted = 53,  // 牌堆由非空变为空；参数见 DrawPileExhaustedEvent。
    AllShopsClosed = 54,  // 全体商店会话关闭且交易结清；参数见 AllShopsClosedEvent。
    FunctionCardUsabilityChanged = 55,  // 整张功能卡临时开关变化；参数见 FunctionCardUsabilityChangedEvent。
    FunctionCardAbolished = 56,  // 整张功能卡被废除；参数见 FunctionCardAbolishedEvent。
    AbilityStateChanged = 57,  // 技能成长或蓄力状态变化；参数见 AbilityStateChangedEvent。
    DelayedEffectScheduled = 58,  // 延迟效果标记建立；参数见 DelayedEffectScheduledEvent。
    DelayedEffectTriggered = 59,  // 延迟标记实际执行完成；参数见 DelayedEffectTriggeredEvent。
    DelayedEffectExpired = 60,  // 延迟标记结束或失效；参数见 DelayedEffectExpiredEvent。
    CoatingApplied = 61,  // 镀层施加完成；参数见 CoatingAppliedEvent。
    CardVisibilityChanged = 62,  // 查看权限或公开信息变化；参数见 CardVisibilityChangedEvent。
    BuyShopItemRequest = 63,  // 购买普通货架商品；参数见 BuyShopItemRequest。
    BuyBoosterPackRequest = 64,  // 购买并立即打开卡包；参数见 BuyBoosterPackRequest。
    SellFunctionCardRequest = 65,  // 出售功能卡；参数见 SellFunctionCardRequest。
    RefreshShopRequest = 66,  // 刷新普通货架；参数见 RefreshShopRequest。
    LockShopSlotRequest = 67,  // 锁定货位；参数见 LockShopSlotRequest。
    UnlockShopSlotRequest = 68,  // 解锁货位；参数见 UnlockShopSlotRequest。
    ChooseBoosterCardRequest = 69,  // 选择卡包候选；参数见 ChooseBoosterCardRequest。
    SkipBoosterPackRequest = 70,  // 放弃已打开卡包；参数见 SkipBoosterPackRequest。
    UpgradeHandPatternRequest = 71,  // 付费加强牌型；参数见 UpgradeHandPatternRequest。
    BuyFacilityRequest = 72,  // 购买设施；参数见 BuyFacilityRequest。
    FinishShoppingRequest = 73,  // 完成本次购物；参数见 FinishShoppingRequest。
    PlayCardsRequest = 74,  // 确认出牌；参数见 PlayCardsRequest。
    PassRequest = 75,  // 主动不出牌；参数见 PassRequest。
    ActivateAbilityRequest = 76,  // 发动主动技能；参数见 ActivateAbilityRequest。
    SubmitChoiceRequest = 77,  // 提交一般目标或方案；参数见 SubmitChoiceRequest。
    ResolveWildcardRequest = 78,  // 确认赖子指定；参数见 ResolveWildcardRequest。
    ContinueGameRequest = 79,  // 请求加赛或无尽模式；参数见 ContinueGameRequest。
    StartNewGameRequest = 80,  // 请求开始新一场；参数见 StartNewGameRequest。
    StateSyncRequest = 81,  // 请求状态同步；参数见 StateSyncRequest。
    PreviewPlayRequest = 82,  // 查询选牌的合法方案；参数见 PreviewPlayRequest。
    QuoteShopOperationRequest = 83,  // 查询商店操作报价；参数见 QuoteShopOperationRequest。
    AddScore = 84,  // 增加实际分数；参数见 AddScoreEvent。
    AddCoins = 85,  // 增加实际金币；参数见 AddCoinsEvent。
    SpendCoins = 86,  // 扣除实际金币；参数见 SpendCoinsEvent。
    AddPendingScore = 87,  // 修正待结算分数；参数见 AddPendingScoreEvent。
    MultiplyPendingScore = 88,  // 缩放待结算分数；参数见 MultiplyPendingScoreEvent。
    AddPendingCoins = 89,  // 修正待结算金币；参数见 AddPendingCoinsEvent。
    ReduceShopCost = 90,  // 降低本次商店报价；参数见 ReduceShopCostEvent。
    GrantFreeRefresh = 91,  // 增加本店免费刷新额度；参数见 GrantFreeRefreshEvent。
    RefreshShop = 92,  // 执行一次货架刷新；参数见 RefreshShopEvent。
    OpenBoosterPack = 93,  // 执行已获准的开包；参数见 OpenBoosterPackEvent。
    LockShopSlot = 94,  // 执行货位锁定；参数见 LockShopSlotEvent。
    UnlockShopSlot = 95,  // 执行货位解锁；参数见 UnlockShopSlotEvent。
    GrantFunctionCard = 96,  // 生成并装备功能卡；参数见 GrantFunctionCardEvent。
    RemoveFunctionCard = 97,  // 移除功能卡；参数见 RemoveFunctionCardEvent。
    ReplaceFunctionCard = 98,  // 替换功能卡；参数见 ReplaceFunctionCardEvent。
    AddAbility = 99,  // 给功能卡添加技能；参数见 AddAbilityEvent。
    RemoveAbility = 100,  // 逻辑删除技能；参数见 RemoveAbilityEvent。
    AbolishAbility = 101,  // 永久废除技能；参数见 AbolishAbilityEvent。
    SetAbilityUsable = 102,  // 设置技能临时开关；参数见 SetAbilityUsableEvent。
    SetFunctionCardUsable = 103,  // 设置整牌临时开关；参数见 SetFunctionCardUsableEvent。
    AbolishFunctionCard = 104,  // 永久废除整张牌；参数见 AbolishFunctionCardEvent。
    ApplyCoating = 105,  // 给功能卡施加镀层；参数见 ApplyCoatingEvent。
    DrawCards = 106,  // 按规则执行非初始摸牌；参数见 DrawCardsEvent。
    SwapCardsWithDeck = 107,  // 将指定手牌换到牌堆底并等量摸顶；参数见 SwapCardsWithDeckEvent。
    ChangeCardRank = 108,  // 临时改变普通牌点数；参数见 ChangeCardRankEvent。
    ChangeCardSuit = 109,  // 临时改变花色；参数见 ChangeCardSuitEvent。
    MakeCardJoker = 110,  // 将指定牌临时变为王；参数见 MakeCardJokerEvent。
    RevealCards = 111,  // 向指定玩家展示合法信息；参数见 RevealCardsEvent。
    ApplyPlayerStatus = 112,  // 施加玩家状态；参数见 ApplyPlayerStatusEvent。
    RemovePlayerStatus = 113,  // 移除玩家状态；参数见 RemovePlayerStatusEvent。
    UpgradeHandPattern = 114,  // 执行牌型加强；参数见 UpgradeHandPatternEvent。
    PurchaseFacility = 115,  // 执行设施购买与生效；参数见 PurchaseFacilityEvent。
    UseExpansionCard = 116,  // 执行获得即使用的拓展牌；参数见 UseExpansionCardEvent。
    AddDelayedEffect = 117,  // 建立延迟效果标记；参数见 AddDelayedEffectEvent。
    OpenChoice = 118,  // 建立选择窗口并挂起相关流程；参数见 OpenChoiceEvent。
    ResolveChoice = 119,  // 执行选择或默认方案；参数见 ResolveChoiceEvent。
    ChangeAbilityCounter = 120,  // 改变已定义的技能成长或蓄力计数；参数见 ChangeAbilityCounterEvent。
    EventBatch = 121,  // 有序原子组合多个效果；参数见 EventBatch。
    PlayerJoinedControl = 122,  // 玩家加入房间；参数见 PlayerJoinedControl。
    PlayerDisconnectedControl = 123,  // 连接断开；参数见 PlayerDisconnectedControl。
    PlayerReconnectedControl = 124,  // 原玩家重连；参数见 PlayerReconnectedControl。
    ShoppingDeadlineControl = 125,  // 商店计时到期；参数见 ShoppingDeadlineControl。
    TurnDeadlineControl = 126,  // 出牌计时到期；参数见 TurnDeadlineControl。
    ChoiceDeadlineControl = 127,  // 选择计时到期；参数见 ChoiceDeadlineControl。
    RequestRejectedControl = 128,  // 操作请求未获接受；参数见 RequestRejectedControl。
    StateSnapshotReadyControl = 129,  // 同步快照已经准备；参数见 StateSnapshotReadyControl。
    PlayOptionsReadyControl = 130,  // 合法出牌或赖子方案已经计算；参数见 PlayOptionsReadyControl。
    ShopQuoteReadyControl = 131,  // 商店报价已经计算；参数见 ShopQuoteReadyControl。
    ConsumableUsed = 132,  // 消耗牌已购买并完成即时效果；参数见 ConsumableUsedEvent。
    ShopSlotsChanged = 133,  // 普通商店货位数量变化；参数见 ShopSlotsChangedEvent。
    ShopLevelChanged = 134,  // 商店等级变化；参数见 ShopLevelChangedEvent。
    FunctionCardEnhanced = 135,  // 功能牌实例强化成功；参数见 FunctionCardEnhancedEvent。
    Count = 136  // 类型总数及掩码长度；不是一个可提交的事件类型。
};

/// 事件类型总数，可用作按 EventType 索引的数组或技能订阅掩码长度。
inline constexpr std::size_t TOTAL_EVENT_TYPES =
    static_cast<std::size_t>(EventType::Count);

/// 出牌牌型；具体合法性和奖励由牌型模块判断。
enum class HandPattern : std::uint8_t {
    Single,  // 单张
    Pair,  // 对子
    Straight,  // 顺子
    Bomb,  // 炸弹
    StraightFlush  // 同花顺
};
/// 同一货架上的商品类型；消耗牌购买后不装备、不存入背包。
enum class ShopProductKind : std::uint8_t { FunctionCard, Consumable };
inline constexpr std::size_t MAX_SHOP_SLOTS = 6;
inline constexpr unsigned MAX_SHOP_LEVEL = 3;
/// 普通扑克牌的四种花色；王的花色处理由具体操作规则决定。
enum class Suit : std::uint8_t {
    Spades,  // 黑桃
    Hearts,  // 红桃
    Clubs,  // 梅花
    Diamonds  // 方块
};
/// 已经确认的不出牌操作的原因。
enum class PassReason : std::uint8_t {
    Chosen,  // 玩家主动选择不出牌
    Timeout,  // 行动超时后按规则不出牌
    Disconnected  // 掉线期间按规则不出牌
};
/// 本次非初始摸牌的业务原因。
enum class DrawReason : std::uint8_t {
    TrickEnd,  // 巡末摸牌
    Skill,  // 技能产生的摸牌
    Swap  // 换牌过程中补入新牌
};
/// 功能卡离开槽位的原因，供外部模块清理对应效果。
enum class RemovalReason : std::uint8_t {
    Sold,  // 出售
    Replaced,  // 被其他功能卡替换
    Consumed,  // 作为消耗品使用完毕
    Deleted  // 由规则或效果删除
};
/// 选择交互的业务内容；选项与授权由交互模块维护。
enum class ChoiceKind : std::uint8_t {
    Booster,  // 选择卡包候选
    Wildcard,  // 指定赖子的点数或花色方案
    Target,  // 选择玩家、牌或技能等目标
    Option  // 选择一般规则选项
};
/// 交互选择完成的方式。
enum class ChoiceReason : std::uint8_t {
    Manual,  // 玩家主动提交选择
    Timeout,  // 超时后采用默认方案
    Disconnected  // 掉线期间采用规则指定方案
};
/// 功能卡镀层的种类；具体效果由功能卡和技能模块定义。
enum class CoatingType : std::uint8_t {
    Gold,  // 黄金镀层
    Laser  // 镭射镀层
};
/// 整场结束后申请的续玩模式。
enum class ContinueMode : std::uint8_t {
    ExtraThree,  // 追加三局
    Endless  // 进入无尽模式
};
/// 需要报价的商店操作。
enum class ShopOperation : std::uint8_t {
    Purchase,  // 购买商品
    Sale,  // 出售功能卡
    Refresh,  // 刷新货架
    HandUpgrade,  // 加强牌型
    Facility  // 购买设施
};
/// 本次刷新采用的结算方式；次数上限仍由商店规则检查。
enum class RefreshMode : std::uint8_t {
    Paid,  // 支付本次刷新费用
    Free  // 使用已获得的免费刷新额度
};
/// 临时效果的有效范围；具体失效边界由生命周期模块处理。
enum class Lifetime : std::uint8_t {
    Event,  // 当前事件范围
    Turn,  // 当前玩家行动回合范围
    Trick,  // 当前一巡范围
    Round,  // 当前一局范围
    Shop,  // 当前商店会话范围
    Game  // 当前整场游戏范围
};
/// 待结算金币修正所指向的奖励组成部分。
enum class RewardComponent : std::uint8_t {
    Base,  // 基础奖励部分
    Extra  // 额外奖励部分
};
/// 有理数倍率运算后转换为整数时采用的取整方式。
enum class RoundingRule : std::uint8_t {
    Floor,  // 向下取整
    Ceil,  // 向上取整
    Nearest  // 取最接近的整数；等距情况由结算规则规定
};
/// 对技能计数项执行的数值操作。
enum class CounterOperation : std::uint8_t {
    Add,  // 在当前值上加 value
    Set  // 直接把当前值设为 value
};

/// 调度记录中的输入来源；由可信入口填写。
enum class EventOrigin : std::uint8_t {
    Server,  // 服务端业务或内部效果。
    Client,  // 已识别玩家提交的外部请求。
    Timer,  // 定时器到期。
    Connection  // 加入、掉线或重连。
};

/// 调度记录中的执行模式；实际行为仍由业务规则决定。
enum class ExecutionMode : std::uint8_t {
    Commit,  // 执行实际操作。
    Preview  // 只计算预览或报价。
};

/// 系统牌在玩家功能牌之前或之后执行。
enum class SystemOrder : std::uint8_t { BeforePlayers, AfterPlayers };
enum class GamePhase : std::uint8_t {
    NotStarted, Shopping, RoundStarting, Playing, RoundEnding, Finished
};
inline constexpr std::size_t MAX_PLAYERS = 4;
inline constexpr std::size_t MAX_FUNCTION_SLOTS = 6;
inline constexpr std::size_t PLAYER_CARD_ROWS = MAX_PLAYERS * MAX_FUNCTION_SLOTS;

}  // namespace cardgame

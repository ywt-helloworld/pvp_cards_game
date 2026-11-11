#pragma once

/// @file Event.hpp
/// @brief 定义事件的操作参数和多态数据结构，使用 C++17。
/// 公共枚举与类型别名统一维护在 Common/enums.hpp 和 Common/types.hpp。
/// 每个具体事件直接继承 Event，构造时固定唯一的 EventType。
/// actor 是操作发起者；target、player、owner、viewer 等字段按各事件说明解释。
/// 构造事件只创建数据；操作执行、合法性校验、状态修改与技能调度由外部模块负责。
/// 请求去重、通用来源链与通用版本记录由外部调度模块管理。
/// 默认构造后的 ID、枚举和数值是占位值，业务模块应在使用前补齐并校验。

#include "Common/enums.hpp"
#include "Common/types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace cardgame {

/// 用所属玩家和稳定序号引用一张功能卡；不是实际扑克牌的 CardId。
struct CardRef {
    Players owner = Players::P1;  // 功能卡持有者；默认 P1 只是初始占位值。
    std::size_t seq = 0;  // 该玩家名下功能卡的稳定实例序号，不是槽位位置。
};

/// 引用一张功能卡上的一个技能，避免持有技能对象的裸指针。
struct AbilityRef {
    CardRef card;  // 技能所属的功能卡引用。
    std::size_t seq = 0;  // 牌内稳定技能编号，不是 vector 下标
};

/// 一次赖子指定方案中的单张牌映射，供外部牌型模块解释。
struct WildcardAssignment {
    CardId cardId = 0;  // 本次指定的实际扑克牌实例。
    int rank = 0;  // 指定后的点数，合法范围由牌型规则检查。
    Suit suit = Suit::Spades;  // 指定后的花色，仅在本次方案需要花色时采用。
};

/// 保存一次操作选择的目标与牌面方案；哪些字段必填由具体操作决定。
struct TargetSpec {
    std::vector<Players> players;  // 所选玩家列表；为空的含义由具体操作规则确定。
    std::vector<CardId> cards;  // 所选实际扑克牌实例列表。
    std::vector<AbilityRef> abilities;  // 所选技能引用列表。
    std::optional<int> rank;  // 可选点数；未指定时为空。
    std::optional<Suit> suit;  // 可选花色；未指定时为空。
};

/// 某个待结算奖励记录的数值快照；数据本身不会发放奖励。
struct RewardData {
    SettlementId settlementId = 0;  // 所属奖励结算记录，用于关联本次修正与发放。
    ScoreValue score = 0;  // 该记录当前计算出的分数。
    CoinValue coins = 0;  // 该记录当前计算出的金币。
};

/// 一次商店报价的数值快照；报价确认和实际扣款由商店模块负责。
struct ShopData {
    SettlementId settlementId = 0;  // 本次报价结算记录。
    CoinValue baseCost = 0;  // 应用本次修正前的基础报价。
    CoinValue finalCost = 0;  // 应用本次修正后的最终报价。
    std::uint64_t priceVersion = 0;  // 报价对应的价格版本，提交操作时由商店模块核验。
};

/// 抽象事件数据基类：type 标识操作，actor 标识发起者，专属参数由派生类声明。
class Event {
public:
    /// 保持基类抽象，并允许通过 EventPtr 正确销毁任意派生事件。
    virtual ~Event() = 0;
    /// 事件实例采用唯一所有权，通过移动 EventPtr 交接，不按值复制。
    Event(const Event&) = delete;
    /// 禁止复制赋值，避免把两个实例混为同一次操作。
    Event& operator=(const Event&) = delete;

    const EventType type;  // 由具体事件构造函数固定，构造后不可改成另一种操作。
    std::optional<Players> actor;  // 发起玩家，与效果 target 分开；全局通知可为空，玩家请求由可信入口填写。

protected:
    /// 仅固定操作类型；构造不检查或执行操作，派生类的其余参数仍需填入。
    explicit Event(EventType type) : type(type) {}
};  // 事件只描述操作标识和参数；执行与调度由外部模块负责。

// 纯虚析构仍须提供定义，派生事件销毁时会调用这里。
inline Event::~Event() = default;

// 操作请求（21 个）：表达玩家意图或只读查询。

/// @brief 购买普通货架商品。
class BuyShopItemRequest final : public Event {
public:
    BuyShopItemRequest()
        : Event(EventType::BuyShopItemRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
    ProductId productId{};  // 商品
    std::optional<CardRef> targetCard;  // 需要强化目标的消耗牌在购买时选定自己的功能牌；其他商品留空。
};  // 玩家请求：购买普通货架商品；商店队列；价格由服务端计算。

/// @brief 购买并立即打开卡包。
class BuyBoosterPackRequest final : public Event {
public:
    BuyBoosterPackRequest()
        : Event(EventType::BuyBoosterPackRequest) {}
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 展示中的卡包实例
};  // 玩家请求：购买并立即打开卡包；商店队列；验证空槽与包限购，扣款后立即开包。

/// @brief 出售功能卡。
class SellFunctionCardRequest final : public Event {
public:
    SellFunctionCardRequest()
        : Event(EventType::SellFunctionCardRequest) {}
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
};  // 玩家请求：出售功能卡；商店队列；出售成本和成长清理按既有规则。

/// @brief 刷新普通货架。
class RefreshShopRequest final : public Event {
public:
    RefreshShopRequest()
        : Event(EventType::RefreshShopRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：刷新普通货架；商店队列；出队时检查刷新上限及当前报价。

/// @brief 锁定货位。
class LockShopSlotRequest final : public Event {
public:
    LockShopSlotRequest()
        : Event(EventType::LockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：锁定货位；由调度器与业务模块校验当前状态、权限和参数后处理。

/// @brief 解锁货位。
class UnlockShopSlotRequest final : public Event {
public:
    UnlockShopSlotRequest()
        : Event(EventType::UnlockShopSlotRequest) {}
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 玩家请求：解锁货位；由调度器与业务模块校验当前状态、权限和参数后处理。

/// @brief 选择卡包候选。
class ChooseBoosterCardRequest final : public Event {
public:
    ChooseBoosterCardRequest()
        : Event(EventType::ChooseBoosterCardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    ChoiceOptionId optionId{};  // 选中的候选
};  // 玩家请求：选择卡包候选；授权选择回复，不作为下一次普通购物排队。

/// @brief 放弃已打开卡包。
class SkipBoosterPackRequest final : public Event {
public:
    SkipBoosterPackRequest()
        : Event(EventType::SkipBoosterPackRequest) {}
    ChoiceId choiceId{};  // 待处理交互
};  // 玩家请求：放弃已打开卡包；授权选择回复；不退已支付包价。

/// @brief 付费加强牌型。
class UpgradeHandPatternRequest final : public Event {
public:
    UpgradeHandPatternRequest()
        : Event(EventType::UpgradeHandPatternRequest) {}
    ShopId shopId{};  // 商店会话
    HandPattern pattern{};  // 牌型
};  // 玩家请求：付费加强牌型；由调度器与业务模块校验当前状态、权限和参数后处理。

/// @brief 购买设施。
class BuyFacilityRequest final : public Event {
public:
    BuyFacilityRequest()
        : Event(EventType::BuyFacilityRequest) {}
    ShopId shopId{};  // 商店会话
    DefinitionId facilityId{};  // 设施定义
};  // 玩家请求：购买设施；由调度器与业务模块校验当前状态、权限和参数后处理。

/// @brief 完成本次购物。
class FinishShoppingRequest final : public Event {
public:
    FinishShoppingRequest()
        : Event(EventType::FinishShoppingRequest) {}
    ShopId shopId{};  // 商店会话
};  // 玩家请求：完成本次购物；由调度器与业务模块校验当前状态、权限和参数后处理。

/// @brief 确认出牌。
class PlayCardsRequest final : public Event {
public:
    PlayCardsRequest()
        : Event(EventType::PlayCardsRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 赖子指定方案
};  // 玩家请求：确认出牌；局内直接；合法提交后才产生出牌成功通知。

/// @brief 主动不出牌。
class PassRequest final : public Event {
public:
    PassRequest()
        : Event(EventType::PassRequest) {}
    TurnId turnId{};  // 所属回合
};  // 玩家请求：主动不出牌；局内直接；先手不能因此跳过领出。

/// @brief 发动主动技能。
class ActivateAbilityRequest final : public Event {
public:
    ActivateAbilityRequest()
        : Event(EventType::ActivateAbilityRequest) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 玩家请求：发动主动技能；校验技能归属、窗口、费用与次数；商店和局内依窗口处理。

/// @brief 提交一般目标或方案。
class SubmitChoiceRequest final : public Event {
public:
    SubmitChoiceRequest()
        : Event(EventType::SubmitChoiceRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 选中的合法选项
};  // 玩家请求：提交一般目标或方案；按交互授权接受，允许非当前行动玩家回复。

/// @brief 确认赖子指定。
class ResolveWildcardRequest final : public Event {
public:
    ResolveWildcardRequest()
        : Event(EventType::ResolveWildcardRequest) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 玩家请求：确认赖子指定；按赖子交互授权接受，确认后仍需校验跟牌。

/// @brief 请求加赛或无尽模式。
class ContinueGameRequest final : public Event {
public:
    ContinueGameRequest()
        : Event(EventType::ContinueGameRequest) {}
    ContinueMode mode{};  // 续玩模式
};  // 玩家请求：请求加赛或无尽模式；游戏结束后的模式请求；多人一致性规则待既有房间模块确定。

/// @brief 请求开始新一场。
class StartNewGameRequest final : public Event {
public:
    StartNewGameRequest()
        : Event(EventType::StartNewGameRequest) {}
};  // 玩家请求：请求开始新一场；按房间流程验证，新游戏重新初始化资源。

/// @brief 请求状态同步。
class StateSyncRequest final : public Event {
public:
    StateSyncRequest()
        : Event(EventType::StateSyncRequest) {}
    std::uint64_t lastSeenVersion{};  // 客户端已知版本
};  // 玩家请求：请求状态同步；只读同步通道，不受出牌请求忙碌门禁限制。

/// @brief 查询选牌的合法方案。
class PreviewPlayRequest final : public Event {
public:
    PreviewPlayRequest()
        : Event(EventType::PreviewPlayRequest) {}
    TurnId turnId{};  // 所属回合
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 玩家请求：查询选牌的合法方案；只读预览；不提交出牌、不扣次数、不推进实际随机序列。

/// @brief 查询商店操作报价。
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

/// @brief 整场开始。
class GameStartedEvent final : public Event {
public:
    GameStartedEvent()
        : Event(EventType::GameStart) {}
    std::vector<Players> participants{};  // 实际参局玩家
    int roundCount{};  // 标准局数
};  // 业务通知：整场开始，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 进入局前商店。
class ShopEnteredEvent final : public Event {
public:
    ShopEnteredEvent()
        : Event(EventType::ShopEnter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
};  // 业务通知：进入局前商店，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 商店窗口内的主动技能调用。
class ShopAbilityActivationEvent final : public Event {
public:
    ShopAbilityActivationEvent()
        : Event(EventType::ShopActionWindow) {}
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：商店窗口内的主动技能调用，由调度器验证请求后只派发给指定功能牌。

/// @brief 刷新报价前。
class BeforeShopRefreshEvent final : public Event {
public:
    BeforeShopRefreshEvent()
        : Event(EventType::ShopRefreshQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：刷新报价前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 刷新成功后。
class ShopRefreshedEvent final : public Event {
public:
    ShopRefreshedEvent()
        : Event(EventType::ShopRefreshAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    std::vector<SlotId> slots{};  // 实际替换的货位
    CoinValue paid{};  // 实际费用
};  // 业务通知：刷新成功后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 购买报价前。
class BeforeShopPurchaseEvent final : public Event {
public:
    BeforeShopPurchaseEvent()
        : Event(EventType::ShopBuyQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SettlementId settlementId{};  // 待结算事务
    ProductId productId{};  // 商品
    ShopData initialQuote{};  // 初始报价
    std::optional<CardRef> targetCard;  // 购买请求中已经选定的功能牌目标，成交前仍需重新校验。
};  // 业务通知：购买报价前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 购买交付成功后。
class ShopPurchasedEvent final : public Event {
public:
    ShopPurchasedEvent()
        : Event(EventType::ShopBuyAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    ProductId productId{};  // 商品
    CoinValue paid{};  // 实际费用
};  // 业务通知：购买交付成功后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 出售报价前。
class BeforeShopSaleEvent final : public Event {
public:
    BeforeShopSaleEvent()
        : Event(EventType::ShopSellQuote) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    SettlementId settlementId{};  // 待结算事务
    ShopData initialQuote{};  // 初始报价
};  // 业务通知：出售报价前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 出售成功后。
class ShopSoldEvent final : public Event {
public:
    ShopSoldEvent()
        : Event(EventType::ShopSellAfter) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    CardRef card{};  // 功能卡实例
    CoinValue received{};  // 实际收入
};  // 业务通知：出售成功后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 离店。
class ShopLeftEvent final : public Event {
public:
    ShopLeftEvent()
        : Event(EventType::ShopLeave) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    bool timedOut{};  // 是否商店超时
};  // 业务通知：离店，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 本局开始。
class RoundStartedEvent final : public Event {
public:
    RoundStartedEvent()
        : Event(EventType::RoundStart) {}
    RoundId roundId{};  // 所属局
    Players firstPlayer{};  // 本局先手
};  // 业务通知：本局开始，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 初始发牌前。
class BeforeInitialDealEvent final : public Event {
public:
    BeforeInitialDealEvent()
        : Event(EventType::InitialDealBefore) {}
    RoundId roundId{};  // 所属局
    int cardsPerPlayer = 7;  // 既有规则起手数量
};  // 业务通知：初始发牌前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 全体初始发牌完成。
class InitialCardsDealtEvent final : public Event {
public:
    InitialCardsDealtEvent()
        : Event(EventType::InitialDealAfter) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体初始发牌完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 本巡开始。
class TrickStartedEvent final : public Event {
public:
    TrickStartedEvent()
        : Event(EventType::TrickStart) {}
    RoundId roundId{};  // 所属局
    TrickId trickId{};  // 所属巡
    Players leader{};  // 领出玩家
};  // 业务通知：本巡开始，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 当前回合开始。
class TurnStartedEvent final : public Event {
public:
    TurnStartedEvent()
        : Event(EventType::TurnStart) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合开始，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 出牌窗口内的主动技能调用。
class TurnAbilityActivationEvent final : public Event {
public:
    TurnAbilityActivationEvent()
        : Event(EventType::PlayActionWindow) {}
    TurnId turnId{};  // 所属回合
    AbilityRef ability{};  // 目标技能
    TargetSpec selection{};  // 目标与方案
};  // 主动调用通知：出牌窗口内的主动技能调用，由调度器验证请求后只派发给指定功能牌。

/// @brief 验证选牌及跟牌。
class PlayValidationEvent final : public Event {
public:
    PlayValidationEvent()
        : Event(EventType::PlayValidate) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::vector<WildcardAssignment> wildcards{};  // 指定方案
};  // 业务通知：验证选牌及跟牌，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 成功提交一手牌。
class CardsPlayedEvent final : public Event {
public:
    CardsPlayedEvent()
        : Event(EventType::PlayCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 已提交的一手牌
    std::vector<CardId> cards{};  // 实际扑克牌实例
    HandPattern pattern{};  // 最终牌型
};  // 业务通知：成功提交一手牌，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 计算本手出牌奖励前。
class BeforePlayRewardEvent final : public Event {
public:
    BeforePlayRewardEvent()
        : Event(EventType::PlayRewardBefore) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 原始奖励
};  // 业务通知：计算本手出牌奖励前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 本手奖励到账后。
class PlayRewardGrantedEvent final : public Event {
public:
    PlayRewardGrantedEvent()
        : Event(EventType::PlayRewardAfter) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PlayedHandId handId{};  // 对应出牌
    RewardData reward{};  // 实际到账
};  // 业务通知：本手奖励到账后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 确认不出牌。
class PlayerPassedEvent final : public Event {
public:
    PlayerPassedEvent()
        : Event(EventType::PassCommitted) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    PassReason reason{};  // 主动、超时或掉线
};  // 业务通知：确认不出牌，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 当前回合结算结束。
class TurnEndedEvent final : public Event {
public:
    TurnEndedEvent()
        : Event(EventType::TurnEnd) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
};  // 业务通知：当前回合结算结束，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 获得牌权并结束本巡。
class TrickEndedEvent final : public Event {
public:
    TrickEndedEvent()
        : Event(EventType::TrickEnd) {}
    TrickId trickId{};  // 所属巡
    Players leader{};  // 牌权获得者
};  // 业务通知：获得牌权并结束本巡，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 非初始摸牌前。
class BeforeCardsDrawEvent final : public Event {
public:
    BeforeCardsDrawEvent()
        : Event(EventType::DrawBefore) {}
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    int requestedCount{};  // 请求摸牌数
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：非初始摸牌前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 实际摸牌完成。
class CardsDrawnEvent final : public Event {
public:
    CardsDrawnEvent()
        : Event(EventType::DrawAfter) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    DrawReason reason{};  // 摸牌原因
};  // 业务通知：实际摸牌完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 巡末摸牌步骤完成。
class TrickDrawingCompletedEvent final : public Event {
public:
    TrickDrawingCompletedEvent()
        : Event(EventType::TrickDrawsAfter) {}
    TrickId trickId{};  // 所属巡
    int actualTotal{};  // 全体实际摸牌总数
};  // 业务通知：巡末摸牌步骤完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 本局结束且最后回合结算完成。
class RoundEndedEvent final : public Event {
public:
    RoundEndedEvent()
        : Event(EventType::RoundEnd) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 已锁定结果
};  // 业务通知：本局结束且最后回合结算完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 计算玩家局末奖励前。
class BeforeRoundRewardEvent final : public Event {
public:
    BeforeRoundRewardEvent()
        : Event(EventType::RoundRewardBefore) {}
    RoundId roundId{};  // 所属局
    Players player{};  // 相关玩家
    SettlementId settlementId{};  // 待结算事务
    RewardData initialReward{};  // 基础待结算奖励
};  // 业务通知：计算玩家局末奖励前，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 全体局末奖励与扣款完成。
class RoundRewardsGrantedEvent final : public Event {
public:
    RoundRewardsGrantedEvent()
        : Event(EventType::RoundRewardAfter) {}
    RoundId roundId{};  // 所属局
    RoundResultId resultId{};  // 实际结算记录
};  // 业务通知：全体局末奖励与扣款完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 整场结束与冠军确认。
class GameEndedEvent final : public Event {
public:
    GameEndedEvent()
        : Event(EventType::GameEnd) {}
    std::vector<Players> winners{};  // 并列冠军
    std::uint64_t finalVersion{};  // 最终状态版本
};  // 业务通知：整场结束与冠军确认，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能实际改变牌面或手牌后。
class PlayingCardsChangedEvent final : public Event {
public:
    PlayingCardsChangedEvent()
        : Event(EventType::CardChanged) {}
    Players player{};  // 相关玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
    std::string reason{};  // 变化原因
};  // 业务通知：技能实际改变牌面或手牌后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能效果成功结算后。
class AbilityResolvedEvent final : public Event {
public:
    AbilityResolvedEvent()
        : Event(EventType::AbilityResolved) {}
    AbilityRef ability{};  // 目标技能
    EventId triggerEventId{};  // 触发事件
};  // 业务通知：技能效果成功结算后，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 功能卡进入槽位。
class FunctionCardEquippedEvent final : public Event {
public:
    FunctionCardEquippedEvent()
        : Event(EventType::FunctionCardEquipped) {}
    CardRef card{};  // 功能卡实例
    std::size_t slot{};  // 实际槽位
};  // 业务通知：功能卡进入槽位，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 功能卡离开槽位。
class FunctionCardRemovedEvent final : public Event {
public:
    FunctionCardRemovedEvent()
        : Event(EventType::FunctionCardRemoved) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 业务通知：功能卡离开槽位，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 货位锁定成功。
class ShopSlotLockedEvent final : public Event {
public:
    ShopSlotLockedEvent()
        : Event(EventType::ShopSlotLocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位锁定成功，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 货位解锁成功。
class ShopSlotUnlockedEvent final : public Event {
public:
    ShopSlotUnlockedEvent()
        : Event(EventType::ShopSlotUnlocked) {}
    Players player{};  // 相关玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 业务通知：货位解锁成功，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 卡包已付费打开。
class BoosterPackOpenedEvent final : public Event {
public:
    BoosterPackOpenedEvent()
        : Event(EventType::BoosterPackOpened) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：卡包已付费打开，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 卡包候选选取完成。
class BoosterPackChoiceCompletedEvent final : public Event {
public:
    BoosterPackChoiceCompletedEvent()
        : Event(EventType::BoosterPackChoiceCompleted) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
    std::vector<ChoiceOptionId> options{};  // 实际选项
};  // 业务通知：卡包候选选取完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 卡包已放弃。
class BoosterPackSkippedEvent final : public Event {
public:
    BoosterPackSkippedEvent()
        : Event(EventType::BoosterPackSkipped) {}
    Players player{};  // 相关玩家
    PackId packId{};  // 包实例
};  // 业务通知：卡包已放弃，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 拓展牌获得即使用完成。
class ExpansionCardUsedEvent final : public Event {
public:
    ExpansionCardUsedEvent()
        : Event(EventType::ExpansionCardUsed) {}
    Players player{};  // 相关玩家
    DefinitionId expansionId{};  // 拓展牌定义
    TargetSpec selection{};  // 目标与方案
};  // 业务通知：拓展牌获得即使用完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 牌型加强完成。
class HandPatternUpgradedEvent final : public Event {
public:
    HandPatternUpgradedEvent()
        : Event(EventType::HandPatternUpgraded) {}
    Players player{};  // 相关玩家
    HandPattern pattern{};  // 牌型
    int beforeLevel{};  // 原等级
    int afterLevel{};  // 新等级
};  // 业务通知：牌型加强完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 设施购买生效。
class FacilityPurchasedEvent final : public Event {
public:
    FacilityPurchasedEvent()
        : Event(EventType::FacilityPurchased) {}
    Players player{};  // 相关玩家
    DefinitionId facilityId{};  // 设施定义
};  // 业务通知：设施购买生效，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 金币余额实际变化。
class CoinsChangedEvent final : public Event {
public:
    CoinsChangedEvent()
        : Event(EventType::CoinsChanged) {}
    Players player{};  // 相关玩家
    CoinValue before{};  // 变化前
    CoinValue after{};  // 变化后
    std::string reason{};  // 收入或支出原因
};  // 业务通知：金币余额实际变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 累计分数实际变化。
class ScoreChangedEvent final : public Event {
public:
    ScoreChangedEvent()
        : Event(EventType::ScoreChanged) {}
    Players player{};  // 相关玩家
    ScoreValue before{};  // 变化前
    ScoreValue after{};  // 变化后
};  // 业务通知：累计分数实际变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 玩家状态实际变化。
class PlayerStatusChangedEvent final : public Event {
public:
    PlayerStatusChangedEvent()
        : Event(EventType::PlayerStatusChanged) {}
    Players player{};  // 相关玩家
    StatusId statusId{};  // 状态实例
    std::string reason{};  // 增加、移除或层数变化
};  // 业务通知：玩家状态实际变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能添加完成。
class AbilityAddedEvent final : public Event {
public:
    AbilityAddedEvent()
        : Event(EventType::AbilityAdded) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能添加完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能逻辑移除完成。
class AbilityRemovedEvent final : public Event {
public:
    AbilityRemovedEvent()
        : Event(EventType::AbilityRemoved) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能逻辑移除完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能被永久废除。
class AbilityAbolishedEvent final : public Event {
public:
    AbilityAbolishedEvent()
        : Event(EventType::AbilityAbolished) {}
    AbilityRef ability{};  // 目标技能
};  // 业务通知：技能被永久废除，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能临时可用开关变化。
class AbilityUsabilityChangedEvent final : public Event {
public:
    AbilityUsabilityChangedEvent()
        : Event(EventType::AbilityUsabilityChanged) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 当前开关
};  // 业务通知：技能临时可用开关变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 交互选择窗口建立。
class ChoiceOpenedEvent final : public Event {
public:
    ChoiceOpenedEvent()
        : Event(EventType::ChoiceOpened) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    ChoiceKind choiceKind{};  // 交互类型
};  // 业务通知：交互选择窗口建立，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 交互选择处理完成。
class ChoiceResolvedEvent final : public Event {
public:
    ChoiceResolvedEvent()
        : Event(EventType::ChoiceResolved) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选择
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 业务通知：交互选择处理完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 交互期限已到并完成默认处理。
class ChoiceExpiredEvent final : public Event {
public:
    ChoiceExpiredEvent()
        : Event(EventType::ChoiceExpired) {}
    Players player{};  // 相关玩家
    ChoiceId choiceId{};  // 待处理交互
};  // 业务通知：交互期限已到并完成默认处理，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 出空时锁定赢家与手牌快照。
class RoundWinnerLockedEvent final : public Event {
public:
    RoundWinnerLockedEvent()
        : Event(EventType::RoundWinnerLocked) {}
    RoundId roundId{};  // 所属局
    Players winner{};  // 赢家
    RoundResultId resultId{};  // 冻结结果
};  // 业务通知：出空时锁定赢家与手牌快照，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 牌堆由非空变为空。
class DrawPileExhaustedEvent final : public Event {
public:
    DrawPileExhaustedEvent()
        : Event(EventType::DrawPileExhausted) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：牌堆由非空变为空，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 全体商店会话关闭且交易结清。
class AllShopsClosedEvent final : public Event {
public:
    AllShopsClosedEvent()
        : Event(EventType::AllShopsClosed) {}
    RoundId roundId{};  // 所属局
};  // 业务通知：全体商店会话关闭且交易结清，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 整张功能卡临时开关变化。
class FunctionCardUsabilityChangedEvent final : public Event {
public:
    FunctionCardUsabilityChangedEvent()
        : Event(EventType::FunctionCardUsabilityChanged) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 当前开关
};  // 业务通知：整张功能卡临时开关变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 整张功能卡被废除。
class FunctionCardAbolishedEvent final : public Event {
public:
    FunctionCardAbolishedEvent()
        : Event(EventType::FunctionCardAbolished) {}
    CardRef card{};  // 功能卡实例
};  // 业务通知：整张功能卡被废除，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 技能成长或蓄力状态变化。
class AbilityStateChangedEvent final : public Event {
public:
    AbilityStateChangedEvent()
        : Event(EventType::AbilityStateChanged) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 已定义的计数状态
    int before{};  // 变化前
    int after{};  // 变化后
};  // 业务通知：技能成长或蓄力状态变化，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 延迟效果标记建立。
class DelayedEffectScheduledEvent final : public Event {
public:
    DelayedEffectScheduledEvent()
        : Event(EventType::DelayedEffectScheduled) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟效果标记建立，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 延迟标记实际执行完成。
class DelayedEffectTriggeredEvent final : public Event {
public:
    DelayedEffectTriggeredEvent()
        : Event(EventType::DelayedEffectTriggered) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记实际执行完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 延迟标记结束或失效。
class DelayedEffectExpiredEvent final : public Event {
public:
    DelayedEffectExpiredEvent()
        : Event(EventType::DelayedEffectExpired) {}
    Players player{};  // 相关玩家
    DelayedEffectId delayedEffectId{};  // 标记实例
};  // 业务通知：延迟标记结束或失效，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 镀层施加完成。
class CoatingAppliedEvent final : public Event {
public:
    CoatingAppliedEvent()
        : Event(EventType::CoatingApplied) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 业务通知：镀层施加完成，在对应操作提交后发布，供已注册功能牌响应。

/// @brief 查看权限或公开信息变化。
class CardVisibilityChangedEvent final : public Event {
public:
    CardVisibilityChangedEvent()
        : Event(EventType::CardVisibilityChanged) {}
    Players viewer{};  // 获得信息的玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 业务通知：查看权限或公开信息变化，在对应操作提交后发布，供已注册功能牌响应。

// 效果请求（38 个）：表达需要执行的效果。

/// @brief 增加实际分数。
class AddScoreEvent final : public Event {
public:
    AddScoreEvent()
        : Event(EventType::AddScore) {}
    Players target{};  // 效果目标玩家
    ScoreValue amount{};  // 增加数量
};  // 效果请求：增加实际分数；由对应系统牌与业务模块校验并执行。

/// @brief 增加实际金币。
class AddCoinsEvent final : public Event {
public:
    AddCoinsEvent()
        : Event(EventType::AddCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 增加数量
};  // 效果请求：增加实际金币；由对应系统牌与业务模块校验并执行。

/// @brief 扣除实际金币。
class SpendCoinsEvent final : public Event {
public:
    SpendCoinsEvent()
        : Event(EventType::SpendCoins) {}
    Players target{};  // 效果目标玩家
    CoinValue amount{};  // 扣款数量
    std::string reason{};  // 扣款用途
};  // 效果请求：扣除实际金币；由对应系统牌与业务模块校验并执行。

/// @brief 修正待结算分数。
class AddPendingScoreEvent final : public Event {
public:
    AddPendingScoreEvent()
        : Event(EventType::AddPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    ScoreValue delta{};  // 有符号增减值
};  // 效果请求：修正待结算分数；由对应系统牌与业务模块校验并执行。

/// @brief 缩放待结算分数。
class MultiplyPendingScoreEvent final : public Event {
public:
    MultiplyPendingScoreEvent()
        : Event(EventType::MultiplyPendingScore) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    int numerator = 1;  // 倍率分子
    int denominator = 1;  // 倍率分母
    RoundingRule rounding{};  // 取整规则
};  // 效果请求：缩放待结算分数；由对应系统牌与业务模块校验并执行。

/// @brief 修正待结算金币。
class AddPendingCoinsEvent final : public Event {
public:
    AddPendingCoinsEvent()
        : Event(EventType::AddPendingCoins) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue delta{};  // 有符号增减值
    RewardComponent component{};  // 基础或额外奖励
    CoinValue minimum{};  // 该部分最低金额
};  // 效果请求：修正待结算金币；由对应系统牌与业务模块校验并执行。

/// @brief 降低本次商店报价。
class ReduceShopCostEvent final : public Event {
public:
    ReduceShopCostEvent()
        : Event(EventType::ReduceShopCost) {}
    Players target{};  // 效果目标玩家
    SettlementId settlementId{};  // 待结算事务
    CoinValue amount{};  // 减费数
    CoinValue minimum{};  // 卡牌规定的最低价
};  // 效果请求：降低本次商店报价；由对应系统牌与业务模块校验并执行。

/// @brief 增加本店免费刷新额度。
class GrantFreeRefreshEvent final : public Event {
public:
    GrantFreeRefreshEvent()
        : Event(EventType::GrantFreeRefresh) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    int uses{};  // 额度数量
};  // 效果请求：增加本店免费刷新额度；由对应系统牌与业务模块校验并执行。

/// @brief 执行一次货架刷新。
class RefreshShopEvent final : public Event {
public:
    RefreshShopEvent()
        : Event(EventType::RefreshShop) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    RefreshMode mode{};  // 付费或免费
};  // 效果请求：执行一次货架刷新；由对应系统牌与业务模块校验并执行，仍检查本店刷新总上限，免费不推进付费价格序号。

/// @brief 执行已获准的开包。
class OpenBoosterPackEvent final : public Event {
public:
    OpenBoosterPackEvent()
        : Event(EventType::OpenBoosterPack) {}
    Players target{};  // 开包玩家
    ShopId shopId{};  // 商店会话
    PackId packId{};  // 卡包实例
    SettlementId settlementId{};  // 已验证的购买或授予事务
};  // 效果请求：执行已获准的开包；由对应系统牌与业务模块校验并执行，与购包事务关联，不跨输入等待；完成后发布开包通知并建立选择。

/// @brief 执行货位锁定。
class LockShopSlotEvent final : public Event {
public:
    LockShopSlotEvent()
        : Event(EventType::LockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位锁定；由对应系统牌与业务模块校验并执行。

/// @brief 执行货位解锁。
class UnlockShopSlotEvent final : public Event {
public:
    UnlockShopSlotEvent()
        : Event(EventType::UnlockShopSlot) {}
    Players target{};  // 效果目标玩家
    ShopId shopId{};  // 商店会话
    SlotId slotId{};  // 货位
};  // 效果请求：执行货位解锁；由对应系统牌与业务模块校验并执行。

/// @brief 生成并装备功能卡。
class GrantFunctionCardEvent final : public Event {
public:
    GrantFunctionCardEvent()
        : Event(EventType::GrantFunctionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId definitionId{};  // 牌种
    CoinValue purchaseCost{};  // 实例出售成本
};  // 效果请求：生成并装备功能卡；由对应系统牌与业务模块校验并执行，检查空槽；免费生成成本为零。

/// @brief 移除功能卡。
class RemoveFunctionCardEvent final : public Event {
public:
    RemoveFunctionCardEvent()
        : Event(EventType::RemoveFunctionCard) {}
    CardRef card{};  // 功能卡实例
    RemovalReason reason{};  // 移除原因
};  // 效果请求：移除功能卡；由对应系统牌与业务模块校验并执行。

/// @brief 替换功能卡。
class ReplaceFunctionCardEvent final : public Event {
public:
    ReplaceFunctionCardEvent()
        : Event(EventType::ReplaceFunctionCard) {}
    CardRef card{};  // 功能卡实例
    DefinitionId newDefinitionId{};  // 新牌种
    bool preservePurchaseCost = true;  // 是否沿用出售成本
};  // 效果请求：替换功能卡；由对应系统牌与业务模块校验并执行。

/// @brief 给功能卡添加技能。
class AddAbilityEvent final : public Event {
public:
    AddAbilityEvent()
        : Event(EventType::AddAbility) {}
    CardRef card{};  // 功能卡实例
    DefinitionId abilityDefinitionId{};  // 技能定义
};  // 效果请求：给功能卡添加技能；由对应系统牌与业务模块校验并执行。

/// @brief 逻辑删除技能。
class RemoveAbilityEvent final : public Event {
public:
    RemoveAbilityEvent()
        : Event(EventType::RemoveAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：逻辑删除技能；由对应系统牌与业务模块校验并执行。

/// @brief 永久废除技能。
class AbolishAbilityEvent final : public Event {
public:
    AbolishAbilityEvent()
        : Event(EventType::AbolishAbility) {}
    AbilityRef ability{};  // 目标技能
};  // 效果请求：永久废除技能；由对应系统牌与业务模块校验并执行。

/// @brief 设置技能临时开关。
class SetAbilityUsableEvent final : public Event {
public:
    SetAbilityUsableEvent()
        : Event(EventType::SetAbilityUsable) {}
    AbilityRef ability{};  // 目标技能
    bool usable{};  // 新开关
};  // 效果请求：设置技能临时开关；由对应系统牌与业务模块校验并执行。

/// @brief 设置整牌临时开关。
class SetFunctionCardUsableEvent final : public Event {
public:
    SetFunctionCardUsableEvent()
        : Event(EventType::SetFunctionCardUsable) {}
    CardRef card{};  // 功能卡实例
    bool usable{};  // 新开关
};  // 效果请求：设置整牌临时开关；由对应系统牌与业务模块校验并执行。

/// @brief 永久废除整张牌。
class AbolishFunctionCardEvent final : public Event {
public:
    AbolishFunctionCardEvent()
        : Event(EventType::AbolishFunctionCard) {}
    CardRef card{};  // 功能卡实例
};  // 效果请求：永久废除整张牌；由对应系统牌与业务模块校验并执行。

/// @brief 给功能卡施加镀层。
class ApplyCoatingEvent final : public Event {
public:
    ApplyCoatingEvent()
        : Event(EventType::ApplyCoating) {}
    CardRef card{};  // 功能卡实例
    CoatingType coating{};  // 镀层
};  // 效果请求：给功能卡施加镀层；由对应系统牌与业务模块校验并执行。

/// @brief 按规则执行非初始摸牌。
class DrawCardsEvent final : public Event {
public:
    DrawCardsEvent()
        : Event(EventType::DrawCards) {}
    Players target{};  // 效果目标玩家
    int count{};  // 数量
    DrawReason reason{};  // 摸牌原因
};  // 效果请求：按规则执行非初始摸牌；由对应系统牌与业务模块校验并执行，锁定本局结束后不得借此进行额外摸牌；牌堆不足按规则处理。

/// @brief 将指定手牌换到牌堆底并等量摸顶。
class SwapCardsWithDeckEvent final : public Event {
public:
    SwapCardsWithDeckEvent()
        : Event(EventType::SwapCardsWithDeck) {}
    Players target{};  // 效果目标玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：将指定手牌换到牌堆底并等量摸顶；由对应系统牌与业务模块校验并执行，保持原牌身份与公共牌组总数。

/// @brief 临时改变普通牌点数。
class ChangeCardRankEvent final : public Event {
public:
    ChangeCardRankEvent()
        : Event(EventType::ChangeCardRank) {}
    CardId cardId{};  // 扑克牌实例
    int rank{};  // 目标点数
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变普通牌点数；由对应系统牌与业务模块校验并执行。

/// @brief 临时改变花色。
class ChangeCardSuitEvent final : public Event {
public:
    ChangeCardSuitEvent()
        : Event(EventType::ChangeCardSuit) {}
    CardId cardId{};  // 扑克牌实例
    Suit suit{};  // 目标花色
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：临时改变花色；由对应系统牌与业务模块校验并执行。

/// @brief 将指定牌临时变为王。
class MakeCardJokerEvent final : public Event {
public:
    MakeCardJokerEvent()
        : Event(EventType::MakeCardJoker) {}
    CardId cardId{};  // 扑克牌实例
    Lifetime lifetime{};  // 持续范围
};  // 效果请求：将指定牌临时变为王；由对应系统牌与业务模块校验并执行。

/// @brief 向指定玩家展示合法信息。
class RevealCardsEvent final : public Event {
public:
    RevealCardsEvent()
        : Event(EventType::RevealCards) {}
    Players viewer{};  // 查看者
    Players target{};  // 被查看玩家
    std::vector<CardId> cards{};  // 实际扑克牌实例
};  // 效果请求：向指定玩家展示合法信息；由对应系统牌与业务模块校验并执行，只向有权限的客户端同步牌面。

/// @brief 施加玩家状态。
class ApplyPlayerStatusEvent final : public Event {
public:
    ApplyPlayerStatusEvent()
        : Event(EventType::ApplyPlayerStatus) {}
    Players target{};  // 效果目标玩家
    DefinitionId statusDefinitionId{};  // 状态定义
    int stacks = 1;  // 层数
    Lifetime lifetime{};  // 期限
};  // 效果请求：施加玩家状态；由对应系统牌与业务模块校验并执行。

/// @brief 移除玩家状态。
class RemovePlayerStatusEvent final : public Event {
public:
    RemovePlayerStatusEvent()
        : Event(EventType::RemovePlayerStatus) {}
    Players target{};  // 效果目标玩家
    StatusId statusId{};  // 状态实例
};  // 效果请求：移除玩家状态；由对应系统牌与业务模块校验并执行。

/// @brief 执行牌型加强。
class UpgradeHandPatternEvent final : public Event {
public:
    UpgradeHandPatternEvent()
        : Event(EventType::UpgradeHandPattern) {}
    Players target{};  // 效果目标玩家
    HandPattern pattern{};  // 牌型
    int levels = 1;  // 加强级数
};  // 效果请求：执行牌型加强；由对应系统牌与业务模块校验并执行。

/// @brief 执行设施购买与生效。
class PurchaseFacilityEvent final : public Event {
public:
    PurchaseFacilityEvent()
        : Event(EventType::PurchaseFacility) {}
    Players target{};  // 效果目标玩家
    DefinitionId facilityId{};  // 设施
};  // 效果请求：执行设施购买与生效；由对应系统牌与业务模块校验并执行。

/// @brief 执行获得即使用的拓展牌。
class UseExpansionCardEvent final : public Event {
public:
    UseExpansionCardEvent()
        : Event(EventType::UseExpansionCard) {}
    Players target{};  // 效果目标玩家
    DefinitionId expansionId{};  // 拓展牌
    TargetSpec selection{};  // 目标与方案
};  // 效果请求：执行获得即使用的拓展牌；由对应系统牌与业务模块校验并执行。

/// @brief 建立延迟效果标记。
class AddDelayedEffectEvent final : public Event {
public:
    AddDelayedEffectEvent()
        : Event(EventType::AddDelayedEffect) {}
    Players target{};  // 效果目标玩家
    DefinitionId effectDefinitionId{};  // 延迟效果定义
    EventType triggerType{};  // 未来触发的通知类型
    TargetSpec selection{};  // 目标与方案
    Lifetime lifetime{};  // 有效期
};  // 效果请求：建立延迟效果标记；由对应系统牌与业务模块校验并执行。

/// @brief 建立选择窗口并挂起相关流程。
class OpenChoiceEvent final : public Event {
public:
    OpenChoiceEvent()
        : Event(EventType::OpenChoice) {}
    Players target{};  // 效果目标玩家
    ChoiceKind choiceKind{};  // 选择类型
    std::vector<ChoiceOptionId> options{};  // 合法选项
    std::uint32_t timeoutMs{};  // 超时长度
    bool optional{};  // 是否可放弃
};  // 效果请求：建立选择窗口并挂起相关流程；由对应系统牌与业务模块校验并执行。

/// @brief 执行选择或默认方案。
class ResolveChoiceEvent final : public Event {
public:
    ResolveChoiceEvent()
        : Event(EventType::ResolveChoice) {}
    ChoiceId choiceId{};  // 待处理交互
    std::vector<ChoiceOptionId> options{};  // 实际选项
    ChoiceReason reason{};  // 手动、超时或掉线
};  // 效果请求：执行选择或默认方案；由对应系统牌与业务模块校验并执行。

/// @brief 改变已定义的技能成长或蓄力计数。
class ChangeAbilityCounterEvent final : public Event {
public:
    ChangeAbilityCounterEvent()
        : Event(EventType::ChangeAbilityCounter) {}
    AbilityRef ability{};  // 目标技能
    CounterId counterId{};  // 计数项
    CounterOperation operation{};  // 加值或设值
    int value{};  // 操作值
};  // 效果请求：改变已定义的技能成长或蓄力计数；由对应系统牌与业务模块校验并执行。

/// @brief 有序原子组合多个效果。
class EventBatch final : public Event {
public:
    EventBatch()
        : Event(EventType::EventBatch) {}
    std::vector<EventPtr> events{};  // 有序子效果
};  // 效果请求：有序原子组合多个效果；由对应系统牌与业务模块校验并执行，只允许 Effect 子事件；先校验后提交，不能跨玩家输入等待。

// 控制事件（10 个）：连接、计时器和服务端反馈。

/// @brief 玩家加入房间。
class PlayerJoinedControl final : public Event {
public:
    PlayerJoinedControl()
        : Event(EventType::PlayerJoinedControl) {}
    Players player{};  // 相关玩家
};  // 控制事件：玩家加入房间，由调度器更新房间流程，满足满员条件后自动开局。

/// @brief 连接断开。
class PlayerDisconnectedControl final : public Event {
public:
    PlayerDisconnectedControl()
        : Event(EventType::PlayerDisconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 连接代次
};  // 控制事件：连接断开，保留原座位与资源，按对应超时规则继续处理。

/// @brief 原玩家重连。
class PlayerReconnectedControl final : public Event {
public:
    PlayerReconnectedControl()
        : Event(EventType::PlayerReconnectedControl) {}
    Players player{};  // 相关玩家
    std::uint64_t connectionVersion{};  // 新连接代次
};  // 控制事件：原玩家重连，恢复原座位并同步有权查看的当前状态。

/// @brief 商店计时到期。
class ShoppingDeadlineControl final : public Event {
public:
    ShoppingDeadlineControl()
        : Event(EventType::ShoppingDeadlineControl) {}
    ShopId shopId{};  // 商店会话
    std::uint64_t expectedVersion{};  // 对应商店版本
};  // 控制事件：商店计时到期，由调度器检查商店会话与版本后执行结束流程。

/// @brief 出牌计时到期。
class TurnDeadlineControl final : public Event {
public:
    TurnDeadlineControl()
        : Event(EventType::TurnDeadlineControl) {}
    TurnId turnId{};  // 所属回合
    Players player{};  // 相关玩家
    std::uint64_t expectedVersion{};  // 对应回合版本
};  // 控制事件：出牌计时到期，由调度器检查回合与版本后执行既定超时操作。

/// @brief 选择计时到期。
class ChoiceDeadlineControl final : public Event {
public:
    ChoiceDeadlineControl()
        : Event(EventType::ChoiceDeadlineControl) {}
    ChoiceId choiceId{};  // 待处理交互
    std::uint64_t expectedVersion{};  // 对应交互版本
};  // 控制事件：选择计时到期，由调度器检查交互与版本后执行默认方案。

/// @brief 操作请求未获接受。
class RequestRejectedControl final : public Event {
public:
    RequestRejectedControl()
        : Event(EventType::RequestRejectedControl) {}
    RequestId requestId{};  // 被拒绝请求
    std::string reason{};  // 拒绝原因
    std::uint64_t currentVersion{};  // 当前版本
};  // 控制反馈：操作请求未获接受，只反馈原因，不视为成功行为或成功类收益来源。

/// @brief 同步快照已经准备。
class StateSnapshotReadyControl final : public Event {
public:
    StateSnapshotReadyControl()
        : Event(EventType::StateSnapshotReadyControl) {}
    Players player{};  // 相关玩家
    SnapshotId snapshotId{};  // 按权限过滤的快照
};  // 控制反馈：同步快照已经准备，只向指定玩家发送其有权查看的信息。

/// @brief 合法出牌或赖子方案已经计算。
class PlayOptionsReadyControl final : public Event {
public:
    PlayOptionsReadyControl()
        : Event(EventType::PlayOptionsReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    std::vector<ChoiceOptionId> options{};  // 合法方案
};  // 查询反馈：合法出牌或赖子方案已经计算，表示预览结果，尚未成功出牌。

/// @brief 商店报价已经计算。
class ShopQuoteReadyControl final : public Event {
public:
    ShopQuoteReadyControl()
        : Event(EventType::ShopQuoteReadyControl) {}
    Players player{};  // 相关玩家
    PreviewId previewId{};  // 预览结果
    ShopData quote{};  // 当前报价
};  // 查询反馈：商店报价已经计算，本身不扣款或消耗免费刷新额度。

/// 消耗牌已完成即时效果；购买、扣款和效果提交后才发布。
class ConsumableUsedEvent final : public Event {
public:
    ConsumableUsedEvent() : Event(EventType::ConsumableUsed) {}
    Players player{};
    ShopId shopId{};
    ProductId productId{};
    DefinitionId definition{};
    std::optional<CardRef> targetCard;  // 强化目标；无目标消耗牌为空。
};
/// 普通货架容量已改变，不是功能牌槽位变化。
class ShopSlotsChangedEvent final : public Event {
public:
    ShopSlotsChangedEvent() : Event(EventType::ShopSlotsChanged) {}
    Players player{};
    std::size_t before{}, after{};
};
/// 商店等级已改变；后续生成商品使用新等级。
class ShopLevelChangedEvent final : public Event {
public:
    ShopLevelChangedEvent() : Event(EventType::ShopLevelChanged) {}
    Players player{};
    unsigned before{}, after{};
};
/// 同一功能牌实例强化完成；不视为卸下或重新装备。
class FunctionCardEnhancedEvent final : public Event {
public:
    FunctionCardEnhancedEvent() : Event(EventType::FunctionCardEnhanced) {}
    CardRef card{};
    unsigned beforeLevel{}, afterLevel{};
};
}  // namespace cardgame

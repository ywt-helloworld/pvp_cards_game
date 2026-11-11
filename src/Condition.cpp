#include "Condition.hpp"

#include <stdexcept>
#include <utility>

namespace cardgame {
namespace {

struct Snapshots {
    std::optional<RewardData> reward;
    std::optional<ShopData> shop;
};

// 只有存在单份奖励或报价参数的事件提取公共快照；其他专属信息仍完整保存在事件中。
template<class EventT>
Snapshots snapshots_of(const EventT&) {
    return {};
}

Snapshots snapshots_of(const BeforePlayRewardEvent& event) {
    return {event.initialReward, std::nullopt};
}

Snapshots snapshots_of(const PlayRewardGrantedEvent& event) {
    return {event.reward, std::nullopt};
}

Snapshots snapshots_of(const BeforeRoundRewardEvent& event) {
    return {event.initialReward, std::nullopt};
}

Snapshots snapshots_of(const BeforeShopRefreshEvent& event) {
    return {std::nullopt, event.initialQuote};
}

Snapshots snapshots_of(const BeforeShopPurchaseEvent& event) {
    return {std::nullopt, event.initialQuote};
}

Snapshots snapshots_of(const BeforeShopSaleEvent& event) {
    return {std::nullopt, event.initialQuote};
}

Snapshots snapshots_of(const ShopQuoteReadyControl& event) {
    return {std::nullopt, event.quote};
}

template<class EventT>
Snapshots inspect_as(const Event& event) {
    const auto* typed = dynamic_cast<const EventT*>(&event);
    if (!typed) {
        throw std::invalid_argument("EventType does not match concrete event");
    }
    return snapshots_of(*typed);
}

Snapshots inspect_event(const Event& event) {
    // 编号和实际派生类一一校验；增加事件时应同步扩展此表。
    switch (event.type) {
    case EventType::BuyShopItemRequest:
        return inspect_as<BuyShopItemRequest>(event);
    case EventType::BuyBoosterPackRequest:
        return inspect_as<BuyBoosterPackRequest>(event);
    case EventType::SellFunctionCardRequest:
        return inspect_as<SellFunctionCardRequest>(event);
    case EventType::RefreshShopRequest:
        return inspect_as<RefreshShopRequest>(event);
    case EventType::LockShopSlotRequest:
        return inspect_as<LockShopSlotRequest>(event);
    case EventType::UnlockShopSlotRequest:
        return inspect_as<UnlockShopSlotRequest>(event);
    case EventType::ChooseBoosterCardRequest:
        return inspect_as<ChooseBoosterCardRequest>(event);
    case EventType::SkipBoosterPackRequest:
        return inspect_as<SkipBoosterPackRequest>(event);
    case EventType::UpgradeHandPatternRequest:
        return inspect_as<UpgradeHandPatternRequest>(event);
    case EventType::BuyFacilityRequest:
        return inspect_as<BuyFacilityRequest>(event);
    case EventType::FinishShoppingRequest:
        return inspect_as<FinishShoppingRequest>(event);
    case EventType::PlayCardsRequest:
        return inspect_as<PlayCardsRequest>(event);
    case EventType::PassRequest:
        return inspect_as<PassRequest>(event);
    case EventType::ActivateAbilityRequest:
        return inspect_as<ActivateAbilityRequest>(event);
    case EventType::SubmitChoiceRequest:
        return inspect_as<SubmitChoiceRequest>(event);
    case EventType::ResolveWildcardRequest:
        return inspect_as<ResolveWildcardRequest>(event);
    case EventType::ContinueGameRequest:
        return inspect_as<ContinueGameRequest>(event);
    case EventType::StartNewGameRequest:
        return inspect_as<StartNewGameRequest>(event);
    case EventType::StateSyncRequest:
        return inspect_as<StateSyncRequest>(event);
    case EventType::PreviewPlayRequest:
        return inspect_as<PreviewPlayRequest>(event);
    case EventType::QuoteShopOperationRequest:
        return inspect_as<QuoteShopOperationRequest>(event);
    case EventType::GameStart:
        return inspect_as<GameStartedEvent>(event);
    case EventType::ShopEnter:
        return inspect_as<ShopEnteredEvent>(event);
    case EventType::ShopActionWindow:
        return inspect_as<ShopAbilityActivationEvent>(event);
    case EventType::ShopRefreshQuote:
        return inspect_as<BeforeShopRefreshEvent>(event);
    case EventType::ShopRefreshAfter:
        return inspect_as<ShopRefreshedEvent>(event);
    case EventType::ShopBuyQuote:
        return inspect_as<BeforeShopPurchaseEvent>(event);
    case EventType::ShopBuyAfter:
        return inspect_as<ShopPurchasedEvent>(event);
    case EventType::ShopSellQuote:
        return inspect_as<BeforeShopSaleEvent>(event);
    case EventType::ShopSellAfter:
        return inspect_as<ShopSoldEvent>(event);
    case EventType::ShopLeave:
        return inspect_as<ShopLeftEvent>(event);
    case EventType::RoundStart:
        return inspect_as<RoundStartedEvent>(event);
    case EventType::InitialDealBefore:
        return inspect_as<BeforeInitialDealEvent>(event);
    case EventType::InitialDealAfter:
        return inspect_as<InitialCardsDealtEvent>(event);
    case EventType::TrickStart:
        return inspect_as<TrickStartedEvent>(event);
    case EventType::TurnStart:
        return inspect_as<TurnStartedEvent>(event);
    case EventType::PlayActionWindow:
        return inspect_as<TurnAbilityActivationEvent>(event);
    case EventType::PlayValidate:
        return inspect_as<PlayValidationEvent>(event);
    case EventType::PlayCommitted:
        return inspect_as<CardsPlayedEvent>(event);
    case EventType::PlayRewardBefore:
        return inspect_as<BeforePlayRewardEvent>(event);
    case EventType::PlayRewardAfter:
        return inspect_as<PlayRewardGrantedEvent>(event);
    case EventType::PassCommitted:
        return inspect_as<PlayerPassedEvent>(event);
    case EventType::TurnEnd:
        return inspect_as<TurnEndedEvent>(event);
    case EventType::TrickEnd:
        return inspect_as<TrickEndedEvent>(event);
    case EventType::DrawBefore:
        return inspect_as<BeforeCardsDrawEvent>(event);
    case EventType::DrawAfter:
        return inspect_as<CardsDrawnEvent>(event);
    case EventType::TrickDrawsAfter:
        return inspect_as<TrickDrawingCompletedEvent>(event);
    case EventType::RoundEnd:
        return inspect_as<RoundEndedEvent>(event);
    case EventType::RoundRewardBefore:
        return inspect_as<BeforeRoundRewardEvent>(event);
    case EventType::RoundRewardAfter:
        return inspect_as<RoundRewardsGrantedEvent>(event);
    case EventType::GameEnd:
        return inspect_as<GameEndedEvent>(event);
    case EventType::CardChanged:
        return inspect_as<PlayingCardsChangedEvent>(event);
    case EventType::AbilityResolved:
        return inspect_as<AbilityResolvedEvent>(event);
    case EventType::FunctionCardEquipped:
        return inspect_as<FunctionCardEquippedEvent>(event);
    case EventType::FunctionCardRemoved:
        return inspect_as<FunctionCardRemovedEvent>(event);
    case EventType::ShopSlotLocked:
        return inspect_as<ShopSlotLockedEvent>(event);
    case EventType::ShopSlotUnlocked:
        return inspect_as<ShopSlotUnlockedEvent>(event);
    case EventType::BoosterPackOpened:
        return inspect_as<BoosterPackOpenedEvent>(event);
    case EventType::BoosterPackChoiceCompleted:
        return inspect_as<BoosterPackChoiceCompletedEvent>(event);
    case EventType::BoosterPackSkipped:
        return inspect_as<BoosterPackSkippedEvent>(event);
    case EventType::ExpansionCardUsed:
        return inspect_as<ExpansionCardUsedEvent>(event);
    case EventType::HandPatternUpgraded:
        return inspect_as<HandPatternUpgradedEvent>(event);
    case EventType::FacilityPurchased:
        return inspect_as<FacilityPurchasedEvent>(event);
    case EventType::CoinsChanged:
        return inspect_as<CoinsChangedEvent>(event);
    case EventType::ScoreChanged:
        return inspect_as<ScoreChangedEvent>(event);
    case EventType::PlayerStatusChanged:
        return inspect_as<PlayerStatusChangedEvent>(event);
    case EventType::AbilityAdded:
        return inspect_as<AbilityAddedEvent>(event);
    case EventType::AbilityRemoved:
        return inspect_as<AbilityRemovedEvent>(event);
    case EventType::AbilityAbolished:
        return inspect_as<AbilityAbolishedEvent>(event);
    case EventType::AbilityUsabilityChanged:
        return inspect_as<AbilityUsabilityChangedEvent>(event);
    case EventType::ChoiceOpened:
        return inspect_as<ChoiceOpenedEvent>(event);
    case EventType::ChoiceResolved:
        return inspect_as<ChoiceResolvedEvent>(event);
    case EventType::ChoiceExpired:
        return inspect_as<ChoiceExpiredEvent>(event);
    case EventType::RoundWinnerLocked:
        return inspect_as<RoundWinnerLockedEvent>(event);
    case EventType::DrawPileExhausted:
        return inspect_as<DrawPileExhaustedEvent>(event);
    case EventType::AllShopsClosed:
        return inspect_as<AllShopsClosedEvent>(event);
    case EventType::FunctionCardUsabilityChanged:
        return inspect_as<FunctionCardUsabilityChangedEvent>(event);
    case EventType::FunctionCardAbolished:
        return inspect_as<FunctionCardAbolishedEvent>(event);
    case EventType::AbilityStateChanged:
        return inspect_as<AbilityStateChangedEvent>(event);
    case EventType::DelayedEffectScheduled:
        return inspect_as<DelayedEffectScheduledEvent>(event);
    case EventType::DelayedEffectTriggered:
        return inspect_as<DelayedEffectTriggeredEvent>(event);
    case EventType::DelayedEffectExpired:
        return inspect_as<DelayedEffectExpiredEvent>(event);
    case EventType::CoatingApplied:
        return inspect_as<CoatingAppliedEvent>(event);
    case EventType::CardVisibilityChanged:
        return inspect_as<CardVisibilityChangedEvent>(event);
    case EventType::AddScore:
        return inspect_as<AddScoreEvent>(event);
    case EventType::AddCoins:
        return inspect_as<AddCoinsEvent>(event);
    case EventType::SpendCoins:
        return inspect_as<SpendCoinsEvent>(event);
    case EventType::AddPendingScore:
        return inspect_as<AddPendingScoreEvent>(event);
    case EventType::MultiplyPendingScore:
        return inspect_as<MultiplyPendingScoreEvent>(event);
    case EventType::AddPendingCoins:
        return inspect_as<AddPendingCoinsEvent>(event);
    case EventType::ReduceShopCost:
        return inspect_as<ReduceShopCostEvent>(event);
    case EventType::GrantFreeRefresh:
        return inspect_as<GrantFreeRefreshEvent>(event);
    case EventType::RefreshShop:
        return inspect_as<RefreshShopEvent>(event);
    case EventType::OpenBoosterPack:
        return inspect_as<OpenBoosterPackEvent>(event);
    case EventType::LockShopSlot:
        return inspect_as<LockShopSlotEvent>(event);
    case EventType::UnlockShopSlot:
        return inspect_as<UnlockShopSlotEvent>(event);
    case EventType::GrantFunctionCard:
        return inspect_as<GrantFunctionCardEvent>(event);
    case EventType::RemoveFunctionCard:
        return inspect_as<RemoveFunctionCardEvent>(event);
    case EventType::ReplaceFunctionCard:
        return inspect_as<ReplaceFunctionCardEvent>(event);
    case EventType::AddAbility:
        return inspect_as<AddAbilityEvent>(event);
    case EventType::RemoveAbility:
        return inspect_as<RemoveAbilityEvent>(event);
    case EventType::AbolishAbility:
        return inspect_as<AbolishAbilityEvent>(event);
    case EventType::SetAbilityUsable:
        return inspect_as<SetAbilityUsableEvent>(event);
    case EventType::SetFunctionCardUsable:
        return inspect_as<SetFunctionCardUsableEvent>(event);
    case EventType::AbolishFunctionCard:
        return inspect_as<AbolishFunctionCardEvent>(event);
    case EventType::ApplyCoating:
        return inspect_as<ApplyCoatingEvent>(event);
    case EventType::DrawCards:
        return inspect_as<DrawCardsEvent>(event);
    case EventType::SwapCardsWithDeck:
        return inspect_as<SwapCardsWithDeckEvent>(event);
    case EventType::ChangeCardRank:
        return inspect_as<ChangeCardRankEvent>(event);
    case EventType::ChangeCardSuit:
        return inspect_as<ChangeCardSuitEvent>(event);
    case EventType::MakeCardJoker:
        return inspect_as<MakeCardJokerEvent>(event);
    case EventType::RevealCards:
        return inspect_as<RevealCardsEvent>(event);
    case EventType::ApplyPlayerStatus:
        return inspect_as<ApplyPlayerStatusEvent>(event);
    case EventType::RemovePlayerStatus:
        return inspect_as<RemovePlayerStatusEvent>(event);
    case EventType::UpgradeHandPattern:
        return inspect_as<UpgradeHandPatternEvent>(event);
    case EventType::PurchaseFacility:
        return inspect_as<PurchaseFacilityEvent>(event);
    case EventType::UseExpansionCard:
        return inspect_as<UseExpansionCardEvent>(event);
    case EventType::AddDelayedEffect:
        return inspect_as<AddDelayedEffectEvent>(event);
    case EventType::OpenChoice:
        return inspect_as<OpenChoiceEvent>(event);
    case EventType::ResolveChoice:
        return inspect_as<ResolveChoiceEvent>(event);
    case EventType::ChangeAbilityCounter:
        return inspect_as<ChangeAbilityCounterEvent>(event);
    case EventType::EventBatch:
        return inspect_as<EventBatch>(event);
    case EventType::PlayerJoinedControl:
        return inspect_as<PlayerJoinedControl>(event);
    case EventType::PlayerDisconnectedControl:
        return inspect_as<PlayerDisconnectedControl>(event);
    case EventType::PlayerReconnectedControl:
        return inspect_as<PlayerReconnectedControl>(event);
    case EventType::ShoppingDeadlineControl:
        return inspect_as<ShoppingDeadlineControl>(event);
    case EventType::TurnDeadlineControl:
        return inspect_as<TurnDeadlineControl>(event);
    case EventType::ChoiceDeadlineControl:
        return inspect_as<ChoiceDeadlineControl>(event);
    case EventType::RequestRejectedControl:
        return inspect_as<RequestRejectedControl>(event);
    case EventType::StateSnapshotReadyControl:
        return inspect_as<StateSnapshotReadyControl>(event);
    case EventType::PlayOptionsReadyControl:
        return inspect_as<PlayOptionsReadyControl>(event);
    case EventType::ShopQuoteReadyControl:
        return inspect_as<ShopQuoteReadyControl>(event);
    case EventType::ConsumableUsed: return inspect_as<ConsumableUsedEvent>(event);
    case EventType::ShopSlotsChanged: return inspect_as<ShopSlotsChangedEvent>(event);
    case EventType::ShopLevelChanged: return inspect_as<ShopLevelChangedEvent>(event);
    case EventType::FunctionCardEnhanced: return inspect_as<FunctionCardEnhancedEvent>(event);
    default:
        throw std::invalid_argument("Unknown EventType");
    }
}

}  // namespace

const Context& Condition::on_event(EventPtr event, DispatchMeta meta) {
    if (!event) {
        throw std::invalid_argument("Condition requires an event");
    }

    // 在修改 Context 前完成所有可能失败的结构检查。
    const auto snapshots = inspect_event(*event);
    context_.event_ = std::move(event);
    context_.metadata_ = std::move(meta);
    context_.receiver_.reset();
    context_.reward_ = snapshots.reward;
    context_.shop_ = snapshots.shop;
    return context_;
}

void Condition::require_event() const {
    if (!context_.has_event()) {
        throw std::logic_error("Context has no current event");
    }
}

void Condition::set_receiver(std::optional<AbilityRef> receiver) {
    require_event();
    context_.receiver_ = std::move(receiver);
}

void Condition::set_reward(std::optional<RewardData> reward) {
    require_event();
    context_.reward_ = std::move(reward);
}

void Condition::set_shop(std::optional<ShopData> shop) {
    require_event();
    context_.shop_ = std::move(shop);
}

void Condition::clear_context() noexcept {
    context_.event_.reset();
    context_.metadata_ = {};
    context_.receiver_.reset();
    context_.reward_.reset();
    context_.shop_.reset();
}

ContextFrame Condition::take_context() noexcept {
    ContextFrame frame{std::move(context_.event_), context_.metadata_,
        context_.receiver_, context_.reward_, context_.shop_};
    clear_context();
    return frame;
}

const Context& Condition::restore_context(ContextFrame frame) {
    if (!frame.event) throw std::invalid_argument("Cannot restore empty context");
    (void)inspect_event(*frame.event);
    context_.event_ = std::move(frame.event);
    context_.metadata_ = std::move(frame.metadata);
    context_.receiver_ = frame.receiver;
    context_.reward_ = frame.reward;
    context_.shop_ = frame.shop;
    return context_;
}

}  // namespace cardgame

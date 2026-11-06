#include "network/ListenEvent.hpp"

#include <exception>

namespace cardgame::network {

ListenEvent::ListenEvent(EventReceiver receiver)
    : state_(std::make_shared<State>(std::move(receiver))) {
    if (!state_->receiver) {
        throw std::invalid_argument("ListenEvent requires a receiver");
    }
}

ListenEvent::~ListenEvent() noexcept {
    try {
        stop();
    } catch (...) {
        // 析构不能向外抛异常；需要报告取消错误的调用方应显式 stop()。
    }
}

EventPtr ListenEvent::create_event(EventType type,
                                  std::optional<Players> actor) {
    EventPtr event;
    // 明确的类型白名单；不根据序号偏移、typeid 哈希或客户端名称猜测类型。
    switch (type) {
    case EventType::BuyShopItemRequest:
        event = std::make_unique<BuyShopItemRequest>();
        break;
    case EventType::BuyBoosterPackRequest:
        event = std::make_unique<BuyBoosterPackRequest>();
        break;
    case EventType::SellFunctionCardRequest:
        event = std::make_unique<SellFunctionCardRequest>();
        break;
    case EventType::RefreshShopRequest:
        event = std::make_unique<RefreshShopRequest>();
        break;
    case EventType::LockShopSlotRequest:
        event = std::make_unique<LockShopSlotRequest>();
        break;
    case EventType::UnlockShopSlotRequest:
        event = std::make_unique<UnlockShopSlotRequest>();
        break;
    case EventType::ChooseBoosterCardRequest:
        event = std::make_unique<ChooseBoosterCardRequest>();
        break;
    case EventType::SkipBoosterPackRequest:
        event = std::make_unique<SkipBoosterPackRequest>();
        break;
    case EventType::UpgradeHandPatternRequest:
        event = std::make_unique<UpgradeHandPatternRequest>();
        break;
    case EventType::BuyFacilityRequest:
        event = std::make_unique<BuyFacilityRequest>();
        break;
    case EventType::FinishShoppingRequest:
        event = std::make_unique<FinishShoppingRequest>();
        break;
    case EventType::PlayCardsRequest:
        event = std::make_unique<PlayCardsRequest>();
        break;
    case EventType::PassRequest:
        event = std::make_unique<PassRequest>();
        break;
    case EventType::ActivateAbilityRequest:
        event = std::make_unique<ActivateAbilityRequest>();
        break;
    case EventType::SubmitChoiceRequest:
        event = std::make_unique<SubmitChoiceRequest>();
        break;
    case EventType::ResolveWildcardRequest:
        event = std::make_unique<ResolveWildcardRequest>();
        break;
    case EventType::ContinueGameRequest:
        event = std::make_unique<ContinueGameRequest>();
        break;
    case EventType::StartNewGameRequest:
        event = std::make_unique<StartNewGameRequest>();
        break;
    case EventType::StateSyncRequest:
        event = std::make_unique<StateSyncRequest>();
        break;
    case EventType::PreviewPlayRequest:
        event = std::make_unique<PreviewPlayRequest>();
        break;
    case EventType::QuoteShopOperationRequest:
        event = std::make_unique<QuoteShopOperationRequest>();
        break;
    case EventType::GameStart:
        event = std::make_unique<GameStartedEvent>();
        break;
    case EventType::ShopEnter:
        event = std::make_unique<ShopEnteredEvent>();
        break;
    case EventType::ShopActionWindow:
        event = std::make_unique<ShopAbilityActivationEvent>();
        break;
    case EventType::ShopRefreshQuote:
        event = std::make_unique<BeforeShopRefreshEvent>();
        break;
    case EventType::ShopRefreshAfter:
        event = std::make_unique<ShopRefreshedEvent>();
        break;
    case EventType::ShopBuyQuote:
        event = std::make_unique<BeforeShopPurchaseEvent>();
        break;
    case EventType::ShopBuyAfter:
        event = std::make_unique<ShopPurchasedEvent>();
        break;
    case EventType::ShopSellQuote:
        event = std::make_unique<BeforeShopSaleEvent>();
        break;
    case EventType::ShopSellAfter:
        event = std::make_unique<ShopSoldEvent>();
        break;
    case EventType::ShopLeave:
        event = std::make_unique<ShopLeftEvent>();
        break;
    case EventType::RoundStart:
        event = std::make_unique<RoundStartedEvent>();
        break;
    case EventType::InitialDealBefore:
        event = std::make_unique<BeforeInitialDealEvent>();
        break;
    case EventType::InitialDealAfter:
        event = std::make_unique<InitialCardsDealtEvent>();
        break;
    case EventType::TrickStart:
        event = std::make_unique<TrickStartedEvent>();
        break;
    case EventType::TurnStart:
        event = std::make_unique<TurnStartedEvent>();
        break;
    case EventType::PlayActionWindow:
        event = std::make_unique<TurnAbilityActivationEvent>();
        break;
    case EventType::PlayValidate:
        event = std::make_unique<PlayValidationEvent>();
        break;
    case EventType::PlayCommitted:
        event = std::make_unique<CardsPlayedEvent>();
        break;
    case EventType::PlayRewardBefore:
        event = std::make_unique<BeforePlayRewardEvent>();
        break;
    case EventType::PlayRewardAfter:
        event = std::make_unique<PlayRewardGrantedEvent>();
        break;
    case EventType::PassCommitted:
        event = std::make_unique<PlayerPassedEvent>();
        break;
    case EventType::TurnEnd:
        event = std::make_unique<TurnEndedEvent>();
        break;
    case EventType::TrickEnd:
        event = std::make_unique<TrickEndedEvent>();
        break;
    case EventType::DrawBefore:
        event = std::make_unique<BeforeCardsDrawEvent>();
        break;
    case EventType::DrawAfter:
        event = std::make_unique<CardsDrawnEvent>();
        break;
    case EventType::TrickDrawsAfter:
        event = std::make_unique<TrickDrawingCompletedEvent>();
        break;
    case EventType::RoundEnd:
        event = std::make_unique<RoundEndedEvent>();
        break;
    case EventType::RoundRewardBefore:
        event = std::make_unique<BeforeRoundRewardEvent>();
        break;
    case EventType::RoundRewardAfter:
        event = std::make_unique<RoundRewardsGrantedEvent>();
        break;
    case EventType::GameEnd:
        event = std::make_unique<GameEndedEvent>();
        break;
    case EventType::CardChanged:
        event = std::make_unique<PlayingCardsChangedEvent>();
        break;
    case EventType::AbilityResolved:
        event = std::make_unique<AbilityResolvedEvent>();
        break;
    case EventType::FunctionCardEquipped:
        event = std::make_unique<FunctionCardEquippedEvent>();
        break;
    case EventType::FunctionCardRemoved:
        event = std::make_unique<FunctionCardRemovedEvent>();
        break;
    case EventType::ShopSlotLocked:
        event = std::make_unique<ShopSlotLockedEvent>();
        break;
    case EventType::ShopSlotUnlocked:
        event = std::make_unique<ShopSlotUnlockedEvent>();
        break;
    case EventType::BoosterPackOpened:
        event = std::make_unique<BoosterPackOpenedEvent>();
        break;
    case EventType::BoosterPackChoiceCompleted:
        event = std::make_unique<BoosterPackChoiceCompletedEvent>();
        break;
    case EventType::BoosterPackSkipped:
        event = std::make_unique<BoosterPackSkippedEvent>();
        break;
    case EventType::ExpansionCardUsed:
        event = std::make_unique<ExpansionCardUsedEvent>();
        break;
    case EventType::HandPatternUpgraded:
        event = std::make_unique<HandPatternUpgradedEvent>();
        break;
    case EventType::FacilityPurchased:
        event = std::make_unique<FacilityPurchasedEvent>();
        break;
    case EventType::CoinsChanged:
        event = std::make_unique<CoinsChangedEvent>();
        break;
    case EventType::ScoreChanged:
        event = std::make_unique<ScoreChangedEvent>();
        break;
    case EventType::PlayerStatusChanged:
        event = std::make_unique<PlayerStatusChangedEvent>();
        break;
    case EventType::AbilityAdded:
        event = std::make_unique<AbilityAddedEvent>();
        break;
    case EventType::AbilityRemoved:
        event = std::make_unique<AbilityRemovedEvent>();
        break;
    case EventType::AbilityAbolished:
        event = std::make_unique<AbilityAbolishedEvent>();
        break;
    case EventType::AbilityUsabilityChanged:
        event = std::make_unique<AbilityUsabilityChangedEvent>();
        break;
    case EventType::ChoiceOpened:
        event = std::make_unique<ChoiceOpenedEvent>();
        break;
    case EventType::ChoiceResolved:
        event = std::make_unique<ChoiceResolvedEvent>();
        break;
    case EventType::ChoiceExpired:
        event = std::make_unique<ChoiceExpiredEvent>();
        break;
    case EventType::RoundWinnerLocked:
        event = std::make_unique<RoundWinnerLockedEvent>();
        break;
    case EventType::DrawPileExhausted:
        event = std::make_unique<DrawPileExhaustedEvent>();
        break;
    case EventType::AllShopsClosed:
        event = std::make_unique<AllShopsClosedEvent>();
        break;
    case EventType::FunctionCardUsabilityChanged:
        event = std::make_unique<FunctionCardUsabilityChangedEvent>();
        break;
    case EventType::FunctionCardAbolished:
        event = std::make_unique<FunctionCardAbolishedEvent>();
        break;
    case EventType::AbilityStateChanged:
        event = std::make_unique<AbilityStateChangedEvent>();
        break;
    case EventType::DelayedEffectScheduled:
        event = std::make_unique<DelayedEffectScheduledEvent>();
        break;
    case EventType::DelayedEffectTriggered:
        event = std::make_unique<DelayedEffectTriggeredEvent>();
        break;
    case EventType::DelayedEffectExpired:
        event = std::make_unique<DelayedEffectExpiredEvent>();
        break;
    case EventType::CoatingApplied:
        event = std::make_unique<CoatingAppliedEvent>();
        break;
    case EventType::CardVisibilityChanged:
        event = std::make_unique<CardVisibilityChangedEvent>();
        break;
    case EventType::AddScore:
        event = std::make_unique<AddScoreEvent>();
        break;
    case EventType::AddCoins:
        event = std::make_unique<AddCoinsEvent>();
        break;
    case EventType::SpendCoins:
        event = std::make_unique<SpendCoinsEvent>();
        break;
    case EventType::AddPendingScore:
        event = std::make_unique<AddPendingScoreEvent>();
        break;
    case EventType::MultiplyPendingScore:
        event = std::make_unique<MultiplyPendingScoreEvent>();
        break;
    case EventType::AddPendingCoins:
        event = std::make_unique<AddPendingCoinsEvent>();
        break;
    case EventType::ReduceShopCost:
        event = std::make_unique<ReduceShopCostEvent>();
        break;
    case EventType::GrantFreeRefresh:
        event = std::make_unique<GrantFreeRefreshEvent>();
        break;
    case EventType::RefreshShop:
        event = std::make_unique<RefreshShopEvent>();
        break;
    case EventType::OpenBoosterPack:
        event = std::make_unique<OpenBoosterPackEvent>();
        break;
    case EventType::LockShopSlot:
        event = std::make_unique<LockShopSlotEvent>();
        break;
    case EventType::UnlockShopSlot:
        event = std::make_unique<UnlockShopSlotEvent>();
        break;
    case EventType::GrantFunctionCard:
        event = std::make_unique<GrantFunctionCardEvent>();
        break;
    case EventType::RemoveFunctionCard:
        event = std::make_unique<RemoveFunctionCardEvent>();
        break;
    case EventType::ReplaceFunctionCard:
        event = std::make_unique<ReplaceFunctionCardEvent>();
        break;
    case EventType::AddAbility:
        event = std::make_unique<AddAbilityEvent>();
        break;
    case EventType::RemoveAbility:
        event = std::make_unique<RemoveAbilityEvent>();
        break;
    case EventType::AbolishAbility:
        event = std::make_unique<AbolishAbilityEvent>();
        break;
    case EventType::SetAbilityUsable:
        event = std::make_unique<SetAbilityUsableEvent>();
        break;
    case EventType::SetFunctionCardUsable:
        event = std::make_unique<SetFunctionCardUsableEvent>();
        break;
    case EventType::AbolishFunctionCard:
        event = std::make_unique<AbolishFunctionCardEvent>();
        break;
    case EventType::ApplyCoating:
        event = std::make_unique<ApplyCoatingEvent>();
        break;
    case EventType::DrawCards:
        event = std::make_unique<DrawCardsEvent>();
        break;
    case EventType::SwapCardsWithDeck:
        event = std::make_unique<SwapCardsWithDeckEvent>();
        break;
    case EventType::ChangeCardRank:
        event = std::make_unique<ChangeCardRankEvent>();
        break;
    case EventType::ChangeCardSuit:
        event = std::make_unique<ChangeCardSuitEvent>();
        break;
    case EventType::MakeCardJoker:
        event = std::make_unique<MakeCardJokerEvent>();
        break;
    case EventType::RevealCards:
        event = std::make_unique<RevealCardsEvent>();
        break;
    case EventType::ApplyPlayerStatus:
        event = std::make_unique<ApplyPlayerStatusEvent>();
        break;
    case EventType::RemovePlayerStatus:
        event = std::make_unique<RemovePlayerStatusEvent>();
        break;
    case EventType::UpgradeHandPattern:
        event = std::make_unique<UpgradeHandPatternEvent>();
        break;
    case EventType::PurchaseFacility:
        event = std::make_unique<PurchaseFacilityEvent>();
        break;
    case EventType::UseExpansionCard:
        event = std::make_unique<UseExpansionCardEvent>();
        break;
    case EventType::AddDelayedEffect:
        event = std::make_unique<AddDelayedEffectEvent>();
        break;
    case EventType::OpenChoice:
        event = std::make_unique<OpenChoiceEvent>();
        break;
    case EventType::ResolveChoice:
        event = std::make_unique<ResolveChoiceEvent>();
        break;
    case EventType::ChangeAbilityCounter:
        event = std::make_unique<ChangeAbilityCounterEvent>();
        break;
    case EventType::EventBatch:
        event = std::make_unique<EventBatch>();
        break;
    case EventType::PlayerJoinedControl:
        event = std::make_unique<PlayerJoinedControl>();
        break;
    case EventType::PlayerDisconnectedControl:
        event = std::make_unique<PlayerDisconnectedControl>();
        break;
    case EventType::PlayerReconnectedControl:
        event = std::make_unique<PlayerReconnectedControl>();
        break;
    case EventType::ShoppingDeadlineControl:
        event = std::make_unique<ShoppingDeadlineControl>();
        break;
    case EventType::TurnDeadlineControl:
        event = std::make_unique<TurnDeadlineControl>();
        break;
    case EventType::ChoiceDeadlineControl:
        event = std::make_unique<ChoiceDeadlineControl>();
        break;
    case EventType::RequestRejectedControl:
        event = std::make_unique<RequestRejectedControl>();
        break;
    case EventType::StateSnapshotReadyControl:
        event = std::make_unique<StateSnapshotReadyControl>();
        break;
    case EventType::PlayOptionsReadyControl:
        event = std::make_unique<PlayOptionsReadyControl>();
        break;
    case EventType::ShopQuoteReadyControl:
        event = std::make_unique<ShopQuoteReadyControl>();
        break;
    default:
        throw std::invalid_argument("Unknown EventType");
    }
    event->actor = actor;
    return event;
}

bool ListenEvent::attach(Registration registration) {
    try {
        std::lock_guard<std::mutex> lock(registrationsMutex_);
        if (state_->active.load(std::memory_order_acquire)) {
            registrations_.push_back(std::move(registration));
            return true;
        }
    } catch (...) {
        // 保存失败时撤销已建立的来源订阅，并保留原始异常。
        registration.enabled->store(false, std::memory_order_release);
        try {
            registration.disconnect();
        } catch (...) {
        }
        throw;
    }

    // subscribe 可能立即触发回调，并在回调中停止本监听器。
    registration.enabled->store(false, std::memory_order_release);
    registration.disconnect();
    return false;
}

void ListenEvent::stop() {
    if (!state_->active.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    std::vector<Registration> removed;
    {
        std::lock_guard<std::mutex> lock(registrationsMutex_);
        removed.swap(registrations_);
    }

    // 调用外部取消函数前释放互斥锁，允许取消过程重入监听器。
    std::exception_ptr firstError;
    for (auto& registration : removed) {
        registration.enabled->store(false, std::memory_order_release);
        try {
            registration.disconnect();
        } catch (...) {
            if (!firstError) {
                firstError = std::current_exception();
            }
        }
    }
    if (firstError) {
        std::rethrow_exception(firstError);
    }
}

bool ListenEvent::listening() const noexcept {
    return state_->active.load(std::memory_order_acquire);
}

std::size_t ListenEvent::subscription_count() const {
    std::lock_guard<std::mutex> lock(registrationsMutex_);
    return registrations_.size();
}

}  // namespace cardgame::network

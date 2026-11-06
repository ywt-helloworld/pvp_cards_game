#pragma once
#include "Player.hpp"
#include "Register.hpp"
#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
namespace cardgame::abilities {
/// 原子技能库：不同操作使用不同参数；没有 Ability 基类或统一 call 接口。
/// 每个操作只改变一种资源、属性或执行一个业务事务。返回事实通知，不递归派发。
EventPtr add_score(Player& player, ScoreValue amount);
EventPtr subtract_score(Player& player, ScoreValue amount);
EventPtr set_score(Player& player, ScoreValue value);
EventPtr gain_coins(Player& player, CoinValue amount, std::string reason = {});
EventPtr spend_coins(Player& player, CoinValue amount, std::string reason = {});
EventPtr set_coins(Player& player, CoinValue value, std::string reason = {});
bool add_pending_score(RewardData& reward, ScoreValue delta);
bool multiply_pending_score(RewardData& reward, ScoreValue numerator, ScoreValue denominator,
                            RoundingRule rounding = RoundingRule::Floor);
bool add_pending_coins(CoinValue& component, CoinValue delta);
bool reduce_shop_cost(ShopData& quote, CoinValue amount, CoinValue minimum = 0);
bool grant_free_refresh(std::size_t& credits, std::size_t count);
bool grant_function_slot(Player& player);
EventPtr add_ability_counter(AbilityRef ability, CounterId counter, int& value, int delta);
EventPtr set_ability_counter(AbilityRef ability, CounterId counter, int& value, int replacement);
EventPtr set_ability_usable(AbilityRef ability, bool& usable, bool replacement);
EventPtr set_function_card_usable(FunctionCards& card, bool usable);
EventPtr abolish_function_card(FunctionCards& card);
EventPtr equip_function_card(Register& registry, Player& player, std::size_t slot, CardPtr card);
EventPtr remove_function_card(Register& registry, Players player, std::size_t slot,
                              RemovalReason reason);
EventPtr deal_initial_cards(RoundId round, const PlayerOrder& players, int count,
                           const InitialDealOperation& deal);

namespace detail {
/// op 必须原子提交并填写确认后的事实；false 表示未执行，异常由调用方处理。
/// 通知对象在业务提交前分配，避免提交之后才分配通知。
template<class Fact, class Operation, class... Args>
EventPtr commit(Operation&& operation, Args&&... args) {
    auto fact = std::make_unique<Fact>();
    if (!std::invoke(std::forward<Operation>(operation),
                     std::forward<Args>(args)..., *fact)) return {};
    return fact;
}
void validate_players(const PlayerOrder& players);
}
/// 刷新未锁定的货架；业务回调签名为 bool(Players, ShopId, RefreshMode, ShopRefreshedEvent&).
template<class Operation>
EventPtr refresh_shop(Players player, ShopId shop, RefreshMode mode, Operation&& operation) {
    return detail::commit<ShopRefreshedEvent>(std::forward<Operation>(operation), player, shop, mode);
}
/// 打开已获准的卡包；业务回调签名为 bool(Players, ShopId, PackId, BoosterPackOpenedEvent&).
template<class Operation>
EventPtr open_booster_pack(Players player, ShopId shop, PackId pack, Operation&& operation) {
    return detail::commit<BoosterPackOpenedEvent>(std::forward<Operation>(operation), player, shop, pack);
}
/// 锁定一个货位；业务回调签名为 bool(Players, ShopId, SlotId, ShopSlotLockedEvent&).
template<class Operation>
EventPtr lock_shop_slot(Players player, ShopId shop, SlotId slot, Operation&& operation) {
    return detail::commit<ShopSlotLockedEvent>(std::forward<Operation>(operation), player, shop, slot);
}
/// 解锁一个货位；业务回调签名为 bool(Players, ShopId, SlotId, ShopSlotUnlockedEvent&).
template<class Operation>
EventPtr unlock_shop_slot(Players player, ShopId shop, SlotId slot, Operation&& operation) {
    return detail::commit<ShopSlotUnlockedEvent>(std::forward<Operation>(operation), player, shop, slot);
}
/// 添加一个技能定义；业务回调签名为 bool(CardRef, DefinitionId, AbilityAddedEvent&).
template<class Operation>
EventPtr add_ability(CardRef card, DefinitionId definition, Operation&& operation) {
    return detail::commit<AbilityAddedEvent>(std::forward<Operation>(operation), card, definition);
}
/// 移除一个技能；业务回调签名为 bool(AbilityRef, AbilityRemovedEvent&).
template<class Operation>
EventPtr remove_ability(AbilityRef ability, Operation&& operation) {
    return detail::commit<AbilityRemovedEvent>(std::forward<Operation>(operation), ability);
}
/// 永久废除一个技能；业务回调签名为 bool(AbilityRef, AbilityAbolishedEvent&).
template<class Operation>
EventPtr abolish_ability(AbilityRef ability, Operation&& operation) {
    return detail::commit<AbilityAbolishedEvent>(std::forward<Operation>(operation), ability);
}
/// 施加一种镀层；业务回调签名为 bool(CardRef, CoatingType, CoatingAppliedEvent&).
template<class Operation>
EventPtr apply_coating(CardRef card, CoatingType coating, Operation&& operation) {
    return detail::commit<CoatingAppliedEvent>(std::forward<Operation>(operation), card, coating);
}
/// 给一个玩家摸牌；业务回调签名为 bool(Players, std::size_t, DrawReason, CardsDrawnEvent&).
template<class Operation>
EventPtr draw_cards(Players player, std::size_t count, DrawReason reason, Operation&& operation) {
    return detail::commit<CardsDrawnEvent>(std::forward<Operation>(operation), player, count, reason);
}
/// 交换指定手牌与牌堆顶牌；业务回调签名为 bool(Players, const std::vector<CardId>&, PlayingCardsChangedEvent&).
template<class Operation>
EventPtr swap_cards_with_deck(Players player, const std::vector<CardId>& cards, Operation&& operation) {
    return detail::commit<PlayingCardsChangedEvent>(std::forward<Operation>(operation), player, cards);
}
/// 修改一张牌的点数；业务回调签名为 bool(CardId, int, Lifetime, PlayingCardsChangedEvent&).
template<class Operation>
EventPtr change_card_rank(CardId card, int rank, Lifetime lifetime, Operation&& operation) {
    return detail::commit<PlayingCardsChangedEvent>(std::forward<Operation>(operation), card, rank, lifetime);
}
/// 修改一张牌的花色；业务回调签名为 bool(CardId, Suit, Lifetime, PlayingCardsChangedEvent&).
template<class Operation>
EventPtr change_card_suit(CardId card, Suit suit, Lifetime lifetime, Operation&& operation) {
    return detail::commit<PlayingCardsChangedEvent>(std::forward<Operation>(operation), card, suit, lifetime);
}
/// 将一张牌临时变为赖子；业务回调签名为 bool(CardId, Lifetime, PlayingCardsChangedEvent&).
template<class Operation>
EventPtr make_card_joker(CardId card, Lifetime lifetime, Operation&& operation) {
    return detail::commit<PlayingCardsChangedEvent>(std::forward<Operation>(operation), card, lifetime);
}
/// 清除临时赖子标记；业务回调签名为 bool(CardId, PlayingCardsChangedEvent&).
template<class Operation>
EventPtr clear_card_joker(CardId card, Operation&& operation) {
    return detail::commit<PlayingCardsChangedEvent>(std::forward<Operation>(operation), card);
}
/// 授予指定牌的查看权限；业务回调签名为 bool(Players, Players, const std::vector<CardId>&, CardVisibilityChangedEvent&).
template<class Operation>
EventPtr reveal_cards(Players viewer, Players target, const std::vector<CardId>& cards, Operation&& operation) {
    return detail::commit<CardVisibilityChangedEvent>(std::forward<Operation>(operation), viewer, target, cards);
}
/// 施加一个玩家状态；业务回调签名为 bool(Players, DefinitionId, int, Lifetime, PlayerStatusChangedEvent&).
template<class Operation>
EventPtr apply_player_status(Players player, DefinitionId definition, int stacks, Lifetime lifetime, Operation&& operation) {
    return detail::commit<PlayerStatusChangedEvent>(std::forward<Operation>(operation), player, definition, stacks, lifetime);
}
/// 移除一个玩家状态；业务回调签名为 bool(Players, StatusId, PlayerStatusChangedEvent&).
template<class Operation>
EventPtr remove_player_status(Players player, StatusId status, Operation&& operation) {
    return detail::commit<PlayerStatusChangedEvent>(std::forward<Operation>(operation), player, status);
}
/// 修改状态层数；业务回调签名为 bool(Players, StatusId, int, PlayerStatusChangedEvent&).
template<class Operation>
EventPtr add_status_stacks(Players player, StatusId status, int delta, Operation&& operation) {
    return detail::commit<PlayerStatusChangedEvent>(std::forward<Operation>(operation), player, status, delta);
}
/// 加强一种牌型；业务回调签名为 bool(Players, HandPattern, int, HandPatternUpgradedEvent&).
template<class Operation>
EventPtr upgrade_hand_pattern(Players player, HandPattern pattern, int levels, Operation&& operation) {
    return detail::commit<HandPatternUpgradedEvent>(std::forward<Operation>(operation), player, pattern, levels);
}
/// 使一项设施生效；业务回调签名为 bool(Players, DefinitionId, FacilityPurchasedEvent&).
template<class Operation>
EventPtr purchase_facility(Players player, DefinitionId facility, Operation&& operation) {
    return detail::commit<FacilityPurchasedEvent>(std::forward<Operation>(operation), player, facility);
}
/// 建立一个延迟效果标记；业务回调签名为 bool(Players, DefinitionId, EventType, const TargetSpec&, Lifetime, DelayedEffectScheduledEvent&).
template<class Operation>
EventPtr schedule_delayed_effect(Players player, DefinitionId definition, EventType trigger, const TargetSpec& selection, Lifetime lifetime, Operation&& operation) {
    return detail::commit<DelayedEffectScheduledEvent>(std::forward<Operation>(operation), player, definition, trigger, selection, lifetime);
}
/// 触发一个已到期标记；业务回调签名为 bool(Players, DelayedEffectId, DelayedEffectTriggeredEvent&).
template<class Operation>
EventPtr trigger_delayed_effect(Players player, DelayedEffectId effect, Operation&& operation) {
    return detail::commit<DelayedEffectTriggeredEvent>(std::forward<Operation>(operation), player, effect);
}
/// 移除或使一个标记失效；业务回调签名为 bool(Players, DelayedEffectId, DelayedEffectExpiredEvent&).
template<class Operation>
EventPtr cancel_delayed_effect(Players player, DelayedEffectId effect, Operation&& operation) {
    return detail::commit<DelayedEffectExpiredEvent>(std::forward<Operation>(operation), player, effect);
}
/// 建立一次选择交互；业务回调签名为 bool(Players, ChoiceKind, const std::vector<ChoiceOptionId>&, std::uint32_t, ChoiceOpenedEvent&).
template<class Operation>
EventPtr open_choice(Players player, ChoiceKind kind, const std::vector<ChoiceOptionId>& options, std::uint32_t timeoutMs, Operation&& operation) {
    return detail::commit<ChoiceOpenedEvent>(std::forward<Operation>(operation), player, kind, options, timeoutMs);
}
/// 提交一个选择结果；业务回调签名为 bool(ChoiceId, const std::vector<ChoiceOptionId>&, ChoiceReason, ChoiceResolvedEvent&).
template<class Operation>
EventPtr resolve_choice(ChoiceId choice, const std::vector<ChoiceOptionId>& options, ChoiceReason reason, Operation&& operation) {
    return detail::commit<ChoiceResolvedEvent>(std::forward<Operation>(operation), choice, options, reason);
}
/// 使一个选择交互到期；业务回调签名为 bool(ChoiceId, ChoiceExpiredEvent&).
template<class Operation>
EventPtr expire_choice(ChoiceId choice, Operation&& operation) {
    return detail::commit<ChoiceExpiredEvent>(std::forward<Operation>(operation), choice);
}
/// 初始化一局的流程状态；业务回调签名为 bool(RoundId, Players, RoundStartedEvent&).
template<class Operation>
EventPtr begin_round(RoundId round, Players first, Operation&& operation) {
    return detail::commit<RoundStartedEvent>(std::forward<Operation>(operation), round, first);
}
/// 开始一巡；业务回调签名为 bool(RoundId, TrickId, Players, TrickStartedEvent&).
template<class Operation>
EventPtr begin_trick(RoundId round, TrickId trick, Players leader, Operation&& operation) {
    return detail::commit<TrickStartedEvent>(std::forward<Operation>(operation), round, trick, leader);
}
/// 开放一个玩家的行动回合；业务回调签名为 bool(TurnId, Players, TurnStartedEvent&).
template<class Operation>
EventPtr begin_turn(TurnId turn, Players player, Operation&& operation) {
    return detail::commit<TurnStartedEvent>(std::forward<Operation>(operation), turn, player);
}
/// 关闭一个行动回合；业务回调签名为 bool(TurnId, Players, TurnEndedEvent&).
template<class Operation>
EventPtr end_turn(TurnId turn, Players player, Operation&& operation) {
    return detail::commit<TurnEndedEvent>(std::forward<Operation>(operation), turn, player);
}
/// 锁定巡末牌权；业务回调签名为 bool(TrickId, Players, TrickEndedEvent&).
template<class Operation>
EventPtr finish_trick(TrickId trick, Players leader, Operation&& operation) {
    return detail::commit<TrickEndedEvent>(std::forward<Operation>(operation), trick, leader);
}
/// 锁定赢家与剩余手牌快照；业务回调签名为 bool(RoundId, Players, RoundWinnerLockedEvent&).
template<class Operation>
EventPtr lock_round_winner(RoundId round, Players winner, Operation&& operation) {
    return detail::commit<RoundWinnerLockedEvent>(std::forward<Operation>(operation), round, winner);
}
/// 关闭已结束的一局；业务回调签名为 bool(RoundId, RoundResultId, RoundEndedEvent&).
template<class Operation>
EventPtr finish_round(RoundId round, RoundResultId result, Operation&& operation) {
    return detail::commit<RoundEndedEvent>(std::forward<Operation>(operation), round, result);
}
/// 关闭整场游戏；业务回调签名为 bool(GameEndedEvent&).
template<class Operation>
EventPtr finish_game(Operation&& operation) {
    return detail::commit<GameEndedEvent>(std::forward<Operation>(operation));
}
/// 关闭一个玩家的商店会话；业务回调签名为 bool(Players, ShopId, ShopLeftEvent&).
template<class Operation>
EventPtr finish_shopping(Players player, ShopId shop, Operation&& operation) {
    return detail::commit<ShopLeftEvent>(std::forward<Operation>(operation), player, shop);
}
/// 关闭本局全部商店会话；业务回调签名为 bool(RoundId, AllShopsClosedEvent&).
template<class Operation>
EventPtr close_all_shops(RoundId round, Operation&& operation) {
    return detail::commit<AllShopsClosedEvent>(std::forward<Operation>(operation), round);
}
/// 提交已经验证的一手牌；业务回调签名为 bool(Players, TurnId, const std::vector<CardId>&, const std::vector<WildcardAssignment>&, CardsPlayedEvent&).
template<class Operation>
EventPtr commit_play(Players player, TurnId turn, const std::vector<CardId>& cards, const std::vector<WildcardAssignment>& wildcards, Operation&& operation) {
    return detail::commit<CardsPlayedEvent>(std::forward<Operation>(operation), player, turn, cards, wildcards);
}
/// 提交一次合法不出牌；业务回调签名为 bool(Players, TurnId, PassReason, PlayerPassedEvent&).
template<class Operation>
EventPtr commit_pass(Players player, TurnId turn, PassReason reason, Operation&& operation) {
    return detail::commit<PlayerPassedEvent>(std::forward<Operation>(operation), player, turn, reason);
}
/// 整批向参与玩家摸牌。业务回调一次性提交，写入与 players 同序的牌列表。
template<class Operation>
EventList draw_to_all(const PlayerOrder& players, std::size_t count,
                      DrawReason reason, Operation&& operation) {
    detail::validate_players(players);
    if (!count) return {};
    EventList result;
    std::vector<std::vector<CardId>> drawn(players.size());
    result.reserve(players.size());
    for (auto player : players) {
        auto event = std::make_unique<CardsDrawnEvent>();
        event->player = player; event->reason = reason;
        result.push_back(std::move(event));
    }
    if (!std::invoke(std::forward<Operation>(operation), players, count, reason, drawn))
        return {};
    if (drawn.size() != players.size()) throw std::logic_error("Invalid draw result size");
    for (std::size_t i = 0; i < players.size(); ++i)
        static_cast<CardsDrawnEvent&>(*result[i]).cards = std::move(drawn[i]);
    result.erase(std::remove_if(result.begin(), result.end(), [](const auto& event) {
        return static_cast<const CardsDrawnEvent&>(*event).cards.empty();
    }), result.end());
    return result;
}
} // namespace cardgame::abilities

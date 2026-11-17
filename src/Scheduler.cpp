#include "Scheduler.hpp"
#include "CardCatalog.hpp"
#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <cmath>
namespace cardgame {
namespace {
template<class E, class Fill> void emit(CardEffects& out, Fill fill) {
    auto event = std::make_unique<E>(); fill(*event);
    constexpr bool global = std::is_same_v<E, GameStartedEvent> ||
        std::is_same_v<E, RoundStartedEvent> || std::is_same_v<E, BeforeInitialDealEvent> ||
        std::is_same_v<E, TrickStartedEvent> || std::is_same_v<E, TrickEndedEvent> ||
        std::is_same_v<E, TrickDrawingCompletedEvent> || std::is_same_v<E, RoundEndedEvent> ||
        std::is_same_v<E, RoundRewardsGrantedEvent> || std::is_same_v<E, AllShopsClosedEvent> ||
        std::is_same_v<E, GameEndedEvent> || std::is_same_v<E, DrawPileExhaustedEvent>;
    out.emit(std::move(event), 0, !global);
}
DefinitionId candy_family(DefinitionId id) {
    switch (id) {
    case 1: case 7: case 10: return 1;
    case 2: case 8: return 2;
    case 3: case 9: return 3;
    default: return 0;
    }
}
const CoinValue facilityPrices[] = {0,18,30,20,35,14,14,12,12};
const CoinValue upgradePrices[] = {4,6,8,10,10};
std::size_t pattern_index(HandPattern pattern) {
    const auto i = static_cast<std::size_t>(pattern);
    if (i >= 5) throw std::invalid_argument("Unknown hand pattern");
    return i;
}
}
std::size_t Scheduler::index(Players p) {
    const auto i = static_cast<std::size_t>(p);
    if (i >= 4) throw std::invalid_argument("Unknown player");
    return i;
}
Scheduler::Scheduler(std::array<Player, 4> players, SchedulerConfig config)
    : players_(std::move(players)), config_(config), handler_(registry_, condition_, {}, 65536),
      random_(config.seed), cards_(standard_deck()) {
    if (!std::isfinite(config_.consumableChance)||config_.consumableChance<0||config_.consumableChance>1||
        !config_.rounds || config_.shopTime.count() <= 0 ||
        config_.turnTime.count() <= 0 || config_.choiceTime.count() <= 0)
        throw std::invalid_argument("Invalid scheduler configuration");
    if (config_.catalog.empty()) {
        using Pattern = HandPattern;
        struct Reward {
            DefinitionId id; const char* name; CoinValue price; unsigned level;
            ScoreValue score; CoinValue coins; unsigned limit;
            std::optional<Pattern> pattern; bool pass; bool anyPlayer;
        };
        const Reward rewards[] = {
            {1,"大白兔奶糖",4,1,1,0,0,{},false,false},
            {2,"QQ糖",4,1,0,1,0,{},false,false},
            {3,"健达巧克力",6,1,1,1,0,{},false,false},
            {4,"旺仔奶糖",3,1,2,0,2,{},true,false},
            {5,"悠哈悠哈",3,1,0,1,2,{},true,false},
            {6,"健达奇趣蛋",6,2,2,2,1,{},false,false},
            {7,"大白兔奶糖+",7,2,2,0,0,{},false,false},
            {8,"QQ糖+",8,2,0,2,0,{},false,false},
            {9,"健达巧克力+",12,3,2,2,0,{},false,false},
            {10,"大白兔奶糖++",11,3,3,0,0,{},false,false},
            {3010,"双子软糖",4,1,0,2,2,Pattern::Pair,false,false},
            {3011,"彩虹糖带",5,1,3,0,2,Pattern::Straight,false,false},
            {3012,"烟花跳跳糖",6,2,0,2,1,Pattern::Bomb,false,true},
            {3013,"爆心硬糖",7,2,3,0,1,Pattern::Bomb,false,false}
        };
        for (const auto& reward : rewards) {
            auto make = [reward](Player& owner,std::size_t seq) {
                return std::make_shared<ResourceBonusCard>(owner,seq,reward.id,reward.name,
                    reward.pass ? EventType::PassCommitted : EventType::PlayCommitted,
                    reward.score,reward.coins,reward.limit,reward.pattern,reward.anyPlayer);
            };
            config_.catalog.push_back({reward.id,reward.name,reward.price,make,reward.level,
                make(players_[0],1)->description()});
        }
        struct Specialty { DefinitionId id; const char* name; CoinValue price; unsigned level; const char* text; };
        const Specialty special[] = {
            {2001,"储钱罐",7,2,"局初持有至少 10 颗糖果时，首次成功出牌额外获得 3 糖度。"},
            {3001,"对对夹心",6,1,"起手补齐首张普通牌的对子，以最后一张其他普通牌交换。"},
            {3002,"彩条软尺",8,2,"起手补齐首张普通牌的下一点数，以最后一张其他普通牌交换。"},
            {3014,"酸糖扒手",5,1,"每局首次成功压过对手，拿走其至多 2 颗糖果。"},
            {3015,"柠檬吸管",6,2,"每局首次用普通顺子压牌，吸取被压者至多 2 糖度。"},
            {3016,"薄荷冲刺鞋",5,1,"率先清手且其他人各剩至少两张牌时，额外获得 4 糖度。"},
            {3017,"什锦糖串",6,2,"每局首次集齐三种已打出的牌型，获得 5 糖度。"},
            {3018,"酸梅夹心",5,2,"每局首次压过出手前糖度高于自己的玩家，获得 3 糖度。"},
            {3020,"夹心妙手",7,2,"每局首次用对子压牌，可随机拿走对手一张普通原牌。对手需至少剩两张。"},
            {3021,"焦糖碎锤",10,3,"用同花顺压牌可摧毁对方一张基础功能牌，对方选择并取回半价糖果。每场限一次。"},
            {3019,"双格糖模",8,2,"每局一次，出牌前支付 2 颗糖果，复制最大的两张普通原牌。"}
        };
        for (auto spec : special) {
            config_.catalog.push_back({spec.id,spec.name,spec.price,
                [spec](Player& owner,std::size_t seq) {
                    return std::make_shared<SpecialtyCard>(CardRef{owner.id,seq},spec.id,spec.name,spec.text);
                },spec.level,spec.text});
        }
        for (auto& card : generated_function_cards())
            if (card.id != 2001) config_.catalog.push_back(std::move(card));
    }
    if(config_.consumables.empty()) {
        config_.consumables.push_back({1001,"货架扩容",4,"立即增加 1 个普通货位，本场保留",ExpandShopSlots{1}});
        config_.consumables.push_back({1002,"商店升级",4,"商店等级 +1，后续出货解锁更高等级功能牌",UpgradeShop{1}});
        FunctionCardEnhancement score;score.scoreDelta=1;
        config_.consumables.push_back({1003,"数值强化",3,"指定功能牌的原有触发效果增加 1 糖度",EnhanceFunctionCard{score}});
        FunctionCardEnhancement skill;
        skill.skills.push_back({FunctionCards::listens_to({EventType::PassCommitted}),
            [this](const Context& c,CardEffects& out) {
                const auto* e=c.event_as<PlayerPassedEvent>();
                if(e&&c.owner()&&e->player==*c.owner())out.emit(abilities::gain_coins(player(e->player),1,"added skill"));
            },"新增技能：自己不出后 +1 糖果"});
        config_.consumables.push_back({1004,"技能强化",4,"指定功能牌新增「自己不出后 +1 糖果」",EnhanceFunctionCard{skill}});
    }
    std::set<DefinitionId> definitions;
    for (const auto& definition:config_.catalog)
        if (!definition.id||definition.name.empty()||definition.price<0||!definition.create||
            !definition.minimumShopLevel||definition.minimumShopLevel>MAX_SHOP_LEVEL||
            !definitions.insert(definition.id).second)
            throw std::invalid_argument("Invalid function-card catalog");
    for(const auto& card:config_.consumables) {
        if(!card.id||card.name.empty()||card.description.empty()||card.price<0||
           !card.minimumShopLevel||card.minimumShopLevel>MAX_SHOP_LEVEL||!definitions.insert(card.id).second)
            throw std::invalid_argument("Invalid consumable catalog");
        std::visit([](const auto& effect) {
            using T=std::decay_t<decltype(effect)>;
            if constexpr(std::is_same_v<T,ExpandShopSlots>||std::is_same_v<T,UpgradeShop>) {
                if(!effect.amount)throw std::invalid_argument("Empty consumable effect");
            } else {
                // 通用参数先检查，目标卡支持性在生成货架和购买时检查。
                const auto& e=effect.enhancement;
                if(e.scoreDelta<0||e.coinsDelta<0||(!e.scoreDelta&&!e.coinsDelta&&e.skills.empty()))
                    throw std::invalid_argument("Empty or negative enhancement");
                for(const auto& skill:e.skills)if(skill.subscriptions.none()||!skill.handler||skill.description.empty())
                    throw std::invalid_argument("Invalid added skill");
            }
        },card.effect);
    }
    for (std::size_t i = 0; i < 4; ++i)
        if (index(players_[i].id) != i) throw std::invalid_argument("Expected four ordered seats");
    first_ = static_cast<Players>(std::uniform_int_distribution<int>(0,3)(random_));
    handler_.set_player_order(starting_at(first_));
    registry_.add_system(std::make_shared<ResourceSystemCard>(players_));
    const auto all = FunctionCards::listens_to({
        EventType::GameStart,
        EventType::FinishShoppingRequest,
        EventType::ShopLeave,
        EventType::AllShopsClosed,
        EventType::RoundStart,
        EventType::InitialDealAfter,
        EventType::TrickStart,
        EventType::TurnStart,
        EventType::PlayCardsRequest,
        EventType::PlayValidate,
        EventType::SubmitChoiceRequest,
        EventType::PassRequest,
        EventType::PlayCommitted,
        EventType::PlayRewardBefore,
        EventType::PlayRewardAfter,
        EventType::PassCommitted,
        EventType::TurnEnd,
        EventType::TrickEnd,
        EventType::DrawBefore,
        EventType::TrickDrawsAfter,
        EventType::RoundEnd,
        EventType::RoundRewardBefore,
        EventType::RoundRewardAfter,
        EventType::BuyShopItemRequest,
        EventType::ShopBuyQuote,
        EventType::SellFunctionCardRequest,
        EventType::ShopSellQuote,
        EventType::RefreshShopRequest,
        EventType::ShopRefreshQuote,
        EventType::LockShopSlotRequest,
        EventType::UnlockShopSlotRequest,
        EventType::UpgradeHandPatternRequest,
        EventType::BuyFacilityRequest,
        EventType::ActivateAbilityRequest,
        EventType::StateSyncRequest,
        EventType::PreviewPlayRequest,
        EventType::QuoteShopOperationRequest,
        EventType::ShoppingDeadlineControl,
        EventType::TurnDeadlineControl,
        EventType::ChoiceDeadlineControl,
        EventType::PlayerDisconnectedControl,
        EventType::PlayerReconnectedControl,
        EventType::ContinueGameRequest,
        EventType::StartNewGameRequest
    });
    registry_.add_system(std::make_shared<CallbackFunctionCard>(900003, "System game flow", all,
        [this](const Context& context, CardEffects& effects) { process(context, effects); },
        SystemOrder::AfterPlayers, true));
    registry_.add_system(std::make_shared<InitialDealSystemCard>(
        PlayerOrder{Players::P1,Players::P2,Players::P3,Players::P4},
        [this](RoundId round, const PlayerOrder& order, int count) {
            return initial_deal(round, order, count);
        }));
    dispatch(std::make_unique<GameStartedEvent>());
}
const Player& Scheduler::player(Players p) const { return players_[index(p)]; }
Player& Scheduler::player(Players p) { return players_[index(p)]; }
const ShopState& Scheduler::shop(Players p) const { return shops_[index(p)]; }
const std::vector<MoveOption>& Scheduler::preview(Players p) const { return previews_[index(p)]; }
const Poker& Scheduler::poker(CardId card) const {
    if (!card || card > cards_.size()) throw std::out_of_range("Unknown playing card");
    return cards_[static_cast<std::size_t>(card - 1)];
}
std::size_t Scheduler::next_card_sequence(Players p) { return ++cardSequences_[index(p)]; }
PlayerOrder Scheduler::starting_at(Players p) const {
    PlayerOrder order;
    for (std::size_t i = 0; i < 4; ++i) order.push_back(static_cast<Players>((index(p)+i)%4));
    return order;
}
DispatchReport Scheduler::dispatch(EventPtr e, DispatchMeta meta) {
    const bool readOnly = meta.mode == ExecutionMode::Preview;
    if (!readOnly) for (auto& preview : previews_) preview.clear();
    const auto cycle = static_cast<std::size_t>((round_-1)%8);
    const auto offset = cycle < 4 ? cycle : 7-cycle;
    handler_.set_player_order(starting_at(static_cast<Players>((index(first_)+offset)%4)));
    meta.stateVersion = version_;
    auto report = handler_.handle_event(std::move(e), std::move(meta));
    if (!readOnly) ++version_;
    return report;
}
DispatchReport Scheduler::submit(EventPtr e, RequestId requestId) {
    if (!e || !e->actor) throw std::invalid_argument("Client request needs authenticated actor");
    const auto p = index(*e->actor);
    if (!players_[p].connected) throw std::logic_error("Player is disconnected");
    const auto type = static_cast<std::size_t>(e->type);
    if (type < static_cast<std::size_t>(EventType::BuyShopItemRequest) ||
        type > static_cast<std::size_t>(EventType::QuoteShopOperationRequest))
        throw std::invalid_argument("Clients may submit requests only");
    const auto routes = registry_.matching(e->type, starting_at(*e->actor));
    if (std::none_of(routes.begin(), routes.end(), [](const auto& entry) { return entry.card->system(); }))
        throw std::invalid_argument("No system operation registered for this request");
    now_ = std::chrono::steady_clock::now();
    DispatchMeta meta; meta.origin = EventOrigin::Client; meta.requestId = requestId;
    if (e->type == EventType::PreviewPlayRequest || e->type == EventType::QuoteShopOperationRequest ||
        e->type == EventType::StateSyncRequest)
        meta.mode = ExecutionMode::Preview;
    return dispatch(std::move(e), meta);
}
void Scheduler::require_shop(Players p, ShopId shopId) const {
    const auto& s = shop(p);
    if (phase_ != GamePhase::Shopping || s.done || shopId != s.id)
        throw std::logic_error("Shop is closed or stale");
}
void Scheduler::require_turn(Players p, TurnId turnId) const {
    if (phase_ != GamePhase::Playing || roundLocked_ || !awaiting_ || p != acting_ || turnId != turn_)
        throw std::logic_error("Not this player's action window");
}
std::vector<Poker> Scheduler::selected_cards(Players p, const std::vector<CardId>& ids) const {
    const auto& hand = player(p).hand;
    std::set<CardId> unique;
    std::vector<Poker> selected;
    for (auto id : ids) {
        if (!unique.insert(id).second || std::find(hand.begin(), hand.end(), id) == hand.end())
            throw std::invalid_argument("Selection is not a subset of hand");
        selected.push_back(poker(id));
    }
    if (selected.empty()) throw std::invalid_argument("Empty play");
    return selected;
}
CardPtr Scheduler::make_card(Player& p, DefinitionId d) {
    const auto found=std::find_if(config_.catalog.begin(),config_.catalog.end(),
        [&](const auto& definition){return definition.id==d;});
    if(found==config_.catalog.end())throw std::invalid_argument("Unknown card definition");
    auto card=found->create(p,next_card_sequence(p.id));
    if(!card||!card->reference()||card->reference()->owner!=p.id||card->definition()!=d)
        throw std::logic_error("Card factory returned inconsistent metadata");
    return card;
}

void Scheduler::fill_shop(Players p, bool refreshing, std::size_t firstSlot) {
    fill_offers(p,shops_[index(p)],refreshing,firstSlot,random_,sequence_);
}
std::size_t Scheduler::shop_slots(const Player& p) const {
    const std::size_t base=p.facilities.count(2)?5:p.facilities.count(1)?4:3;
    return std::min(MAX_SHOP_SLOTS,base+p.extraShopSlots);
}
const ConsumableCard& Scheduler::consumable(DefinitionId id) const {
    const auto it=std::find_if(config_.consumables.begin(),config_.consumables.end(),[&](const auto& c){return c.id==id;});
    if(it==config_.consumables.end())throw std::invalid_argument("Unknown consumable");
    return *it;
}
std::vector<CardRef> Scheduler::consumable_targets(Players p,DefinitionId id) const {
    const auto& c=consumable(id);std::vector<CardRef> targets;
    if(!c.needs_target())return targets;
    for(const auto& card:registry_.player_cards(p)) {
        try { card->validate_enhancement(std::get<EnhanceFunctionCard>(c.effect).enhancement); }
        catch(const std::exception&) { continue; }
        targets.push_back(*card->reference());
    }
    return targets;
}
void Scheduler::validate_consumable(Players p,const ConsumableCard& card,std::optional<CardRef> target) const {
    if(player(p).shopLevel<card.minimumShopLevel)throw std::logic_error("Shop level too low");
    std::visit([&](const auto& effect) {
        using T=std::decay_t<decltype(effect)>;
        if constexpr(std::is_same_v<T,ExpandShopSlots>) {
            if(target||effect.amount>MAX_SHOP_SLOTS-shop_slots(player(p)))throw std::logic_error("Shop slot limit reached");
        } else if constexpr(std::is_same_v<T,UpgradeShop>) {
            if(target||player(p).shopLevel>MAX_SHOP_LEVEL||effect.amount>MAX_SHOP_LEVEL-player(p).shopLevel)
                throw std::logic_error("Shop level limit reached");
        } else {
            if(!target||target->owner!=p)throw std::invalid_argument("Choose your own enhancement target");
            registry_.at(p,card_slot(*target))->validate_enhancement(effect.enhancement);
        }
    },card.effect);
}
void Scheduler::use_consumable(Players p,const ConsumableCard& card,std::optional<CardRef> target,CardEffects& out) {
    validate_consumable(p,card,target);
    std::visit([&](const auto& effect) {
        using T=std::decay_t<decltype(effect)>;
        if constexpr(std::is_same_v<T,ExpandShopSlots>) {
            auto staged=shop(p);const auto before=staged.offers.size();
            staged.offers.resize(before+effect.amount);
            auto random=random_;auto sequence=sequence_;
            fill_offers(p,staged,true,before,random,sequence);
            out.emit(abilities::expand_shop_slots(p,effect.amount,[&](Players who,std::size_t amount,ShopSlotsChangedEvent& fact) {
                fact.player=who;fact.before=before;fact.after=staged.offers.size();
                shops_[index(who)]=std::move(staged);random_=random;sequence_=sequence;
                player(who).extraShopSlots+=amount;return true;
            }));
        } else if constexpr(std::is_same_v<T,UpgradeShop>) {
            out.emit(abilities::upgrade_shop(player(p),effect.amount));
        } else out.emit(abilities::enhance_function_card(registry_,p,card_slot(*target),effect.enhancement));
    },card.effect);
}
void Scheduler::price_family_offers(Players seat, ShopState& shop) const {
    for (auto& offer : shop.offers) {
        const auto family = candy_family(offer.definition);
        if (!offer.id || !family) continue;
        const auto price = [&](DefinitionId id) {
            return std::find_if(config_.catalog.begin(), config_.catalog.end(),
                [&](const auto& card){return card.id == id;})->price;
        };
        offer.price = price(offer.definition);
        offer.upgrade = false;
        for (const auto& owned : registry_.player_cards(seat)) {
            if (candy_family(owned->definition()) != family) continue;
            if (owned->definition() >= offer.definition) offer = {};
            else {
                offer.price -= price(owned->definition());
                offer.upgrade = true;
            }
            break;
        }
    }
}
void Scheduler::fill_offers(Players p,ShopState& s,bool refreshing,std::size_t firstSlot,
                            std::mt19937& random,std::uint64_t& sequence) {
    std::set<DefinitionId> used;
    for (const auto& card : registry_.player_cards(p)) {
        used.insert(card->definition());
        const auto family = candy_family(card->definition());
        if (family) for (const auto& def : config_.catalog)
            if (candy_family(def.id)==family && def.id<=card->definition()) used.insert(def.id);
    }
    for (std::size_t i = 0; i < s.offers.size(); ++i)
        if (i < firstSlot || (s.locked && *s.locked == i) || (refreshing && !firstSlot && !s.offers[i].id))
            if (s.offers[i].id) used.insert(s.offers[i].definition);
    for (std::size_t i = 0; i < s.offers.size(); ++i) {
        if (i < firstSlot || (s.locked && *s.locked == i) || (refreshing && !firstSlot && !s.offers[i].id)) continue;
        std::vector<DefinitionId> choices;
        for (const auto& definition:config_.catalog)
            if (!used.count(definition.id)&&definition.minimumShopLevel<=player(p).shopLevel) choices.push_back(definition.id);
        std::vector<const ConsumableCard*> consumables;
        if(i>=2||choices.empty()) for(const auto& card:config_.consumables) {
            if(used.count(card.id))continue;
            if(const auto* expand=std::get_if<ExpandShopSlots>(&card.effect))
                if(expand->amount>MAX_SHOP_SLOTS-s.offers.size())continue;
            try {
                if(card.needs_target()) {
                    const auto targets=consumable_targets(p,card.id);
                    if(targets.empty())continue;
                    validate_consumable(p,card,targets.front());
                } else validate_consumable(p,card,std::nullopt);
            } catch(const std::exception&) { continue; }
            consumables.push_back(&card);
        }
        if(!consumables.empty()&&(choices.empty()||std::bernoulli_distribution(config_.consumableChance)(random))) {
            const auto& card=*consumables[std::uniform_int_distribution<std::size_t>(0,consumables.size()-1)(random)];
            s.offers[i]={++sequence,card.id,card.price,card.name,ShopProductKind::Consumable,card.description,card.needs_target()};
            used.insert(card.id);continue;
        }
        if (choices.empty()) { s.offers[i] = {}; continue; }
        const auto d = choices[std::uniform_int_distribution<std::size_t>(0,choices.size()-1)(random)];
        used.insert(d);
        const auto definition=std::find_if(config_.catalog.begin(),config_.catalog.end(),
            [&](const auto& item){return item.id==d;});
        s.offers[i] = {++sequence, d, definition->price, definition->name,ShopProductKind::FunctionCard,definition->description};
    }
    price_family_offers(p,s);
}
void Scheduler::open_shops(CardEffects& out) {
    phase_ = GamePhase::Shopping; awaiting_ = false; choice_.reset();
    deadline_ = now_ + config_.shopTime;
    for (auto& p : players_) {
        auto& s = shops_[index(p.id)];
        const auto old = s;
        s = {}; s.id = ++sequence_;
        const std::size_t size = shop_slots(p);
        s.offers.resize(size);
        if (old.locked && *old.locked < size && *old.locked < old.offers.size()
            && old.offers[*old.locked].id) {
            s.locked = old.locked; s.offers[*s.locked] = old.offers[*old.locked];
        }
        std::vector<DefinitionId> facilities;
        for (DefinitionId f : {1,2,3,4,5,7,8}) {
            if (p.facilities.count(f) || (f == 2 && !p.facilities.count(1)) ||
                (f == 4 && !p.facilities.count(3))) continue;
            facilities.push_back(f);
        }
        if (!facilities.empty())
            s.facility = facilities[std::uniform_int_distribution<std::size_t>(0,facilities.size()-1)(random_)];
        fill_shop(p.id, false);
        emit<ShopEnteredEvent>(out, [&](auto& e){ e.player=p.id; e.shopId=s.id; });
    }
}
void Scheduler::start_round(CardEffects& out) {
    phase_ = GamePhase::RoundStarting; roundLocked_ = false; choice_.reset();
    usedSpecial_ = {}; playedPatterns_ = {};
    for (const auto& p : players_) savingsReady_[index(p.id)] = p.coins >= 10;
    cards_ = standard_deck(); drawPile_.clear(); discarded_.clear(); tableCards_.clear(); table_.reset();
    for (const auto& card : cards_) drawPile_.push_back(card.id);
    std::shuffle(drawPile_.begin(), drawPile_.end(), random_);
    for (auto& p : players_) { p.hand.clear(); p.roundPlayCoins=0; p.roundPlayScore=0; }
    const auto cycle = static_cast<std::size_t>((round_-1)%8);
    const auto offset = cycle < 4 ? cycle : 7-cycle;
    acting_ = static_cast<Players>((index(first_)+offset)%4);
    emit<RoundStartedEvent>(out,[&](auto& e){e.roundId=round_;e.firstPlayer=acting_;});
}
bool Scheduler::initial_deal(RoundId round, const PlayerOrder& order, int count) {
    if (phase_ != GamePhase::RoundStarting || round != round_ || count <= 0 ||
        static_cast<std::size_t>(count) > drawPile_.size()/order.size())
        throw std::logic_error("Invalid initial deal state");
    std::array<std::vector<CardId>,4> hands;
    auto pile = drawPile_;
    for (int n=0;n<count;++n) for (auto p:order) {
        hands[index(p)].push_back(pile.back()); pile.pop_back();
    }
    for (const auto seat : order) {
        auto& hand = hands[index(seat)];
        for (const auto& effect : registry_.player_cards(seat)) {
            const auto id = effect->definition();
            if ((id != 3001 && id != 3002) || !effect->available() || !effect->usable()) continue;
            const auto ordinary = [&](CardId card){ return !poker(card).isJoker; };
            const auto anchor = std::find_if(hand.begin(),hand.end(),ordinary);
            const auto payment = std::find_if(hand.rbegin(),hand.rend(),ordinary);
            if (anchor == hand.end() || *anchor == *payment) continue;
            const int wanted = *poker(*anchor).rank + (id == 3002 ? 1 : 0);
            const auto matches = [&](CardId card){ return poker(card).rank == wanted; };
            if (std::any_of(anchor+1,hand.end(),matches)) continue;
            const auto replacement = std::find_if(pile.rbegin(),pile.rend(),matches);
            if (replacement != pile.rend()) std::iter_swap(payment,replacement);
        }
    }
    for (auto& p:players_) p.hand.swap(hands[index(p.id)]);
    drawPile_.swap(pile); phase_=GamePhase::Playing;
    return true;
}
void Scheduler::next_turn(Players p, CardEffects& out) {
    emit<TurnStartedEvent>(out,[&](auto& e){e.turnId=++turn_;e.player=p;});
}
void Scheduler::play(Players p, const std::vector<CardId>& ids,
                      const MoveOption& option, CardEffects& out) {
    (void)selected_cards(p, ids);
    auto& hand=player(p).hand;
    auto remaining=hand;
    for (auto id:ids) remaining.erase(std::find(remaining.begin(),remaining.end(),id));
    discarded_.reserve(discarded_.size()+ids.size());
    hand.swap(remaining); discarded_.insert(discarded_.end(),ids.begin(),ids.end());
    const bool covering = table_.has_value() && lastPlayed_ != p;
    auto& owner = player(p);
    auto& opponent = player(lastPlayed_);
    const bool chasing = covering && opponent.score > owner.score;
    auto& used = usedSpecial_[index(p)];
    auto& patterns = playedPatterns_[index(p)];
    patterns.insert(option.pattern);
    for (const auto& equipped : registry_.player_cards(p)) {
        auto card = std::dynamic_pointer_cast<SpecialtyCard>(equipped);
        if (!card || !card->available() || !card->usable()) continue;
        const auto id = card->definition();
        if (used.count(id)) continue;
        ScoreValue reward = 0;
        switch (id) {
        case 2001: if (!savingsReady_[index(p)]) continue; reward=3; break;
        case 3014:
            if (!covering) continue;
            if (const auto n=std::min<CoinValue>(2,opponent.coins)) {
                out.emit(abilities::spend_coins(opponent,n,"酸糖扒手"));
                out.emit(abilities::gain_coins(owner,n,"酸糖扒手"));
            }
            break;
        case 3015:
            if (!covering || option.pattern!=HandPattern::Straight) continue;
            if (const auto n=std::clamp<ScoreValue>(opponent.score,0,2)) {
                out.emit(abilities::subtract_score(opponent,n));
                out.emit(abilities::add_score(owner,n));
            }
            break;
        case 3020:
            if (!covering || option.pattern!=HandPattern::Pair) continue;
            if (!hand.empty() && opponent.hand.size()>=2 &&
                std::any_of(opponent.hand.begin(),opponent.hand.end(),[&](CardId c){return c<=52;}))
                candyChoice_=CandyChoice{++sequence_,CandyChoice::Kind::Steal,p,lastPlayed_,{}};
            break;
        case 3021: {
            if (!covering || option.pattern!=HandPattern::StraightFlush || hand.empty()
                || hammerUsed_[index(p)] || hammerHit_[index(lastPlayed_)]) continue;
            auto owned=registry_.player_cards(lastPlayed_);
            if (owned.size()<2) continue;
            std::vector<std::pair<CoinValue,std::size_t>> eligible;
            for (const auto& item : owned) {
                for (const auto& def : config_.catalog)
                    if (item->definition()==def.id && def.minimumShopLevel==1)
                        eligible.emplace_back(def.price,card_slot(*item->reference()));
            }
            if (eligible.empty()) continue;
            std::sort(eligible.begin(),eligible.end());
            candyChoice_=CandyChoice{++sequence_,CandyChoice::Kind::Hammer,p,lastPlayed_,{}};
            for (auto entry : eligible) candyChoice_->slots.push_back(entry.second);
            continue;
        }
        case 3016:
            if (!hand.empty() || std::any_of(players_.begin(),players_.end(),
                    [p](const Player& other){return other.id!=p && other.hand.size()<2;})) continue;
            reward=4; break;
        case 3017: if (patterns.size()<3) continue; reward=5; break;
        case 3018: if (!chasing) continue; reward=3; break;
        default: continue;
        }
        used.insert(id);
        if (reward) out.emit(abilities::add_score(owner,reward+card->bonus()));
    }
    table_=option; tableCards_=ids; lastPlayed_=p; awaiting_=false; choice_.reset();
    if (hand.empty()) {
        roundLocked_=true;
        for (const auto& value:players_) lockedHands_[index(value.id)]=value.hand.size();
        emit<RoundWinnerLockedEvent>(out,[&](auto& e){
            e.roundId=round_;e.winner=p;e.resultId=round_;});
    }
    emit<CardsPlayedEvent>(out,[&](auto& e){
        e.actor=p;e.turnId=turn_;e.player=p;e.handId=++sequence_;e.cards=ids;e.pattern=option.pattern;});
}
std::size_t Scheduler::card_slot(CardRef card) const {
    for (std::size_t slot=0;slot<MAX_FUNCTION_SLOTS;++slot) {
        const auto current=registry_.at(card.owner,slot);
        if (current && current->reference()->seq==card.seq) return slot;
    }
    throw std::invalid_argument("Card is not equipped");
}
CoinValue Scheduler::sale_value(Players p, CardRef card) const {
    if (card.owner!=p) throw std::invalid_argument("Wrong card owner");
    (void)card_slot(card);
    const auto& costs=paidCosts_[index(p)];
    const auto it=costs.find(card.seq);
    const auto cost=it==costs.end()?0:it->second;
    const auto ratio=player(p).facilities.count(8)?6:5;
    return (cost/10)*ratio+((cost%10)*ratio)/10;
}
CoinValue Scheduler::upgrade_cost(Players p, HandPattern pattern) const {
    const auto i=pattern_index(pattern);const auto level=player(p).upgrades[i];
    if (level>=4) throw std::logic_error("Upgrade maximum reached");
    auto cost=upgradePrices[i]*(level+1);
    if (player(p).facilities.count(5) && !shop(p).upgradeDiscountUsed) cost=std::max<CoinValue>(1,cost-2);
    return cost;
}
void Scheduler::process(const Context& c, CardEffects& out) {
    const auto type=*c.type();
    const bool query=type==EventType::PreviewPlayRequest ||
        type==EventType::QuoteShopOperationRequest || type==EventType::StateSyncRequest;
    if (c.metadata()->mode==ExecutionMode::Preview && !query) return;
    const auto actor=[&](){if(!c.actor())throw std::invalid_argument("Missing actor");return *c.actor();};
    switch(type) {
    case EventType::GameStart: open_shops(out); break;
    case EventType::FinishShoppingRequest: {
        const auto& e=*c.event_as<FinishShoppingRequest>();const auto p=actor();require_shop(p,e.shopId);
        shops_[index(p)].done=true;
        emit<ShopLeftEvent>(out,[&](auto& x){x.player=p;x.shopId=e.shopId;});break;
    }
    case EventType::ShopLeave:
        if (phase_==GamePhase::Shopping &&
            std::all_of(shops_.begin(),shops_.end(),[](const auto& s){return s.done;}))
            emit<AllShopsClosedEvent>(out,[&](auto& e){e.roundId=round_;});
        break;
    case EventType::AllShopsClosed:
        if (phase_==GamePhase::Shopping &&
            std::all_of(shops_.begin(),shops_.end(),[](const auto& s){return s.done;})) start_round(out);
        break;
    case EventType::RoundStart:
        emit<BeforeInitialDealEvent>(out,[&](auto& e){e.roundId=round_;e.cardsPerPlayer=7;});break;
    case EventType::InitialDealAfter:
        emit<TrickStartedEvent>(out,[&](auto& e){e.roundId=round_;e.trickId=++trick_;e.leader=acting_;});break;
    case EventType::TrickStart: {
        const auto& e=*c.event_as<TrickStartedEvent>();table_.reset();tableCards_.clear();lastPlayed_=e.leader;
        next_turn(e.leader,out);break;
    }
    case EventType::TurnStart: {
        const auto& e=*c.event_as<TurnStartedEvent>();acting_=e.player;awaiting_=true;
        deadline_=now_+config_.turnTime;
        if (!player(acting_).connected)
            emit<TurnDeadlineControl>(out,[&](auto& x){x.turnId=turn_;x.player=acting_;x.expectedVersion=turn_;});
        break;
    }
    case EventType::PlayCardsRequest: {
        const auto& e=*c.event_as<PlayCardsRequest>();const auto p=actor();require_turn(p,e.turnId);
        if(choice_)throw std::logic_error("Resolve the pending choice first");
        (void)selected_cards(p,e.cards);
        emit<PlayValidationEvent>(out,[&](auto& x){
            x.actor=p;x.player=p;x.turnId=e.turnId;x.cards=e.cards;x.wildcards=e.wildcards;});
        break;
    }
    case EventType::PlayValidate: {
        const auto& e=*c.event_as<PlayValidationEvent>();const auto p=e.player;require_turn(p,e.turnId);
        if(choice_)throw std::logic_error("Resolve the pending choice first");
        const auto selected=selected_cards(p,e.cards);
        const bool joker=std::any_of(selected.begin(),selected.end(),[](const auto& card){return card.isJoker;});
        auto options=legal_moves(selected,table_,e.wildcards.empty()?nullptr:&e.wildcards);
        if(options.empty())throw std::invalid_argument("Illegal play");
        if((joker && e.wildcards.empty()) || options.size()>1) {
            choice_=PendingPlayChoice{++sequence_,p,turn_,e.cards,std::move(options)};
            deadline_=now_+config_.choiceTime;
            emit<ChoiceOpenedEvent>(out,[&](auto& x){
                x.choiceId=choice_->id;x.player=p;x.choiceKind=ChoiceKind::Wildcard;});
        } else play(p,e.cards,options.front(),out);
        break;
    }
    case EventType::SubmitChoiceRequest: {
        const auto& e=*c.event_as<SubmitChoiceRequest>();const auto p=actor();
        if (candyChoice_) {
            const auto pending=*candyChoice_;
            if (pending.responder()!=p || pending.id!=e.choiceId || e.options.size()!=1)
                throw std::invalid_argument("Invalid candy choice");
            const auto answer=e.options.front();
            auto& target=player(pending.defender);
            if (pending.kind==CandyChoice::Kind::Sacrifice) {
                if (answer>=pending.slots.size()) throw std::invalid_argument("Invalid sacrifice");
                const auto slot=pending.slots[answer];
                const auto ref=*registry_.at(target.id,slot)->reference();
                auto& costs=paidCosts_[index(target.id)];
                const auto paid=costs.find(ref.seq);
                const auto refund=paid==costs.end()?0:paid->second/2;
                out.emit(abilities::remove_function_card(registry_,target.id,slot,RemovalReason::Deleted));
                costs.erase(ref.seq);
                out.emit(abilities::gain_coins(target,refund,"焦糖碎锤"));
                hammerUsed_[index(pending.attacker)]=hammerHit_[index(target.id)]=true;
            } else {
                if (answer>1) throw std::invalid_argument("Invalid decision");
                if (answer && pending.kind==CandyChoice::Kind::Hammer) {
                    candyChoice_->kind=CandyChoice::Kind::Sacrifice;
                    candyChoice_->id=++sequence_;
                    deadline_=now_+config_.choiceTime;
                    break;
                }
                if (answer) {
                    std::vector<CardId> eligible;
                    for (auto id : target.hand) if (id<=52) eligible.push_back(id);
                    const auto taken=eligible[std::uniform_int_distribution<std::size_t>(0,eligible.size()-1)(random_)];
                    player(pending.attacker).hand.push_back(taken);
                    target.hand.erase(std::find(target.hand.begin(),target.hand.end(),taken));
                }
            }
            candyChoice_.reset();
            emit<TurnEndedEvent>(out,[&](auto& x){x.actor=pending.attacker;x.player=pending.attacker;x.turnId=turn_;});
            break;
        }
        if(!choice_||choice_->player!=p||choice_->id!=e.choiceId||e.options.size()!=1||
            e.options[0]>=choice_->options.size())throw std::invalid_argument("Invalid choice");
        require_turn(p,choice_->turn);const auto selected=choice_->cards;
        const auto option=choice_->options[static_cast<std::size_t>(e.options[0])];
        auto valid=legal_moves(selected_cards(p,selected),table_,&option.wildcards);
        if(valid.empty())throw std::logic_error("Choice became stale");
        play(p,selected,option,out);break;
    }
    case EventType::PassRequest: {
        const auto& e=*c.event_as<PassRequest>();const auto p=actor();require_turn(p,e.turnId);
        if(choice_||!table_)throw std::logic_error("Leader must play");
        awaiting_=false;
        emit<PlayerPassedEvent>(out,[&](auto& x){x.actor=p;x.turnId=turn_;x.player=p;x.reason=PassReason::Chosen;});break;
    }
    case EventType::PlayCommitted: {
        const auto& e=*c.event_as<CardsPlayedEvent>();const auto i=pattern_index(e.pattern);
        const ScoreValue bases[]={1,3,5,8,8};const CoinValue money[]={1,2,3,5,5};
        emit<BeforePlayRewardEvent>(out,[&](auto& x){
            x.actor=e.player;x.turnId=e.turnId;x.player=e.player;x.handId=e.handId;
            x.settlementId=++sequence_;x.initialReward={x.settlementId,bases[i]*(player(e.player).upgrades[i]+1),money[i]};});
        break;
    }
    case EventType::PlayRewardBefore: {
        const auto& e=*c.event_as<BeforePlayRewardEvent>();auto& p=player(e.player);const auto reward=out.reward();
        out.emit(abilities::add_score(p,reward.score));out.emit(abilities::gain_coins(p,reward.coins,"play"));
        p.roundPlayScore+=reward.score;p.roundPlayCoins+=reward.coins;
        emit<PlayRewardGrantedEvent>(out,[&](auto& x){
            x.actor=p.id;x.turnId=e.turnId;x.player=p.id;x.handId=e.handId;x.reward=reward;});break;
    }
    case EventType::PlayRewardAfter: {
        if (candyChoice_) { deadline_=now_+config_.choiceTime; break; }
        const auto& e=*c.event_as<PlayRewardGrantedEvent>();
        emit<TurnEndedEvent>(out,[&](auto& x){x.actor=e.player;x.turnId=e.turnId;x.player=e.player;});break;
    }
    case EventType::PassCommitted: {
        const auto& e=*c.event_as<PlayerPassedEvent>();
        emit<TurnEndedEvent>(out,[&](auto& x){x.actor=e.player;x.turnId=e.turnId;x.player=e.player;});break;
    }
    case EventType::TurnEnd:
        if(roundLocked_) {
            phase_=GamePhase::RoundEnding;
            emit<RoundEndedEvent>(out,[&](auto& e){e.roundId=round_;e.resultId=round_;});
        } else {
            const auto next=static_cast<Players>((index(acting_)+1)%4);
            if(table_ && next==lastPlayed_)
                emit<TrickEndedEvent>(out,[&](auto& e){e.trickId=trick_;e.leader=lastPlayed_;});
            else next_turn(next,out);
        } break;
    case EventType::TrickEnd:
        drawOrder_=starting_at(lastPlayed_);drawAcknowledged_.fill(false);
        for(auto p:drawOrder_) emit<BeforeCardsDrawEvent>(out,[&](auto& e){
            e.player=p;e.settlementId=trick_;e.requestedCount=1;e.reason=DrawReason::TrickEnd;});
        break;
    case EventType::DrawBefore: {
        const auto& e=*c.event_as<BeforeCardsDrawEvent>();
        if(roundLocked_||drawOrder_.empty()||e.reason!=DrawReason::TrickEnd||e.settlementId!=trick_)break;
        drawAcknowledged_[index(e.player)]=true;
        if(!std::all_of(drawAcknowledged_.begin(),drawAcknowledged_.end(),[](bool v){return v;}))break;
        const bool hadCards=!drawPile_.empty();
        out.emit(abilities::draw_to_all(drawOrder_,1,DrawReason::TrickEnd,
            [&](const PlayerOrder& order,std::size_t count,DrawReason,
                std::vector<std::vector<CardId>>& drawn) {
                auto pile=drawPile_;std::array<std::vector<CardId>,4> hands;
                for(auto p:order)hands[index(p)]=player(p).hand;
                for(std::size_t i=0;i<order.size();++i)
                    for(std::size_t n=0;n<count&&!pile.empty();++n) {
                        drawn[i].push_back(pile.back());hands[index(order[i])].push_back(pile.back());pile.pop_back();
                    }
                for(auto p:order)player(p).hand.swap(hands[index(p)]);
                drawPile_.swap(pile);return true;
            }));
        drawOrder_.clear();
        if(hadCards&&drawPile_.empty())emit<DrawPileExhaustedEvent>(out,[&](auto& x){x.roundId=round_;});
        emit<TrickDrawingCompletedEvent>(out,[&](auto& x){x.trickId=trick_;});break;
    }
    case EventType::TrickDrawsAfter:
        acting_=lastPlayed_;
        emit<TrickStartedEvent>(out,[&](auto& e){e.roundId=round_;e.trickId=++trick_;e.leader=acting_;});break;
    case EventType::RoundEnd:
        roundPaid_.fill(false);
        for(const auto& p:players_)emit<BeforeRoundRewardEvent>(out,[&](auto& e){
            e.actor=p.id;e.roundId=round_;e.player=p.id;e.settlementId=++sequence_;
            const auto base=16+5*static_cast<ScoreValue>(round_-1);
            e.initialReward={e.settlementId,std::max<ScoreValue>(0,base-static_cast<ScoreValue>(lockedHands_[index(p.id)])),
                8+5*static_cast<CoinValue>(round_-1)};});
        break;
    case EventType::RoundRewardBefore: {
        const auto& e=*c.event_as<BeforeRoundRewardEvent>();auto& p=player(e.player);const auto r=out.reward();
        if(roundPaid_[index(p.id)])break;
        out.emit(abilities::add_score(p,r.score));out.emit(abilities::gain_coins(p,r.coins,"round"));
        roundPaid_[index(p.id)]=true;
        if(std::all_of(roundPaid_.begin(),roundPaid_.end(),[](bool paid){return paid;}))
            emit<RoundRewardsGrantedEvent>(out,[&](auto& x){x.roundId=round_;x.resultId=round_;});
        break;
    }
    case EventType::RoundRewardAfter:
        for (auto& owner : players_)
            owner.hand.erase(std::remove_if(owner.hand.begin(),owner.hand.end(),
                [](CardId id){return id>54;}),owner.hand.end());
        if(round_>=config_.rounds) {
            phase_=GamePhase::Finished;winners_.clear();
            const auto best=std::max_element(players_.begin(),players_.end(),
                [](const auto& a,const auto& b){return a.score<b.score;})->score;
            for(const auto& p:players_)if(p.score==best)winners_.push_back(p.id);
            emit<GameEndedEvent>(out,[&](auto& e){e.winners=winners_;e.finalVersion=version_+1;});
        } else {++round_;open_shops(out);}
        break;
    case EventType::BuyShopItemRequest: {
        const auto& e=*c.event_as<BuyShopItemRequest>();const auto p=actor();require_shop(p,e.shopId);
        const auto& s=shop(p);
        if(e.slotId>=s.offers.size()||!s.offers[static_cast<std::size_t>(e.slotId)].id||
            s.offers[static_cast<std::size_t>(e.slotId)].id!=e.productId)throw std::invalid_argument("Invalid shop product");
        const auto price=s.offers[static_cast<std::size_t>(e.slotId)].price;
        emit<BeforeShopPurchaseEvent>(out,[&](auto& x){x.actor=p;x.player=p;x.shopId=s.id;x.productId=e.productId;
            x.targetCard=e.targetCard;x.settlementId=++sequence_;x.initialQuote={x.settlementId,price,price,version_};});break;
    }
    case EventType::ShopBuyQuote: {
        const auto& e=*c.event_as<BeforeShopPurchaseEvent>();require_shop(e.player,e.shopId);
        auto& s=shops_[index(e.player)];auto it=std::find_if(s.offers.begin(),s.offers.end(),
            [&](const auto& offer){return offer.id==e.productId&&offer.id;});
        if(it==s.offers.end())throw std::logic_error("Product changed");
        auto& p=player(e.player);
        const auto product=*it;const auto offerSlot=static_cast<std::size_t>(it-s.offers.begin());
        const auto cost=out.shop().finalCost;
        if(cost<0||p.coins<cost)throw std::logic_error("Insufficient coins");
        if(product.kind==ShopProductKind::Consumable) {
            use_consumable(p.id,consumable(product.definition),e.targetCard,out);
        } else {
            if(e.targetCard)throw std::invalid_argument("Function card purchase does not take a target");
            if (product.upgrade) {
                const auto owned = registry_.player_cards(p.id);
                auto previous = std::find_if(owned.begin(),owned.end(),[&](const auto& card){
                    return candy_family(card->definition())==candy_family(product.definition);
                });
                if (previous==owned.end()) throw std::logic_error("Upgrade target changed");
                auto next=make_card(p,product.definition);
                next->inherit_enhancements(**previous);
                const auto ref=*(*previous)->reference();
                auto& costs=paidCosts_[index(p.id)];
                const auto total=costs.at(ref.seq)+cost;
                registry_.replace(p.id,card_slot(ref),next);
                costs[next->reference()->seq]=total;
                costs.erase(ref.seq);
            } else {
            std::size_t slot=0;while(slot<p.functionSlots&&registry_.at(p.id,slot))++slot;
            if(slot>=p.functionSlots)throw std::logic_error("Function slots full");
            auto card=make_card(p,product.definition);const auto cardSeq=card->reference()->seq;
            out.emit(abilities::equip_function_card(registry_,p,slot,std::move(card)));
            paidCosts_[index(p.id)][cardSeq]=cost;
            }
        }
        out.emit(abilities::spend_coins(p,cost,"purchase"));
        s.offers[offerSlot]={};price_family_offers(p.id,s);if(s.locked&&*s.locked==offerSlot)s.locked.reset();
        emit<ShopPurchasedEvent>(out,[&](auto& x){x.player=p.id;x.shopId=s.id;x.productId=e.productId;x.paid=cost;});
        if(product.kind==ShopProductKind::Consumable)
            emit<ConsumableUsedEvent>(out,[&](auto& x){x.player=p.id;x.shopId=s.id;x.productId=e.productId;
                x.definition=product.definition;x.targetCard=e.targetCard;});
        break;
    }
    case EventType::SellFunctionCardRequest: {
        const auto& e=*c.event_as<SellFunctionCardRequest>();const auto p=actor();require_shop(p,e.shopId);
        const auto value=sale_value(p,e.card);
        emit<BeforeShopSaleEvent>(out,[&](auto& x){x.actor=p;x.player=p;x.shopId=e.shopId;x.card=e.card;
            x.settlementId=++sequence_;x.initialQuote={x.settlementId,value,value,version_};});break;
    }
    case EventType::ShopSellQuote: {
        const auto& e=*c.event_as<BeforeShopSaleEvent>();require_shop(e.player,e.shopId);
        if(e.card.owner!=e.player)throw std::invalid_argument("Wrong card owner");
        const auto slot=card_slot(e.card);const auto value=out.shop().finalCost;
        if(value<0)throw std::invalid_argument("Negative sale");
        out.emit(abilities::gain_coins(player(e.player),value,"sale"));
        out.emit(abilities::remove_function_card(registry_,e.player,slot,RemovalReason::Sold));
        paidCosts_[index(e.player)].erase(e.card.seq);
        price_family_offers(e.player,shops_[index(e.player)]);
        emit<ShopSoldEvent>(out,[&](auto& x){x.player=e.player;x.shopId=e.shopId;x.card=e.card;x.received=value;});break;
    }
    case EventType::RefreshShopRequest: {
        const auto& e=*c.event_as<RefreshShopRequest>();const auto p=actor();require_shop(p,e.shopId);
        const auto& s=shop(p);if(s.refreshes>=5)throw std::logic_error("Refresh limit");
        CoinValue cost=s.freeRefreshes?0:2+static_cast<CoinValue>(s.paidRefreshes);
        if(cost&&player(p).facilities.count(7)&&!s.refreshDiscountUsed)cost=std::max<CoinValue>(1,cost-1);
        emit<BeforeShopRefreshEvent>(out,[&](auto& x){x.actor=p;x.player=p;x.shopId=s.id;
            x.settlementId=++sequence_;x.initialQuote={x.settlementId,cost,cost,version_};});break;
    }
    case EventType::ShopRefreshQuote: {
        const auto& e=*c.event_as<BeforeShopRefreshEvent>();require_shop(e.player,e.shopId);
        auto& s=shops_[index(e.player)];auto& p=player(e.player);
        if(s.refreshes>=5)throw std::logic_error("Refresh limit");
        const auto cost=s.freeRefreshes?0:out.shop().finalCost;
        if(cost<0||p.coins<cost)throw std::logic_error("Insufficient coins");
        fill_shop(p.id,true);out.emit(abilities::spend_coins(p,cost,"refresh"));
        if(s.freeRefreshes)--s.freeRefreshes;else {++s.paidRefreshes;s.refreshDiscountUsed=true;}
        ++s.refreshes;
        emit<ShopRefreshedEvent>(out,[&](auto& x){x.player=p.id;x.shopId=s.id;x.paid=cost;
            for(std::size_t i=0;i<s.offers.size();++i)if(s.offers[i].id&&(!s.locked||*s.locked!=i))x.slots.push_back(i);});break;
    }
    case EventType::LockShopSlotRequest: {
        const auto& e=*c.event_as<LockShopSlotRequest>();const auto p=actor();require_shop(p,e.shopId);
        auto& s=shops_[index(p)];
        if(e.slotId>=s.offers.size()||!s.offers[static_cast<std::size_t>(e.slotId)].id||
            (s.locked&&*s.locked!=e.slotId))throw std::logic_error("Invalid lock");
        if(s.locked&&*s.locked==e.slotId)break;
        s.locked=static_cast<std::size_t>(e.slotId);
        emit<ShopSlotLockedEvent>(out,[&](auto& x){x.player=p;x.shopId=s.id;x.slotId=e.slotId;});break;
    }
    case EventType::UnlockShopSlotRequest: {
        const auto& e=*c.event_as<UnlockShopSlotRequest>();const auto p=actor();require_shop(p,e.shopId);
        auto& s=shops_[index(p)];
        if(!s.locked||*s.locked!=e.slotId)throw std::logic_error("Slot not locked");
        s.locked.reset();
        emit<ShopSlotUnlockedEvent>(out,[&](auto& x){x.player=p;x.shopId=s.id;x.slotId=e.slotId;});break;
    }
    case EventType::UpgradeHandPatternRequest: {
        const auto& e=*c.event_as<UpgradeHandPatternRequest>();const auto p=actor();require_shop(p,e.shopId);
        const auto cost=upgrade_cost(p,e.pattern);auto& value=player(p);
        out.emit(abilities::spend_coins(value,cost,"upgrade"));
        ++value.upgrades[pattern_index(e.pattern)];shops_[index(p)].upgradeDiscountUsed=true;
        emit<HandPatternUpgradedEvent>(out,[&](auto& x){x.player=p;x.pattern=e.pattern;
            x.afterLevel=static_cast<int>(value.upgrades[pattern_index(e.pattern)]);
            x.beforeLevel=x.afterLevel-1;});break;
    }
    case EventType::BuyFacilityRequest: {
        const auto& e=*c.event_as<BuyFacilityRequest>();const auto p=actor();require_shop(p,e.shopId);
        auto& s=shops_[index(p)];auto& value=player(p);
        if(!s.facility||s.facility!=e.facilityId||e.facilityId>8)throw std::logic_error("Facility unavailable");
        const auto cost=facilityPrices[e.facilityId];
        if(value.coins<cost)throw std::logic_error("Insufficient coins");
        value.facilities.insert(e.facilityId);
        out.emit(abilities::spend_coins(value,cost,"facility"));
        if(e.facilityId==3||e.facilityId==4)abilities::grant_function_slot(value);
        if(e.facilityId==1||e.facilityId==2) {
            const auto oldSize=s.offers.size();s.offers.resize(shop_slots(value));
            fill_shop(p,true,oldSize);
        }
        s.facility=0;emit<FacilityPurchasedEvent>(out,[&](auto& x){x.player=p;x.facilityId=e.facilityId;});break;
    }
    case EventType::ActivateAbilityRequest: {
        const auto& e=*c.event_as<ActivateAbilityRequest>();const auto p=actor();
        if(e.ability.card.owner!=p)throw std::logic_error("Wrong ability owner");
        const auto selectedCard=registry_.at(p,card_slot(e.ability.card));
        if(!selectedCard->available()||!selectedCard->usable())throw std::logic_error("Card is unavailable");
        if (selectedCard->definition()==3019) {
            require_turn(p,turn_);
            auto& owner=player(p);
            if (choice_ || e.ability.seq!=1 || !e.selection.players.empty() ||
                usedSpecial_[index(p)].count(3019) || owner.coins<2)
                throw std::logic_error("Cannot activate candy mold");
            std::vector<CardId> eligible;
            for (auto id : owner.hand)
                if (id<=54 && !poker(id).isJoker) eligible.push_back(id);
            if (eligible.size()<2) throw std::logic_error("Need two original cards");
            const auto strength=[&](CardId id){const int r=*poker(id).rank; return r==2?15:r;};
            std::stable_sort(eligible.begin(),eligible.end(),[&](CardId a,CardId b){
                return strength(a)>strength(b);
            });
            cards_.reserve(cards_.size()+2);
            owner.hand.reserve(owner.hand.size()+2);
            out.emit(abilities::spend_coins(owner,2,"双格糖模"));
            for (unsigned i=0;i<2;++i) {
                Poker copy=poker(eligible[i]); copy.id=cards_.size()+1;
                cards_.push_back(copy); owner.hand.push_back(copy.id);
            }
            usedSpecial_[index(p)].insert(3019);
            break;
        }
        if(phase_==GamePhase::Shopping) {
            require_shop(p,shop(p).id);
            if(!selectedCard->subscriptions().test(static_cast<std::size_t>(EventType::ShopActionWindow)))
                throw std::logic_error("Card has no shop activation");
            emit<ShopAbilityActivationEvent>(out,[&](auto& x){x.actor=p;x.ability=e.ability;x.selection=e.selection;});
        } else {
            require_turn(p,turn_);
            if(choice_)throw std::logic_error("Resolve the pending choice first");
            if(!selectedCard->subscriptions().test(static_cast<std::size_t>(EventType::PlayActionWindow)))
                throw std::logic_error("Card has no turn activation");
            emit<TurnAbilityActivationEvent>(out,[&](auto& x){x.actor=p;x.turnId=turn_;x.ability=e.ability;x.selection=e.selection;});
        }break;
    }
    case EventType::StateSyncRequest:
        emit<StateSnapshotReadyControl>(out,[&](auto& e){e.player=actor();e.snapshotId=version_;});break;
    case EventType::PreviewPlayRequest: {
        const auto& e=*c.event_as<PreviewPlayRequest>();const auto p=actor();
        previews_[index(p)]=legal_moves(selected_cards(p,e.cards),table_);
        emit<PlayOptionsReadyControl>(out,[&](auto& x){x.player=p;x.previewId=version_;
            for(std::size_t i=0;i<previews_[index(p)].size();++i)x.options.push_back(i);});break;
    }
    case EventType::QuoteShopOperationRequest: {
        const auto& e=*c.event_as<QuoteShopOperationRequest>();const auto p=actor();require_shop(p,e.shopId);
        CoinValue cost=0;
        if(e.operation==ShopOperation::Refresh)cost=shop(p).freeRefreshes?0:2+static_cast<CoinValue>(shop(p).paidRefreshes);
        else if(e.operation==ShopOperation::HandUpgrade&&e.pattern)cost=upgrade_cost(p,*e.pattern);
        else if(e.operation==ShopOperation::Sale&&e.card)cost=sale_value(p,*e.card);
        else if(e.operation==ShopOperation::Facility&&e.facilityId&&*e.facilityId<9)cost=facilityPrices[*e.facilityId];
        else if(e.operation==ShopOperation::Purchase&&e.productId) {
            const auto& offers=shop(p).offers;auto found=std::find_if(offers.begin(),offers.end(),
                [&](const auto& v){return v.id==*e.productId&&v.id;});
            if(found==offers.end())throw std::invalid_argument("Unknown product");
            cost=found->price;
        } else throw std::invalid_argument("Incomplete quote request");
        emit<ShopQuoteReadyControl>(out,[&](auto& x){x.player=p;x.previewId=version_;x.quote={0,cost,cost,version_};});break;
    }
    case EventType::ShoppingDeadlineControl: {
        const auto& e=*c.event_as<ShoppingDeadlineControl>();
        if(phase_!=GamePhase::Shopping)break;
        for(auto& s:shops_)if(s.id==e.shopId&&!s.done&&e.expectedVersion==round_) {
            s.done=true;const auto p=static_cast<Players>(&s-&shops_[0]);
            emit<ShopLeftEvent>(out,[&](auto& x){x.player=p;x.shopId=s.id;});break;
        }break;
    }
    case EventType::TurnDeadlineControl: {
        const auto& e=*c.event_as<TurnDeadlineControl>();
        if(phase_!=GamePhase::Playing||!awaiting_||e.turnId!=turn_||e.expectedVersion!=turn_||e.player!=acting_)break;
        if(choice_) {
            emit<ChoiceDeadlineControl>(out,[&](auto& x){x.choiceId=choice_->id;x.expectedVersion=turn_;});break;
        }
        if(table_) {
            awaiting_=false;
            emit<PlayerPassedEvent>(out,[&](auto& x){x.actor=acting_;x.player=acting_;x.turnId=turn_;
                x.reason=player(acting_).connected?PassReason::Timeout:PassReason::Disconnected;});
        } else {
            const auto& hand=player(acting_).hand;
            const auto best=*std::min_element(hand.begin(),hand.end(),[&](auto a,auto b){
                const auto strength=[&](auto id){const auto& v=poker(id);
                    return v.isJoker?16:*v.rank==2?15:*v.rank;};
                return strength(a)<strength(b)||(strength(a)==strength(b)&&a<b);
            });
            auto options=legal_moves({poker(best)});
            if(options.empty())throw std::logic_error("No timeout play");
            auto selected=options.begin();
            if(poker(best).isJoker)
                selected=std::find_if(options.begin(),options.end(),[](const auto& option){return option.rank==3;});
            play(acting_,{best},*selected,out);
        }break;
    }
    case EventType::ChoiceDeadlineControl: {
        const auto& e=*c.event_as<ChoiceDeadlineControl>();
        if(!choice_||e.choiceId!=choice_->id||e.expectedVersion!=turn_)break;
        const auto choice=*choice_;
        const auto selected=std::uniform_int_distribution<std::size_t>(0,choice.options.size()-1)(random_);
        play(choice.player,choice.cards,choice.options[selected],out);break;
    }
    case EventType::PlayerDisconnectedControl: {
        const auto& e=*c.event_as<PlayerDisconnectedControl>();auto& p=player(e.player);
        if(e.connectionVersion<p.connectionGeneration)break;
        p.connected=false;p.connectionGeneration=e.connectionVersion;
        if(phase_==GamePhase::Playing&&acting_==p.id&&awaiting_)
            emit<TurnDeadlineControl>(out,[&](auto& x){x.turnId=turn_;x.player=p.id;x.expectedVersion=turn_;});
        break;
    }
    case EventType::PlayerReconnectedControl: {
        const auto& e=*c.event_as<PlayerReconnectedControl>();auto& p=player(e.player);
        if(e.connectionVersion<p.connectionGeneration)break;
        p.connected=true;p.connectionGeneration=e.connectionVersion;break;
    }
    case EventType::ContinueGameRequest: {
        const auto& e=*c.event_as<ContinueGameRequest>();const auto p=actor();
        if(phase_!=GamePhase::Finished)throw std::logic_error("Game is not finished");
        if(e.mode!=ContinueMode::ExtraThree&&e.mode!=ContinueMode::Endless)throw std::invalid_argument("Unknown continuation mode");
        continueVotes_[index(p)]=e.mode;
        if(std::all_of(continueVotes_.begin(),continueVotes_.end(),[&](const auto& v){return v&&*v==e.mode;})) {
            config_.rounds=e.mode==ContinueMode::ExtraThree?static_cast<std::size_t>(round_+3):std::numeric_limits<std::size_t>::max();
            continueVotes_.fill(std::nullopt);newGameVotes_.fill(false);++round_;open_shops(out);
        }break;
    }
    case EventType::StartNewGameRequest: {
        if(phase_!=GamePhase::Finished)throw std::logic_error("Game is not finished");
        newGameVotes_[index(actor())]=true;
        if(std::all_of(newGameVotes_.begin(),newGameVotes_.end(),[](bool v){return v;})) {
            for(auto& p:players_) {
                p.coins=6;p.score=0;p.hand.clear();p.upgrades.fill(0);p.facilities.clear();p.functionSlots=4;
                p.extraShopSlots=0;p.shopLevel=1;
                for(std::size_t slot=0;slot<MAX_FUNCTION_SLOTS;++slot)registry_.unequip(p.id,slot);
            }
            hammerUsed_={}; hammerHit_={}; candyChoice_.reset();
            paidCosts_={};shops_={};continueVotes_.fill(std::nullopt);newGameVotes_.fill(false);
            round_=1;config_.rounds=8;roundLocked_=false;table_.reset();choice_.reset();
            first_=static_cast<Players>(std::uniform_int_distribution<int>(0,3)(random_));
            emit<GameStartedEvent>(out,[](auto&) {});
        }break;
    }
    default:
        // 事实通知和资源系统牌已处理的事件无需在此重复操作。
        if(static_cast<std::size_t>(type)>=63&&static_cast<std::size_t>(type)<=83)
            throw std::invalid_argument("This request requires a configured business adapter");
        break;
    }
}
void Scheduler::timeout() {
    DispatchMeta meta;meta.origin=EventOrigin::Timer;
    if(phase_==GamePhase::Shopping) {
        for(std::size_t i=0;i<4&&phase_==GamePhase::Shopping;++i)if(!shops_[i].done) {
            auto e=std::make_unique<ShoppingDeadlineControl>();e->shopId=shops_[i].id;e->expectedVersion=round_;
            dispatch(std::move(e),meta);
        }
    } else if(candyChoice_) {
        auto e=std::make_unique<SubmitChoiceRequest>();
        e->actor=candyChoice_->responder(); e->choiceId=candyChoice_->id; e->options={0};
        dispatch(std::move(e),meta);
    } else if(phase_==GamePhase::Playing&&awaiting_) {
        auto e=std::make_unique<TurnDeadlineControl>();e->player=acting_;e->turnId=turn_;e->expectedVersion=turn_;
        dispatch(std::move(e),meta);
    }
}
void Scheduler::tick(std::chrono::steady_clock::time_point now) {
    now_=now;if(now_>=deadline_)timeout();
}
void Scheduler::disconnected(Players p) {
    auto e=std::make_unique<PlayerDisconnectedControl>();e->player=p;
    e->connectionVersion=player(p).connectionGeneration+1;
    DispatchMeta m;m.origin=EventOrigin::Connection;dispatch(std::move(e),m);
}
void Scheduler::reconnected(Players p) {
    auto e=std::make_unique<PlayerReconnectedControl>();e->player=p;
    e->connectionVersion=player(p).connectionGeneration+1;
    DispatchMeta m;m.origin=EventOrigin::Connection;dispatch(std::move(e),m);
}
} // namespace cardgame

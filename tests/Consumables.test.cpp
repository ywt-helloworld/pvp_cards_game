#include "Scheduler.hpp"
#include "network/Protocol.hpp"
#include <cassert>
#include <iostream>
#include <limits>
using namespace cardgame;
namespace {
std::array<Player,4> players(){return {Player(Players::P1,"one"),Player(Players::P2,"two"),Player(Players::P3,"three"),Player(Players::P4,"four")};}
SchedulerConfig config(ConsumableEffect effect) {
    SchedulerConfig c;c.seed=2;c.consumableChance=1;
    for(DefinitionId id=1;id<=6;++id)c.catalog.push_back({id,"test",1,
        [id](Player& p,std::size_t seq){return std::make_shared<ResourceBonusCard>(p,seq,id,"test",EventType::PlayCommitted,1,0);},id<=4?1u:id==5?2u:3u});
    c.consumables.push_back({500,"consumable",2,"test effect",std::move(effect)});return c;
}
DispatchReport buy(Scheduler& g,Players p,std::size_t slot,std::optional<CardRef> target={}) {
    auto e=std::make_unique<BuyShopItemRequest>();e->actor=p;e->shopId=g.shop(p).id;
    e->slotId=slot;e->productId=g.shop(p).offers[slot].id;e->targetCard=target;return g.submit(std::move(e));
}
void shop_capacity_and_failure() {
    Scheduler g(players(),config(ExpandShopSlots{1}));const auto p=Players::P1;
    const auto offer=g.shop(p).offers[2];assert(offer.kind==ShopProductKind::Consumable);
    for(std::size_t i=0;i<g.player(p).functionSlots;++i)
        g.registry().equip(p,i,std::make_shared<ResourceBonusCard>(g.player(p),100+i,80+i,"full",EventType::PlayCommitted,1,0));
    g.player(p).coins=1;
    assert(!buy(g,p,2).failures.empty());assert(g.player(p).coins==1&&g.shop(p).offers.size()==3&&g.shop(p).offers[2].id==offer.id);
    g.player(p).coins=6;bool observed=false;
    g.registry().add_system(std::make_shared<CallbackFunctionCard>(999001,"watch",FunctionCards::listens_to({EventType::ConsumableUsed}),
        [&](const Context& c,CardEffects&){assert(c.event_as<ConsumableUsedEvent>()->definition==500);
            assert(g.player(p).coins==4&&g.shop(p).offers.size()==4&&!g.shop(p).offers[2].id);observed=true;}));
    assert(buy(g,p,2).failures.empty());assert(observed);
    assert(g.player(p).extraShopSlots==1&&g.registry().player_cards(p).size()==4&&g.registry().matrix().size()==28);
    auto repeat=std::make_unique<BuyShopItemRequest>();repeat->actor=p;repeat->shopId=g.shop(p).id;repeat->slotId=2;repeat->productId=offer.id;
    assert(!g.submit(std::move(repeat)).failures.empty());assert(g.player(p).coins==4&&g.shop(p).offers.size()==4);
    // 跨局保留，升级/加赛不是新游戏；完全重开应重置容量和等级。
    for(int i=0;i<20000&&g.round()==1;++i)g.timeout();
    assert(g.round()==2&&g.phase()==GamePhase::Shopping&&g.shop(p).offers.size()==4);
    while(g.phase()!=GamePhase::Finished)g.timeout();
    for(unsigned i=0;i<4;++i){auto e=std::make_unique<StartNewGameRequest>();e->actor=static_cast<Players>(i);assert(g.submit(std::move(e)).failures.empty());}
    assert(g.shop(p).offers.size()==3&&g.player(p).extraShopSlots==0&&g.player(p).shopLevel==1);
}
void shop_level_and_caps() {
    Scheduler g(players(),config(UpgradeShop{1}));const auto p=Players::P1;
    assert(g.shop(p).offers[0].definition<=4&&g.shop(p).offers[1].definition<=4);
    assert(buy(g,p,2).failures.empty());assert(g.player(p).shopLevel==2&&g.player(Players::P2).shopLevel==1);
    bool unlocked=false;
    for(int i=0;i<5;++i){g.player(p).coins=100;auto e=std::make_unique<RefreshShopRequest>();e->actor=p;e->shopId=g.shop(p).id;assert(g.submit(std::move(e)).failures.empty());
        for(const auto& o:g.shop(p).offers){assert(o.definition!=6);if(o.definition==5)unlocked=true;}}
    assert(unlocked);
    Scheduler cap(players(),config(ExpandShopSlots{4}));
    for(const auto& o:cap.shop(p).offers)assert(o.kind!=ShopProductKind::Consumable); // 超过容量的商品不生成。
}
void numeric_target_checks() {
    FunctionCardEnhancement upgrade;upgrade.scoreDelta=2;
    Scheduler g(players(),config(EnhanceFunctionCard{upgrade}));const auto p=Players::P1;
    assert(g.shop(p).offers[2].kind==ShopProductKind::FunctionCard); // 没有目标，不生成强化牌。
    assert(buy(g,p,0).failures.empty());
    auto card=g.registry().at(p,0);const auto ref=*card->reference();
    auto e=std::make_unique<RefreshShopRequest>();e->actor=p;e->shopId=g.shop(p).id;assert(g.submit(std::move(e)).failures.empty());
    const auto product=g.shop(p).offers[2].id;const auto coins=g.player(p).coins;
    assert(g.shop(p).offers[2].needsTarget);
    assert(network::state_message(g,p).find("\"needsTarget\":true")!=std::string::npos);
    assert(!buy(g,p,2).failures.empty());assert(!buy(g,p,2,CardRef{Players::P2,ref.seq}).failures.empty());
    assert(!buy(g,p,2,CardRef{p,9999}).failures.empty());
    assert(g.player(p).coins==coins&&g.shop(p).offers[2].id==product&&card->enhancement_level()==0);
    assert(buy(g,p,2,ref).failures.empty());assert(g.registry().at(p,0)==card&&card->enhancement_level()==1);
    assert(std::dynamic_pointer_cast<ResourceBonusCard>(card)->score_bonus()==3);
    assert(network::state_message(g,p).find("\"enhancementLevel\":1")!=std::string::npos);
    auto parsed=network::parse_request(p,"BUY 1 2 3 7");assert(static_cast<BuyShopItemRequest&>(*parsed).targetCard->seq==7);
    g.player(p).score=0;
    auto played=std::make_unique<CardsPlayedEvent>();played->actor=p;played->player=p;
    // 独立注册表只验证这张牌的新数值，不重复调用完整游戏流程系统牌。
    Register isolated;isolated.equip(p,0,card);Condition c;HandleEvent h(isolated,c);
    assert(h.handle_event(std::move(played)).failures.empty());assert(g.player(p).score==3);
}
void added_skill_and_atomic_validation() {
    auto p=players();Register registry;Condition condition;HandleEvent handle(registry,condition);
    auto card=std::make_shared<ResourceBonusCard>(p[0],1,1,"card",EventType::PlayCommitted,1,0);registry.equip(Players::P1,0,card);
    const auto token=registry.matching(EventType::PlayCommitted,{Players::P1})[0];
    FunctionCardEnhancement enhancement;
    enhancement.skills.push_back({FunctionCards::listens_to({EventType::PassCommitted}),
        [&](const Context&,CardEffects& out){out.emit(abilities::gain_coins(p[0],2));},"pass +2 coins"});
    auto fact=abilities::enhance_function_card(registry,Players::P1,0,enhancement);
    assert(fact->type==EventType::FunctionCardEnhanced&&registry.current(token));
    assert(registry.matrix()[0].test(static_cast<std::size_t>(EventType::PassCommitted)));
    auto pass=std::make_unique<PlayerPassedEvent>();pass->actor=Players::P1;pass->player=Players::P1;
    assert(handle.handle_event(std::move(pass)).failures.empty());assert(p[0].coins==8&&p[0].score==0); // 不能误触发原来的出牌技能。
    FunctionCardEnhancement bad;bad.scoreDelta=std::numeric_limits<ScoreValue>::max();bad.skills=enhancement.skills;
    bool rejected=false;try{registry.enhance(Players::P1,0,bad);}catch(const std::overflow_error&){rejected=true;}
    assert(rejected&&card->enhancement_level()==1&&card->score_bonus()==1);
    registry.unequip(Players::P1,0);assert(registry.matrix()[0].none());
}
void facilities_compose_with_consumables() {
    bool checked=false;
    for(unsigned seed=0;seed<30&&!checked;++seed) {
        auto initial=players();initial[0].facilities.insert(1);
        auto c=config(ExpandShopSlots{1});c.seed=seed;Scheduler g(std::move(initial),c);
        const auto p=Players::P1;if(g.shop(p).facility!=2)continue;
        g.player(p).coins=100;
        assert(g.shop(p).offers.size()==4&&buy(g,p,2).failures.empty());
        assert(g.shop(p).offers.size()==5);
        const auto before=g.shop(p).offers;
        auto purchase=std::make_unique<BuyFacilityRequest>();purchase->actor=p;purchase->shopId=g.shop(p).id;purchase->facilityId=2;
        assert(g.submit(std::move(purchase)).failures.empty());assert(g.shop(p).offers.size()==6);
        for(std::size_t i=0;i<before.size();++i)assert(g.shop(p).offers[i].id==before[i].id);
        checked=true;
    }
    assert(checked);
}
}
int main(){shop_capacity_and_failure();shop_level_and_caps();numeric_target_checks();added_skill_and_atomic_validation();facilities_compose_with_consumables();std::cout<<"Consumables: immediate use, persistence, caps, targets, numeric/skill enhancements and registration passed.\n";}

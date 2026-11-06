#include "Scheduler.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <set>
#include <string>
using namespace cardgame;
namespace {
template<class E,class F>void error(F call){bool caught=false;try{call();}catch(const E&){caught=true;}assert(caught);}
std::array<Player,4> players(){return {Player(Players::P1,"one"),Player(Players::P2,"two"),Player(Players::P3,"three"),Player(Players::P4,"four")};}
void atomic_operations(){
 auto p=Player(Players::P1,"p");auto score=abilities::add_score(p,3);
 assert(score->type==EventType::ScoreChanged&&p.score==3&&p.coins==6);
 auto coins=abilities::gain_coins(p,2);assert(coins->type==EventType::CoinsChanged&&p.coins==8);
 error<std::domain_error>([&]{abilities::spend_coins(p,9);});assert(p.coins==8);
 p.score=std::numeric_limits<ScoreValue>::max();
 error<std::overflow_error>([&]{abilities::add_score(p,1);});
 assert(p.score==std::numeric_limits<ScoreValue>::max());
 RewardData r{1,-5,2};abilities::multiply_pending_score(r,1,2,RoundingRule::Floor);assert(r.score==-3);
 ShopData q{1,5,5,1};abilities::reduce_shop_cost(q,20,1);assert(q.finalCost==1);
 bool called=false;
 auto no=abilities::change_card_rank(1,7,Lifetime::Round,
   [&](CardId id,int rank,Lifetime lifetime,PlayingCardsChangedEvent&){called=id==1&&rank==7&&lifetime==Lifetime::Round;return false;});
 assert(called&&!no);
 const PlayerOrder order{Players::P1,Players::P2,Players::P3,Players::P4};
 auto deal=abilities::deal_initial_cards(4,order,7,[&](RoundId round,const PlayerOrder& list,int n){return round==4&&list==order&&n==7;});
 assert(deal&&deal->type==EventType::InitialDealAfter);
 auto drawn=abilities::draw_to_all(order,1,DrawReason::TrickEnd,
    [](const PlayerOrder&,std::size_t,DrawReason,std::vector<std::vector<CardId>>& result){
        result[0]={1};result[1]={2};return true;});
 assert(drawn.size()==2);
}
void per_card_queue_and_dynamic_matrix(){
 auto p=players();Register registry;Condition condition;HandleEvent handle(registry,condition);
 std::vector<std::string> trace;
 auto a=std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,1},1,"a",
 FunctionCards::listens_to({EventType::RoundStart}),
 [&](const Context&,CardEffects& out){
   trace.push_back("a-score");out.emit(abilities::add_score(p[0],1),1);
   trace.push_back("a-coins");out.emit(abilities::gain_coins(p[0],1),2);
 });
 registry.equip(Players::P1,0,a);
 registry.equip(Players::P1,1,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,2},2,"b",
 FunctionCards::listens_to({EventType::RoundStart}),
 [&](const Context& c,CardEffects&){assert(c.type()==EventType::RoundStart);trace.push_back("b");}));
 registry.equip(Players::P2,0,std::make_shared<CallbackFunctionCard>(CardRef{Players::P2,1},3,"watch",
 FunctionCards::listens_to({EventType::ScoreChanged,EventType::CoinsChanged}),
 [&](const Context& c,CardEffects&){
   assert(p[0].score==1&&p[0].coins==7);
   assert(c.metadata()->originAbility&&c.metadata()->originAbility->card.seq==1);
   trace.push_back(c.type()==EventType::ScoreChanged?"score-event":"coins-event");
 }));
 assert(registry.matrix().size()==24&&registry.matrix()[0].test(10));
 auto report=handle.handle_event(std::make_unique<RoundStartedEvent>());
 assert(report.failures.empty());
 assert(trace==std::vector<std::string>({"a-score","a-coins","score-event","coins-event","b"}));
 assert(condition.context().type()==EventType::RoundStart);
 auto sys=std::make_shared<CallbackFunctionCard>(100,"hidden",FunctionCards::listens_to({EventType::GameStart}),
 [](const Context&,CardEffects&){});
 auto row=registry.add_system(sys);assert(row==24&&registry.matrix().size()==25&&!sys->visible_to_players());
 assert(registry.player_cards(Players::P1).size()==2);
 registry.remove_system(row);assert(registry.matrix()[24].none());
 auto token=registry.matching(EventType::RoundStart,{Players::P1})[0];
 registry.move(Players::P1,0,5);assert(registry.current(token)&&registry.matrix()[5].test(10)&&registry.matrix()[0].none());
 registry.unequip(Players::P1,5);assert(!registry.current(token));
 error<std::out_of_range>([&]{Register::row_of(Players::P1,6);});
}
void mutations_and_failure(){
 auto p=players();Register registry;Condition condition;HandleEvent handle(registry,condition);
 int removed=0,added=0,notified=0;
 auto late=std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,3},3,"new",
 FunctionCards::listens_to({EventType::RoundStart}),[&](const Context&,CardEffects&){++added;});
 registry.equip(Players::P1,0,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,1},1,"first",
 FunctionCards::listens_to({EventType::RoundStart}),[&](const Context&,CardEffects& out){
   if(registry.at(Players::P1,1)!=late)registry.replace(Players::P1,1,late);
   out.emit(abilities::add_score(p[0],1));
   throw std::runtime_error("second primitive failed");
 }));
 registry.equip(Players::P1,1,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,2},2,"old",
 FunctionCards::listens_to({EventType::RoundStart}),[&](const Context&,CardEffects&){++removed;}));
 registry.add_system(std::make_shared<CallbackFunctionCard>(101,"facts",
 FunctionCards::listens_to({EventType::ScoreChanged}),[&](const Context&,CardEffects&){++notified;}));
 auto report=handle.handle_event(std::make_unique<RoundStartedEvent>());
 assert(report.failures.size()==1&&removed==0&&added==0&&notified==1&&p[0].score==1);
 report=handle.handle_event(std::make_unique<RoundStartedEvent>());
 assert(added==1&&notified==2);
 Register cycles;Condition second;HandleEvent limited(cycles,second,{Players::P1},8);
 cycles.equip(Players::P1,0,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,9},9,"cycle",
 FunctionCards::listens_to({EventType::ScoreChanged}),[&](const Context&,CardEffects& out){out.emit(abilities::add_score(p[0],1));}));
 error<std::length_error>([&]{limited.handle_event(std::make_unique<ScoreChangedEvent>());});
}
void poker_rules(){
 auto deck=standard_deck();
 auto get=[&](int rank,Suit suit=Suit::Spades){for(const auto& p:deck)if(p.rank==rank&&p.suit==suit)return p;throw std::logic_error("missing");};
 auto options=legal_moves({get(14),get(2),get(3)});assert(!options.empty()&&options[0].rank==1);
 assert(legal_moves({get(13),get(14),get(2)}).empty());
 MoveOption three{HandPattern::Single,3,1,{}};
 assert(!legal_moves({get(4)},three).empty());assert(legal_moves({get(5)},three).empty());
 MoveOption two{HandPattern::Single,15,1,{}};assert(legal_moves({get(2)},two).empty());
 assert(!legal_moves({get(7),get(7,Suit::Hearts),get(7,Suit::Clubs)},two).empty());
 auto joker=legal_moves({deck[52]});assert(joker.size()==52);
 std::vector<WildcardAssignment> chosen{{53,2,Suit::Diamonds}};
 auto selected=legal_moves({deck[52]},three,&chosen);assert(selected.size()==1&&selected[0].rank==15);
 MoveOption straight{HandPattern::Straight,3,3,{}};
 assert(!legal_moves({get(4),get(5),get(6)},straight).empty());
}

void snapshots_active_and_shop_services(){
 auto ps=players();Register registry;Condition condition;HandleEvent handle(registry,condition);
 int invoked=0;
 auto mask=FunctionCards::listens_to({EventType::ShopActionWindow,EventType::PlayRewardBefore});
 registry.equip(Players::P1,0,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,1},1,"first",mask,
 [&](const Context& c,CardEffects& out){
   ++invoked;
   if(c.type()==EventType::PlayRewardBefore){abilities::multiply_pending_score(out.reward(),2,1);out.emit(std::make_unique<ScoreChangedEvent>());}
 },true));
 registry.equip(Players::P1,1,std::make_shared<CallbackFunctionCard>(CardRef{Players::P1,2},2,"second",mask,
 [&](const Context& c,CardEffects&){
   assert(c.type()==EventType::PlayRewardBefore&&c.reward()->score==20);
   assert(c.event_as<BeforePlayRewardEvent>()->initialReward.score==10);++invoked;
 },true));
 auto active=std::make_unique<ShopAbilityActivationEvent>();active->ability={{Players::P1,1},3};
 auto report=handle.handle_event(std::move(active));assert(report.failures.empty()&&invoked==1);
 auto reward=std::make_unique<BeforePlayRewardEvent>();reward->initialReward={1,10,2};
 report=handle.handle_event(std::move(reward));assert(report.failures.empty()&&invoked==3);
 assert(condition.context().reward()->score==20);
 SchedulerConfig config;config.seed=19;Scheduler game(players(),config);
 const auto p=Players::P1;const auto shopId=game.shop(p).id;
 auto offers=game.shop(p).offers;const auto before=game.player(p).coins;
 auto buy=std::make_unique<BuyShopItemRequest>();buy->actor=p;buy->shopId=shopId;buy->slotId=0;buy->productId=offers[0].id;
 report=game.submit(std::move(buy));assert(report.failures.empty());
 assert(!game.shop(p).offers[0].id&&game.player(p).coins==before-offers[0].price);
 const auto owned=game.registry().at(p,0);assert(owned&&game.registry().matrix()[0].any());
 auto sell=std::make_unique<SellFunctionCardRequest>();sell->actor=p;sell->shopId=shopId;sell->card=*owned->reference();
 report=game.submit(std::move(sell));assert(report.failures.empty());
 assert(!game.registry().at(p,0)&&game.registry().matrix()[0].none());
 assert(game.player(p).coins==before-offers[0].price+offers[0].price/2);
 game.player(p).coins=200;
 auto lock=std::make_unique<LockShopSlotRequest>();lock->actor=p;lock->shopId=shopId;lock->slotId=1;
 assert(game.submit(std::move(lock)).failures.empty());
 const auto locked=game.shop(p).offers[1].id;
 for(int i=0;i<5;++i){
   const auto coins=game.player(p).coins;
   auto refresh=std::make_unique<RefreshShopRequest>();refresh->actor=p;refresh->shopId=shopId;
   assert(game.submit(std::move(refresh)).failures.empty());
   assert(game.player(p).coins==coins-(i+2));
   assert(game.shop(p).offers[1].id==locked&&!game.shop(p).offers[0].id);
 }
 auto refresh=std::make_unique<RefreshShopRequest>();refresh->actor=p;refresh->shopId=shopId;
 assert(game.submit(std::move(refresh)).failures.size()==1&&game.shop(p).refreshes==5);
 auto upgrade=std::make_unique<UpgradeHandPatternRequest>();upgrade->actor=p;upgrade->shopId=shopId;upgrade->pattern=HandPattern::Single;
 assert(game.submit(std::move(upgrade)).failures.empty()&&game.player(p).upgrades[0]==1);
 const auto facility=game.shop(p).facility;
 if(facility){
   const auto previous=game.shop(p).offers;
   const auto slots=game.player(p).functionSlots;
   auto request=std::make_unique<BuyFacilityRequest>();request->actor=p;request->shopId=shopId;request->facilityId=facility;
   assert(game.submit(std::move(request)).failures.empty());
   assert(game.player(p).facilities.count(facility));
   if(facility==1||facility==2)
     for(std::size_t i=0;i<previous.size();++i)assert(game.shop(p).offers[i].id==previous[i].id);
   if(facility==3||facility==4)assert(game.player(p).functionSlots==slots+1);
 }
 auto unsupported=std::make_unique<BuyBoosterPackRequest>();unsupported->actor=p;unsupported->shopId=shopId;
 error<std::invalid_argument>([&]{game.submit(std::move(unsupported));});
}

void scheduler_full_game(){
 SchedulerConfig config;config.seed=77;
 Scheduler game(players(),config);
 assert(game.phase()==GamePhase::Shopping&&game.registry().matrix().size()==27);
 bool locked=false;std::vector<Players> starters;int deals=0;
 EventMask all;all.set();
 game.registry().add_system(std::make_shared<CallbackFunctionCard>(9999,"audit",all,
 [&](const Context& c,CardEffects&){
   if(c.type()==EventType::RoundStart){assert(!c.actor());locked=false;starters.push_back(c.event_as<RoundStartedEvent>()->firstPlayer);}
   if(c.type()==EventType::RoundEnd||c.type()==EventType::GameEnd)assert(!c.actor());
   if(c.type()==EventType::RoundWinnerLocked)locked=true;
   if(c.type()==EventType::TrickEnd||c.type()==EventType::DrawAfter)assert(!locked);
   if(c.type()==EventType::InitialDealAfter){++deals;for(auto p:{Players::P1,Players::P2,Players::P3,Players::P4})assert(game.player(p).hand.size()==7);}
 }));
 std::size_t steps=0;
 while(game.phase()!=GamePhase::Finished&&steps++<5000){
   if(game.phase()==GamePhase::Shopping){
     for(auto p:{Players::P1,Players::P2,Players::P3,Players::P4}){
       if(game.phase()!=GamePhase::Shopping)break;
       auto e=std::make_unique<FinishShoppingRequest>();e->actor=p;e->shopId=game.shop(p).id;
       const auto report=game.submit(std::move(e));assert(report.failures.empty());
     }
   } else game.timeout();
   if(game.phase()==GamePhase::Playing){
     std::set<CardId> seen;
     for(auto p:{Players::P1,Players::P2,Players::P3,Players::P4})
       for(auto id:game.player(p).hand)assert(seen.insert(id).second);
     for(auto id:game.discard())assert(seen.insert(id).second);
     assert(seen.size()+game.deck_remaining()==54);
   }
 }
 assert(game.phase()==GamePhase::Finished&&game.round()==8&&deals==8&&starters.size()==8);
 for(std::size_t i=0;i<8;++i)assert(starters[i]==starters[i<4?i:7-i]);
 assert(!game.winners().empty());
 for(auto p:{Players::P1,Players::P2,Players::P3,Players::P4})assert(game.player(p).coins>=210&&game.player(p).score>0);
}
}
int main(){
 atomic_operations();per_card_queue_and_dynamic_matrix();mutations_and_failure();poker_rules();snapshots_active_and_shop_services();scheduler_full_game();
 std::cout<<"Card pipeline: atomic operations, dynamic registration, deferred chains, poker rules and complete 8-round server flow passed.\n";
}

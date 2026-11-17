#include "Scheduler.hpp"
#include "network/Protocol.hpp"
#include <cassert>
#include <iostream>

using namespace cardgame;
namespace {
std::array<Player,4> seats() {
    return {Player(Players::P1,"a"),Player(Players::P2,"b"),Player(Players::P3,"c"),Player(Players::P4,"d")};
}
void equip(Scheduler& game, Players owner, std::size_t slot, DefinitionId id) {
    game.registry().equip(owner,slot,std::make_shared<SpecialtyCard>(
        CardRef{owner,game.next_card_sequence(owner)},id,"test","test"));
}
void play(Scheduler& game, std::vector<CardId> hand, std::vector<CardId> move) {
    const auto seat=game.acting_player();
    game.player(seat).hand=std::move(hand);
    auto e=std::make_unique<PlayCardsRequest>();
    e->actor=seat;e->turnId=game.turn();e->cards=std::move(move);
    assert(game.submit(std::move(e)).failures.empty());
}
void answer(Scheduler& game,unsigned option) {
    const auto pending=game.candy_choice().value();
    auto req=std::make_unique<SubmitChoiceRequest>();
    req->actor=pending.responder();req->choiceId=pending.id;req->options={option};
    assert(game.submit(std::move(req)).failures.empty());
}
void family_upgrade() {
    bool tested=false;
    for (unsigned seed=0;seed<1000 && !tested;++seed) {
        auto initial=seats();initial[0].shopLevel=3;initial[0].coins=50;
        initial[0].functionSlots=1;
        SchedulerConfig config;config.seed=seed;config.consumableChance=0;
        Scheduler game(std::move(initial),config);
        const auto seat=Players::P1;
        const auto& shop=game.shop(seat);
        auto slot=[&](DefinitionId id){
            return std::find_if(shop.offers.begin(),shop.offers.end(),[&](const auto& item){return item.definition==id;});
        };
        const auto base=slot(1),higher=slot(7);
        if(base==shop.offers.end() || higher==shop.offers.end()) continue;
        auto purchase=[&](const auto& it){
            auto buy=std::make_unique<BuyShopItemRequest>();
            buy->actor=seat;buy->shopId=shop.id;buy->slotId=it-shop.offers.begin();buy->productId=it->id;
            assert(game.submit(std::move(buy)).failures.empty());
        };
        purchase(base);
        FunctionCardEnhancement enhancement;enhancement.scoreDelta=1;
        game.registry().enhance(seat,0,enhancement);
        assert(higher->upgrade && higher->price==3);
        purchase(higher);
        auto upgraded=std::dynamic_pointer_cast<ResourceBonusCard>(game.registry().at(seat,0));
        assert(upgraded && upgraded->definition()==7 && upgraded->score_bonus()==3);
        assert(upgraded->enhancement_level()==1 && game.player(seat).coins==43);
        auto sell=std::make_unique<SellFunctionCardRequest>();
        sell->actor=seat;sell->shopId=shop.id;sell->card=*upgraded->reference();
        assert(game.submit(std::move(sell)).failures.empty());
        assert(game.player(seat).coins==46);
        tested=true;
    }
    assert(tested);
}
void mold() {
    Scheduler game(seats());game.timeout();
    auto seat=game.acting_player();auto& owner=game.player(seat);
    owner.hand={1,53,49,45};owner.coins=2;
    equip(game,seat,0,3019);
    const auto ref=*game.registry().at(seat,0)->reference();
    auto activate=[&]{
        auto req=std::make_unique<ActivateAbilityRequest>();req->actor=seat;req->ability={ref,1};
        return game.submit(std::move(req)).failures.empty();
    };
    assert(activate() && owner.coins==0 && owner.hand.size()==6);
    assert(game.poker(owner.hand[4]).rank==2 && game.poker(owner.hand[5]).rank==14);
    owner.coins=10;assert(!activate() && owner.coins==10);
    const auto round=game.round();
    while(game.round()==round) game.timeout();
    for(auto card:owner.hand) assert(card<=54);
}
void steal(bool accept) {
    Scheduler game(seats());game.timeout();
    const auto defender=game.acting_player();
    play(game,{1,2,3,53},{1,2});
    const auto attacker=game.acting_player();equip(game,attacker,0,3020);equip(game,attacker,1,3014);
    game.player(defender).coins=1;
    auto coins=game.player(attacker).coins;
    play(game,{5,6,30},{5,6});
    assert(game.candy_choice() && game.candy_choice()->responder()==attacker);
    assert(game.player(defender).coins==0 && game.player(attacker).coins==coins+3);
    answer(game,accept?1:0);
    assert(!game.candy_choice() && game.player(attacker).hand.size()==(accept?2u:1u));
    assert(game.player(defender).hand.size()==(accept?1u:2u));
    if(accept) assert(game.player(attacker).hand.back()==3);
}
void hammer(bool timeout) {
    Scheduler game(seats());game.timeout();const auto defender=game.acting_player();
    equip(game,defender,0,1);equip(game,defender,1,2);
    play(game,{1,5,9,23},{1,5,9});
    auto attacker=game.acting_player();equip(game,attacker,0,3021);
    play(game,{6,10,14,33},{6,10,14});
    answer(game,1);
    assert(game.candy_choice()->responder()==defender);
    if(timeout) game.timeout();else answer(game,1);
    assert(!game.candy_choice());
    assert(!game.registry().at(defender,timeout?0:1));
    assert(game.registry().at(defender,timeout?1:0));
}
}
int main() {
    family_upgrade();mold();steal(true);steal(false);hammer(true);hammer(false);
    std::cout<<"Candy action and choice scenarios passed.\n";
}

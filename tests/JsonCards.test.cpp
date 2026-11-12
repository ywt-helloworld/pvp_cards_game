#include "CardCatalog.hpp"
#include "cards/generated/steady_income.hpp"
#include <cassert>
#include <iostream>
using namespace cardgame;
int main() {
    Player p(Players::P1,"owner");Register registry;Condition condition;HandleEvent handle(registry,condition);
    const auto catalog=generated_function_cards();
    const auto definition=std::find_if(catalog.begin(),catalog.end(),[](const auto& c){return c.id==2001;});
    assert(definition!=catalog.end()&&definition->minimumShopLevel==2&&!definition->description.empty());
    auto card=definition->create(p,7);registry.equip(p.id,0,card);
    int facts=0;
    registry.add_system(std::make_shared<CallbackFunctionCard>(999123,"observer",
        FunctionCards::listens_to({EventType::ScoreChanged,EventType::CoinsChanged}),
        [&](const Context& c,CardEffects&){
            if(facts<2)assert(p.score==2&&p.coins==7); // 同牌的两个技能完成后才处理第一个通知。
            assert(c.metadata()->originAbility&&c.metadata()->originAbility->seq>=1);++facts;
        }));
    auto play=[&](Players actor,ExecutionMode mode=ExecutionMode::Commit) {
        auto event=std::make_unique<CardsPlayedEvent>();event->player=actor;event->actor=actor;
        DispatchMeta meta;meta.mode=mode;return handle.handle_event(std::move(event),meta);
    };
    assert(play(Players::P2).failures.empty());assert(p.score==0&&p.coins==6);
    assert(play(p.id,ExecutionMode::Preview).failures.empty());assert(p.score==0&&p.coins==6);
    assert(play(p.id).failures.empty());assert(p.score==2&&p.coins==7&&facts==2);
    auto pass=std::make_unique<PlayerPassedEvent>();pass->player=p.id;pass->actor=p.id;
    assert(handle.handle_event(std::move(pass)).failures.empty());assert(p.coins==8&&p.score==2);
    FunctionCardEnhancement upgrade;upgrade.scoreDelta=1;upgrade.coinsDelta=1;
    auto fact=registry.enhance(p.id,0,upgrade);assert(fact->type==EventType::FunctionCardEnhanced);
    assert(play(p.id).failures.empty());assert(p.score==5&&p.coins==10);
    assert(card->reference()->seq==7&&card->enhancement_level()==1);
    bool purchased=false;
    for(unsigned seed=0;seed<30&&!purchased;++seed) {
        std::array<Player,4> initial{Player(Players::P1,"one"),Player(Players::P2,"two"),Player(Players::P3,"three"),Player(Players::P4,"four")};
        initial[0].shopLevel=2;SchedulerConfig config;config.seed=seed;config.consumableChance=0;
        Scheduler game(std::move(initial),config);
        const auto& offers=game.shop(p.id).offers;
        const auto offer=std::find_if(offers.begin(),offers.end(),[](const auto& o){return o.definition==2001;});
        if(offer==offers.end())continue;
        assert(offer->description==definition->description);
        auto buy=std::make_unique<BuyShopItemRequest>();buy->actor=p.id;buy->shopId=game.shop(p.id).id;
        buy->slotId=static_cast<std::size_t>(offer-offers.begin());buy->productId=offer->id;
        assert(game.submit(std::move(buy)).failures.empty());
        assert(game.registry().at(p.id,0)->definition()==2001);purchased=true;
    }
    assert(purchased); // 默认卡池自动注册、货架生成和购买的实际路径。
    std::cout<<"JSON cards: compiled factory, scope, atomic actions, preview and enhancement passed.\n";
}

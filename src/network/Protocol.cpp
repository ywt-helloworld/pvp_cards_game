#include "network/Protocol.hpp"
#include "network/ListenEvent.hpp"
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace cardgame::network {
std::string json_string(const std::string& value) {
    std::ostringstream out;out<<'"';
    for (unsigned char c:value) {
        if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
        else out<<static_cast<char>(c);
    }
    out<<'"';return out.str();
}
namespace {
void moves(std::ostringstream& out,const std::vector<MoveOption>& options) {
    out<<'[';
    for(std::size_t i=0;i<options.size();++i) {
        if(i) { out<<','; }
        const auto& m=options[i];
        out<<"{\"id\":"<<i<<",\"pattern\":"<<static_cast<unsigned>(m.pattern)<<",\"rank\":"<<m.rank<<",\"wildcards\":[";
        for(std::size_t j=0;j<m.wildcards.size();++j) {
            if(j) { out<<','; }
            const auto& w=m.wildcards[j];
            out<<"["<<w.cardId<<','<<w.rank<<','<<static_cast<unsigned>(w.suit)<<"]";
        }
        out<<"]}";
    }out<<']';
}
template<class T>void read(std::istringstream& input,T& value) {
    std::string token;
    if(!(input>>token)||token.empty()||token.front()=='-')throw std::invalid_argument("Missing or negative number");
    std::size_t used=0;
    unsigned long long number=0;
    try{number=std::stoull(token,&used);}catch(...){throw std::invalid_argument("Invalid number");}
    if(used!=token.size()||number>static_cast<unsigned long long>(std::numeric_limits<T>::max()))
        throw std::invalid_argument("Invalid number");
    value=static_cast<T>(number);
}
}
std::string state_message(const Scheduler& g,Players viewer) {
    std::ostringstream out;
    const char* phases[]={"not_started","shopping","round_starting","playing","round_ending","finished"};
    out<<"{\"type\":\"state\",\"seat\":"<<static_cast<unsigned>(viewer)
       <<",\"version\":"<<g.version()<<",\"phase\":"<<json_string(phases[static_cast<unsigned>(g.phase())])
       <<",\"round\":"<<g.round()<<",\"turn\":"<<g.turn()<<",\"acting\":"<<static_cast<unsigned>(g.acting_player())
       <<",\"deck\":"<<g.deck_remaining()<<",\"players\":[";
    for(unsigned i=0;i<4;++i) {
        if(i) { out<<','; }
        const auto p=static_cast<Players>(i);const auto& v=g.player(p);
        const bool privateShop=p!=viewer&&g.phase()==GamePhase::Shopping;
        out<<"{\"seat\":"<<i<<",\"name\":"<<json_string(v.name)<<",\"score\":";
        if(privateShop)out<<"null";else out<<v.score;
        out<<",\"coins\":";
        if(privateShop)out<<"null";else out<<v.coins;
        out
           <<",\"connected\":"<<(v.connected?"true":"false")<<",\"handCount\":"<<v.hand.size()<<",\"cards\":[";
        bool first=true;
        for(std::size_t slot=0;slot<MAX_FUNCTION_SLOTS;++slot)if(auto c=g.registry().at(p,slot)) {
            if(p!=viewer&&g.phase()==GamePhase::Shopping)continue;
            if(!first) { out<<','; }
            first=false;
            out<<"{\"slot\":"<<slot<<",\"seq\":"<<c->reference()->seq<<",\"definition\":"<<c->definition()
               <<",\"name\":"<<json_string(c->name())<<"}";
        }out<<"]}";
    }
    out<<"],\"hand\":[";
    const auto& hand=g.player(viewer).hand;
    for(std::size_t i=0;i<hand.size();++i) {
        if(i) { out<<','; }
        const auto& c=g.poker(hand[i]);
        out<<"{\"id\":"<<c.id<<",\"rank\":"<<c.rank.value_or(0)<<",\"suit\":"
           <<(c.suit?static_cast<int>(*c.suit):-1)<<",\"joker\":"<<(c.isJoker?"true":"false")<<"}";
    }
    out<<"],\"table\":[";
    for(std::size_t i=0;i<g.table_cards().size();++i){if(i)out<<',';out<<g.table_cards()[i];}
    out<<"],\"lastPlay\":";
    moves(out,g.table_move()?std::vector<MoveOption>{*g.table_move()}:std::vector<MoveOption>{});
    out<<",\"shop\":";
    if(g.phase()==GamePhase::Shopping) {
        const auto& s=g.shop(viewer);
        out<<"{\"id\":"<<s.id<<",\"done\":"<<(s.done?"true":"false")<<",\"refreshes\":"<<s.refreshes
           <<",\"facility\":"<<s.facility<<",\"offers\":[";
        for(std::size_t i=0;i<s.offers.size();++i) {
            if(i) { out<<','; }
            const auto& o=s.offers[i];
            out<<"{\"slot\":"<<i<<",\"product\":"<<o.id<<",\"definition\":"<<o.definition<<",\"price\":"<<o.price
               <<",\"name\":"<<json_string(o.name)<<",\"locked\":"<<((s.locked&&*s.locked==i)?"true":"false")<<"}";
        }out<<"]}";
    }else out<<"null";
    out<<",\"choice\":";
    if(g.choice()&&g.choice()->player==viewer) {
        out<<"{\"id\":"<<g.choice()->id<<",\"cards\":[";
        for(std::size_t i=0;i<g.choice()->cards.size();++i){if(i)out<<',';out<<g.choice()->cards[i];}
        out<<"],\"options\":";
        moves(out,g.choice()->options);out<<'}';
    }else out<<"null";
    out<<",\"preview\":";moves(out,g.preview(viewer));
    out<<",\"winners\":[";
    for(std::size_t i=0;i<g.winners().size();++i){if(i)out<<',';out<<static_cast<unsigned>(g.winners()[i]);}
    out<<"]}";return out.str();
}
EventPtr parse_request(Players actor,const std::string& command) {
    std::istringstream input(command);std::string verb;input>>verb;
    EventPtr result;
    const auto create=[&](EventType type)->Event&{
        result=ListenEvent::create_event(type,actor);return *result;};
    if(verb=="STATE")create(EventType::StateSyncRequest);
    else if(verb=="DONE"){auto& e=static_cast<FinishShoppingRequest&>(create(EventType::FinishShoppingRequest));read(input,e.shopId);}
    else if(verb=="PASS"){auto& e=static_cast<PassRequest&>(create(EventType::PassRequest));read(input,e.turnId);}
    else if(verb=="PLAY"){
        auto& e=static_cast<PlayCardsRequest&>(create(EventType::PlayCardsRequest));read(input,e.turnId);
        while(input>>std::ws&&!input.eof()){CardId id;read(input,id);e.cards.push_back(id);}
        if(e.cards.empty())throw std::invalid_argument("PLAY needs card IDs");
    }else if(verb=="OPTIONS"){
        auto& e=static_cast<PreviewPlayRequest&>(create(EventType::PreviewPlayRequest));
        while(input>>std::ws&&!input.eof()){CardId id;read(input,id);e.cards.push_back(id);}
        if(e.cards.empty())throw std::invalid_argument("OPTIONS needs card IDs");
    }else if(verb=="CHOOSE"){
        auto& e=static_cast<SubmitChoiceRequest&>(create(EventType::SubmitChoiceRequest));
        ChoiceOptionId option;read(input,e.choiceId);read(input,option);e.options={option};
    }else if(verb=="BUY"){
        auto& e=static_cast<BuyShopItemRequest&>(create(EventType::BuyShopItemRequest));
        read(input,e.shopId);read(input,e.slotId);read(input,e.productId);
    }else if(verb=="SELL"){
        auto& e=static_cast<SellFunctionCardRequest&>(create(EventType::SellFunctionCardRequest));
        e.card.owner=actor;read(input,e.shopId);read(input,e.card.seq);
    }else if(verb=="REFRESH"){
        auto& e=static_cast<RefreshShopRequest&>(create(EventType::RefreshShopRequest));read(input,e.shopId);
    }else if(verb=="LOCK"){
        auto& e=static_cast<LockShopSlotRequest&>(create(EventType::LockShopSlotRequest));read(input,e.shopId);read(input,e.slotId);
    }else if(verb=="UNLOCK"){
        auto& e=static_cast<UnlockShopSlotRequest&>(create(EventType::UnlockShopSlotRequest));read(input,e.shopId);read(input,e.slotId);
    }else if(verb=="UPGRADE"){
        auto& e=static_cast<UpgradeHandPatternRequest&>(create(EventType::UpgradeHandPatternRequest));
        unsigned pattern;read(input,e.shopId);read(input,pattern);
        if(pattern>=5)throw std::invalid_argument("Invalid pattern");
        e.pattern=static_cast<HandPattern>(pattern);
    }else if(verb=="FACILITY"){
        auto& e=static_cast<BuyFacilityRequest&>(create(EventType::BuyFacilityRequest));read(input,e.shopId);read(input,e.facilityId);
    }else if(verb=="ABILITY"){
        auto& e=static_cast<ActivateAbilityRequest&>(create(EventType::ActivateAbilityRequest));
        e.ability.card.owner=actor;read(input,e.ability.card.seq);read(input,e.ability.seq);
        while(input>>std::ws&&!input.eof()){
            unsigned target;read(input,target);if(target>=4)throw std::invalid_argument("Invalid target");
            e.selection.players.push_back(static_cast<Players>(target));
        }
    }else if(verb=="CONTINUE"){
        auto& e=static_cast<ContinueGameRequest&>(create(EventType::ContinueGameRequest));std::string mode;input>>mode;
        if(mode=="EXTRA3")e.mode=ContinueMode::ExtraThree;
        else if(mode=="ENDLESS")e.mode=ContinueMode::Endless;
        else throw std::invalid_argument("Unknown continuation mode");
    }else if(verb=="NEW")create(EventType::StartNewGameRequest);
    else throw std::invalid_argument("Unknown command");
    input>>std::ws;if(!input.eof())throw std::invalid_argument("Unexpected trailing fields");
    return result;
}
} // namespace cardgame::network

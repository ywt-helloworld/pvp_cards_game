#include "SystemCards.hpp"
#include <stdexcept>
namespace cardgame {
ResourceSystemCard::ResourceSystemCard(std::array<Player, 4>& players)
    : FunctionCards(900001, "System resources",
        listens_to({EventType::AddScore, EventType::AddCoins, EventType::SpendCoins}),
        SystemOrder::BeforePlayers), players_(players) {}
void ResourceSystemCard::resolve(const Context& c, CardEffects& out) {
    const auto lookup = [&](Players player) -> Player& {
        const auto i = static_cast<std::size_t>(player);
        if (i >= players_.size() || players_[i].id != player)
            throw std::invalid_argument("Unknown player");
        return players_[i];
    };
    if (const auto* e = c.event_as<AddScoreEvent>())
        out.emit(abilities::add_score(lookup(e->target), e->amount));
    else if (const auto* e = c.event_as<AddCoinsEvent>())
        out.emit(abilities::gain_coins(lookup(e->target), e->amount, "skill"));
    else if (const auto* e = c.event_as<SpendCoinsEvent>())
        out.emit(abilities::spend_coins(lookup(e->target), e->amount, e->reason));
}
InitialDealSystemCard::InitialDealSystemCard(PlayerOrder players, InitialDealOperation operation)
    : FunctionCards(900002, "System initial deal", listens_to({EventType::InitialDealBefore}),
        SystemOrder::AfterPlayers), players_(std::move(players)), operation_(std::move(operation)) {
    if (!operation_) throw std::invalid_argument("Missing deal operation");
    abilities::detail::validate_players(players_);
}
void InitialDealSystemCard::resolve(const Context& c, CardEffects& out) {
    const auto* e = c.event_as<BeforeInitialDealEvent>();
    if (e) out.emit(abilities::deal_initial_cards(e->roundId, players_,
                                                  e->cardsPerPlayer, operation_));
}
ResourceBonusCard::ResourceBonusCard(Player& owner, std::size_t sequence,
    DefinitionId definition, std::string name, EventType trigger, ScoreValue score, CoinValue coins,
    unsigned limit, std::optional<HandPattern> pattern, bool anyPlayer)
    : FunctionCards(CardRef{owner.id, sequence}, definition, std::move(name),
        listens_to({trigger,EventType::RoundStart})), limit_(limit), remaining_(limit),
      pattern_(pattern), anyPlayer_(anyPlayer), trigger_(trigger), owner_(owner), score_(score), coins_(coins) {
    if (score < 0 || coins < 0) throw std::invalid_argument("Negative bonus");
}
std::string ResourceBonusCard::description() const {
    std::string prefix;
    if (limit_) prefix = "每局最多 " + std::to_string(limit_) + " 次，";
    if (pattern_) {
        const char* patterns[] = {"单张","对子","普通顺子","炸弹","同花顺"};
        return prefix + (anyPlayer_ ? "任意玩家打出" : "自己打出") + patterns[static_cast<unsigned>(*pattern_)] +
            "后：+" + std::to_string(score_) + " 糖度，+" + std::to_string(coins_) + " 糖果";
    }
    return prefix + std::string(trigger_==EventType::PassCommitted?"自己不出后":"自己出牌后")+
        "：+"+std::to_string(score_)+" 糖度，+"+std::to_string(coins_)+" 糖果";
}
void ResourceBonusCard::validate_bonus_upgrade(ScoreValue score,CoinValue coins) const {
    if ((score && !score_) || (coins && !coins_)) throw std::invalid_argument("Unsupported numerical enhancement");
    if(score>std::numeric_limits<ScoreValue>::max()-score_ ||
       coins>std::numeric_limits<CoinValue>::max()-coins_) throw std::overflow_error("Bonus overflow");
}
void ResourceBonusCard::apply_bonus_upgrade(ScoreValue score,CoinValue coins) noexcept {
    score_+=score;coins_+=coins;
}
void ResourceBonusCard::resolve(const Context& c, CardEffects& out) {
    if (c.event_as<RoundStartedEvent>()) { remaining_ = limit_; return; }
    if (limit_ && !remaining_) return;
    const auto* play = c.event_as<CardsPlayedEvent>();
    if (pattern_ && (!play || play->pattern != *pattern_)) return;
    Players subject = c.actor().value_or(Players::P1);
    bool hasSubject = c.actor().has_value();
    if (const auto* e = c.event_as<CardsPlayedEvent>()) { subject = e->player; hasSubject = true; }
    if (const auto* e = c.event_as<PlayerPassedEvent>()) { subject = e->player; hasSubject = true; }
    if (!hasSubject || (!anyPlayer_ && subject != owner_.id)) return;
    if (limit_) --remaining_;
    out.emit(abilities::add_score(owner_, score_), 1);
    out.emit(abilities::gain_coins(owner_, coins_, "function card"), 2);
}
} // namespace cardgame

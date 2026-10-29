#include "Tournament.hpp"

#include "BasicStrategy.hpp"
#include "Constants.hpp"
#include "FontManager.hpp"

#include <algorithm>

using namespace BasicStrategy;

namespace
{
    // Recompute the book's action for a hand + dealer up-card - exactly what getRecommendedAction (the same advisor
    // Game::giveBasicStrategyAdvice wraps) prescribes, so a tournament seat plays identically to an in-game one.
    char recommendAction(const Hand& hand, const Card& dealerUpCard)
    {
        int value = hand.getValue();
        bool hasAce = hand.isSoft();
        bool isPair = hand.size() == 2 &&
                      hand.getCard(0).getBlackjackValue() == hand.getCard(1).getBlackjackValue();
        int pairValue = isPair ? hand.getCard(0).getBlackjackValue() : 0;
        return getRecommendedAction(value, dealerUpCard.getBlackjackValue(), hasAce, isPair, pairValue);
    }
}

Tournament::Tournament() = default;

void Tournament::start(const TournamentConfig& config, int deckCount)
{
    config_ = config;

    // Build one player per seat with equal starting chips. Names fall back to "Seat N" when a name is empty.
    seats_.clear();
    for (std::size_t i = 0; i < config_.seatNames.size(); ++i)
    {
        std::string name = config_.seatNames[i].empty() ? ("Seat " + std::to_string(i + 1)) : config_.seatNames[i];
        seats_.emplace_back(name, config_.startingChips);
    }
    // Normalise: guarantee at least a playable two-seat table even if the caller passed an empty name list.
    if (seats_.size() < 2)
        seats_.emplace_back("Seat " + std::to_string(seats_.size() + 1), config_.startingChips);

    for (auto& seat : seats_)
        seat.setBankroll(config_.startingChips); // authoritative starting bankroll regardless of ctor behaviour

    handPlayed_ = 0;
    winner_ = -1;
    complete_ = false;
    handTimer_ = 0.0f;

    if (!injectedDeck)
    {
        shoe.create(std::max(1, deckCount));
        shoe.shuffle();
    }
}

void Tournament::setDeck(const std::vector<Card>& cards)
{
    // Mirrors Game::setDeck: Deck::draw() pops from the back, so store reversed to honour natural DEAL order
    // (the first card in `cards` is the first one drawn). This keeps injected shoes intuitive for deterministic tests.
    shoe.setCards(std::vector<Card>(cards.rbegin(), cards.rend()));
    injectedDeck = true;
}

void Tournament::playSeatHand(Hand& hand, const Card& dealerUpCard)
{
    while (true)
    {
        if (hand.isBlackjack() || hand.isBust())
            break; // a natural or a bust needs no further decision

        char action = recommendAction(hand, dealerUpCard);

        if (action == 'S' || action == 'R')
            break; // stand (or surrender, treated as a stand in tournament play-out) -> stop here
        if (action == 'D' && hand.size() == 2)
        {
            hand.addCard(shoe.draw()); // tournament double: draw exactly one more card, then stand
            break;
        }
        // Hit (a pair's split is treated as an extra hit for a deterministic play-out): draw one more card.
        hand.addCard(shoe.draw());
    }
}

void Tournament::playHand()
{
    if (complete_)
        return;

    std::vector<Hand> hands(seats_.size());
    std::vector<bool> seated(seats_.size(), false);

    // Deal two cards to each seat, round-robin (all first cards, then all second cards), before the dealer's cards.
    for (int c = 0; c < 2; ++c)
        for (std::size_t s = 0; s < seats_.size(); ++s)
            hands[s].addCard(shoe.draw());

    // Force the blind bet where affordable; a seat that can't afford it sits out this round (RoundEngine's same rule).
    for (std::size_t s = 0; s < seats_.size(); ++s)
        seated[s] = seats_[s].placeBet(config_.blindBet);

    // Dealer: up-card first, then hole card.
    Hand dealer;
    dealer.addCard(shoe.draw());
    dealer.addCard(shoe.draw());

    // Play each seated seat on basic strategy against the revealed up-card.
    for (std::size_t s = 0; s < seats_.size(); ++s)
        if (seated[s])
            playSeatHand(hands[s], dealer.getCard(0));

    // Dealer draws to 17 and stands on all 17, reusing Hand value/bust logic.
    while (dealer.getValue() < 17 && !dealer.isBust())
        dealer.addCard(shoe.draw());

    const int dealerValue = dealer.getValue();
    const bool dealerBust = dealer.isBust();
    const bool dealerNatural = dealer.isBlackjack();

    // Settle every seated hand through the game's own settlement primitive. Player::settle applies the 3:2 natural
    // blackjack / even-money / push result via winBet/loseBet, so each seat's bankroll moves exactly as in-game and
    // reflects net profit/loss against the starting chip count (no stake was deducted up front).
    for (std::size_t s = 0; s < seats_.size(); ++s)
        if (seated[s])
        {
            hands[s].setBet(config_.blindBet);
            int payout = 0;
            seats_[s].settle(hands[s], dealerValue, dealerBust, dealerNatural, payout);
        }

    // Advance the counter and decide the winner: the seat with the strictly highest chips (lowest index breaks ties).
    ++handPlayed_;
    if (handPlayed_ >= config_.handsToPlay)
    {
        complete_ = true;
        winner_ = 0;
        for (std::size_t s = 1; s < seats_.size(); ++s)
            if (seats_[s].getBankroll() > seats_[winner_].getBankroll())
                winner_ = static_cast<int>(s);
    }
}

void Tournament::update(float deltaTime)
{
    // Paces automatic in-game progress at a readable rate. Pure-logic tests advance directly with playHand(), so this
    // timer never affects the deterministic path; it exists only to make the mode playable when driven by Game::update().
    if (complete_)
        return;
    handTimer_ += deltaTime;
    const float kHandInterval = 0.5f;
    if (handTimer_ >= kHandInterval)
    {
        handTimer_ = 0.0f;
        playHand();
    }
}

void Tournament::draw(sf::RenderWindow& window) const
{
    const float cx = window.getSize().x / 2.0f;

    // Backdrop: a distinctive slate so the tournament screen is visually distinct from felt + other modes.
    window.clear(sf::Color(25, 32, 48));

    auto text = [&](const std::string& label, int size, sf::Color color, float y)
    {
        sf::Text t(FontManager::get(), label, static_cast<unsigned int>(size));
        t.setFillColor(color);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, y));
        window.draw(t);
    };

    text("TOURNAMENT", 40, sf::Color(255, 215, 0), 90.f);
    text("Equal chips - forced blind bet", 16, sf::Color(180, 180, 180), 120.f);

    float y = 170.f;
    for (std::size_t s = 0; s < seats_.size(); ++s)
    {
        const bool isWinner = complete_ && static_cast<int>(s) == winner_;
        std::string row = seats_[s].getName() + ": " + std::to_string(seats_[s].getBankroll());
        text(row, 24, isWinner ? sf::Color(120, 255, 160) : sf::Color(230, 230, 230), y);
        y += 34.f;
    }

    std::string progress = "Hand " + std::to_string(currentHandForTest()) + " / " +
                           std::to_string(handsToPlayForTest());
    text(progress, 18, sf::Color(200, 200, 200), y + 8.f);

    if (complete_)
    {
        text("Winner: " + winnerNameForTest() + " (" + std::to_string(chipsForTest(static_cast<size_t>(winner_))) + " chips)",
             26, sf::Color(255, 215, 0), y + 46.f);
        text("Press Escape to return to the menu", 14, sf::Color(140, 140, 140), y + 78.f);
    }
    else
    {
        text("Highest chip count after all hands wins. Escape: back to menu", 14, sf::Color(150, 150, 150), y + 46.f);
    }
}

#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>

#include "Deck.hpp"
#include "Hand.hpp"
#include "Player.hpp"

// a fixed-hand, equal-chips blackjack tournament where every seat is forced to the same
// blind bet each round and the seat with the most chips when the hands run out wins. It reuses the game's own
// primitives - Deck for the shoe, Hand for dealing/value/soft/pair detection and Player::settle (which applies the
// standard 3:2 natural-blackjack / even-money / push settlement through winBet/loseBet) - with decisions taken from
// BasicStrategy::getRecommendedAction
// FORCED (no user choice of size)
// not from betting volume.
//
// Like StrategyTrainer this is its own AppMode state machine with pure, headless-safe logic and a graphics-only draw()
// - no SFML in the play-out path -
// card order via setDeck() (mirroring Game::setDeck) and drive one round at a time through playHand(); the on-screen
// presentation lives only in draw(). It deliberately does NOT touch PlayerProfile
class Tournament
{
  public:
    // Tournament parameters. Equal starting chips for every seat; each round forces the same blind bet; the run ends
    // after `handsToPlay` rounds and the highest final chip count wins. Defaults are a playable default table.
    struct TournamentConfig
    {
        int handsToPlay = 20;
        int startingChips = 1000;
        int blindBet = 10;
        std::vector<std::string> seatNames = {"Player", "Seat 2", "Seat 3", "Seat 4"};
    };

    Tournament();

    // Begin (or restart) a tournament from a fresh, shuffled shoe of `deckCount` decks. Seats are created with equal
    // starting chips and the names in config (falling back to Seat N when a name is empty). A positive deckCount uses
    // a real random deal; setDeck() overrides this for deterministic tests.
    void start(const TournamentConfig& config, int deckCount = 6);

    // Replace the shoe with an exact card order
    // the game's auto-shuffle path calls start(deckCount) instead). Must be called before the first playHand().
    void setDeck(const std::vector<Card>& cards);

    // Play exactly one tournament round: deal from the shoe, force the blind bet on every seat that can afford it,
    // let each seat act on basic strategy (hit until the book says otherwise; a double draws one more card then
    // stands), let the dealer finish to 17, and settle every hand via Player::settle. Advancing is explicit
    // can assert standings after any round without SFML or timing. Deal order within a round is seat-by-seat for two
    // cards each (round-robin: all first cards, then all second cards), then the dealer's up-card and hole card, so
    // an injected shoe maps to predictable seats. Ends the tournament when handsPlayed_ reaches config_.handsToPlay.
    void playHand();

    // Frame update for AppMode parity (mirrors Replay/StrategyTrainer wiring). Paces automatic in-game progress at a
    // readable rate; pure-logic tests drive advancement directly with playHand() and never rely on this timer
    // deterministic path stays timing-independent. No game-state mutation beyond playing one hand per tick interval.
    void update(float deltaTime);

    // --- Test accessors (mirror the *ForTest() convention) ---
    int currentHandForTest() const { return handPlayed_; }
    int handsToPlayForTest() const { return config_.handsToPlay; }
    bool isCompleteForTest() const { return complete_; }
    size_t seatCountForTest() const { return seats_.size(); }
    int chipsForTest(size_t i) const { return seats_[i].getBankroll(); }
    int winnerIndexForTest() const { return winner_; }
    std::string winnerNameForTest() const { return winner_ >= 0 && winner_ < static_cast<int>(seats_.size()) ? seats_[winner_].getName() : ""; }
    const TournamentConfig& configForTest() const { return config_; }

    // --- Presentation (graphics-only, no game-state mutation) ---
    void draw(sf::RenderWindow& window) const;

  private:
    Deck shoe;
    bool injectedDeck = false;

    TournamentConfig config_;
    int handPlayed_ = 0;
    int winner_ = -1;
    bool complete_ = false;
    float handTimer_ = 0.0f;

    // One player per seat, holding name + bankroll for the run (no disk profile I/O unless saveProfile is ever called).
    std::vector<Player> seats_;

    void playSeatHand(Hand& hand, const Card& dealerUpCard);
};
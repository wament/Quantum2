#include "Game.hpp"
#include "BasicStrategy.hpp"
#include "RoundEngine.hpp"

#include <sstream>
#include <stdexcept>
#include <filesystem>

#include "FontManager.hpp"
#include "Logger.hpp"
#include "Replay.hpp"

// Defined here (Replay.hpp is included in this translation unit) so unique_ptr<Replay>'s member
// destruction runs with a complete Replay type regardless of who destroys the Game.
Game::~Game() {}

Game::Game()
    : window(
          sf::VideoMode({Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT}), Constants::WINDOW_TITLE),
      gameView(
          sf::FloatRect(
              {0.f, 0.f},
              {static_cast<float>(Constants::WINDOW_WIDTH),
               static_cast<float>(Constants::WINDOW_HEIGHT)})),
      inputHandler(*this),
      evaluator(*this)
{
    window.setView(gameView);
    players.push_back(Player("William"));
    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;
    loadAssets();

    replay = std::make_unique<Replay>(window, cardTexture, FontManager::get());
    auto profile = players[0].getProfile();
    replay->loadHistory(profile.getLongHistory());
    replay->setStartingBankroll(players[0].getBankroll());
    Logger::info(R"(

=========================================================
                    ♠ BLACKJACK ♣
                     PLAYER CONTROLS
=========================================================

BETTING PHASE
-------------
  ↑  Up Arrow      Increase Bet
  ↓  Down Arrow    Decrease Bet
  <-  Right Arrow   Auto Kelly Bet 
  ->            Confirm Bet

PLAYER TURN
-----------
  H                Hit
  S                Stand
  D                Double Down
      P                Split
      Y                Surrender

ROUND / GAME
------------
  R                Next Round (after round ends)
  R                Restart Game (when Game Over)

Close the window at any time to quit.

=========================================================

)");
}

void Game::run()
{
    sf::Clock clock;

    while (window.isOpen())
    {
        deltaTime = clock.restart().asSeconds();
        inputHandler.processEvents();
        update(deltaTime);
        renderer.draw(*this);
    }
}

void Game::startNewGame()
{
    Logger::info("Starting new game");
    players.clear();

    // Human player (William) - always first
    players.push_back(Player("William"));

    for (int i = 0; i < GameConfig::NPC_COUNT && i + 1 < playerCount; i++)
    {
        const auto& cfg = GameConfig::NPC_CONFIGS[i];
        // Fresh in-memory NPC: no disk read, no JSON parse, no persistence. The human player above loads the real profile.
        players.push_back(Player(cfg.name, cfg.bankroll));
        players[i + 1].setBettingStrategy(static_cast<BettingStrategy>(cfg.strategy), cfg.baseBet);
        Logger::info() << players[i + 1].getName() << " starts with $" << cfg.bankroll
                       << " using " << cfg.strategy << " strategy";
    }

    auto profile = players[0].getProfile();

    deck.create(Constants::NUM_DECKS);
    deck.shuffle();
    currentShoeRecord.initialDeck = deck.getCards();
    currentShoeRecord.rounds.clear();

    // Reko is unbalanced and compensated by starting the count at -4 per deck; Hi-Lo needs no offset.
    runningCount = (countingStyle == BasicStrategy::CountingStyle::REKO) ? -4 * Constants::NUM_DECKS : 0;
    shoeCumulativeWinLoss = 0;
    sessionCumulativeWinLoss = 0;
    handActions.clear();
    handActions.resize(1);
    dealer.clear();
    players[0].resetRound(0);
    nextRound();

    appMode = AppMode::Playing;
}

void Game::setPlayerCount(int count)
{
    playerCount = std::max(1, std::min(6, count));
    Logger::debug() << "Player count set to " << playerCount;
}

void Game::loadAssets()
{
    Logger::debug("Current working directory: " + std::filesystem::current_path().string());

    if (!FontManager::load("assets/fonts/noto/NotoSans-Regular.ttf",
                           "assets/fonts/roboto/RobotoFlex-VariableFont.ttf"))
    {
        Logger::error("Failed to load font!");
        return;
    }

    hud.initialize(FontManager::get());

    if (!backgroundTexture.loadFromFile(
            "assets/images/felt.png"))
    {
        throw std::runtime_error(
            "Failed to load felt texture");
    }

    backgroundSprite = std::make_unique<sf::Sprite>(backgroundTexture);

    sf::Vector2u textureSize = backgroundTexture.getSize();
    sf::Vector2u windowSize = window.getSize();
    backgroundSprite->setScale(
        sf::Vector2f(
            static_cast<float>(windowSize.x) / textureSize.x,
            static_cast<float>(windowSize.y) / textureSize.y));

    if (!cardTexture.loadFromFile(
            "assets/images/cardsLarge_tilemap_packed.png"))
    {
        throw std::runtime_error(
            "Failed to load card texture");
    }
}

void Game::nextRound()
{
    Logger::info("New round");
    for (auto& p : players)
    {
        p.resetRound(initialBet);
    }
    dealer.clear();
    currentRoundRecord = {};
    handActions.clear();
    handActions.resize(players.size()); // one entry per player initially
    roundState = RoundState::PlaceBets;
    activePlayerIndex = 0;
}
// Events
void Game::update(float deltaTime)
{
    if (appMode == AppMode::MainMenu)
        return;

    if (appMode == AppMode::ReplayPlaying || appMode == AppMode::ReplayHistory)
    {
        replay->update(deltaTime);
        return;
    }
    if (appMode != AppMode::Playing)
        return;

    // Delegate the turn/round state machine to RoundEngine.
    RoundEngine engine;
    engine.tick(deltaTime, *this);
}

// Shared HUD timer countdown used by update().
void Game::tickTimer(float& timerField, float deltaTime)
{
    if (timerField > 0.0f)
    {
        timerField -= deltaTime;
        if (timerField < 0.0f)
            timerField = 0.0f;
    }
}

void Game::updateHUDState()
{
    hudState.bankroll = players[0].getBankroll();

    hudState.currentBet = players[0].hands[0].getBet();

    // Populate all player bankrolls
    hudState.playerBankrolls.clear();
    for (auto& p : players)
    {
        hudState.playerBankrolls.push_back(p.getBankroll());
    }

    hudState.runningCount = runningCount;
    hudState.countingStyleName = BasicStrategy::countingStyleName(countingStyle);

    // The True Count box always shows the HI_LO true count, even when Reko is active (surfaced in yellow as a
    // cross-check). Derive it from the HI_LO running count rather than the active system's count.
    const int hiLoRunningCount =
        (countingStyle == BasicStrategy::CountingStyle::HI_LO) ? runningCount : backgroundRunningCount;
    hudState.trueCount =
        BasicStrategy::computeTrueCount(hiLoRunningCount, static_cast<int>(deck.size()));
    hudState.trueCountHighlighted = (countingStyle == BasicStrategy::CountingStyle::REKO);

    hudState.cardsRemaining = deck.size();

    // Turn awareness so the book advisor can hide its action during the dealer's turn.
    hudState.isDealerTurn = (roundState == RoundState::DealerTurn);

    // Persistently advise basic strategy for the active hand while it is the player's turn.
    if (!hudState.isDealerTurn && roundState == RoundState::PlayerTurn &&
        !players.empty() &&
        activePlayerIndex < static_cast<int>(players.size()) &&
        !players[static_cast<size_t>(activePlayerIndex)].hands.empty())
    {
        hudState.basicStrategyHint = std::string(1, giveBasicStrategyAdvice(activeHand(), dealer));
    }
    else
    {
        hudState.basicStrategyHint.clear();
    }

    hudState.playerStats = &getStats();

    hudState.currentShoe = &currentShoeRecord;
}

// Test support: headless construction + deterministic deck injection.

Game::Game(bool headless)
    : window(),
      gameView(),
      inputHandler(*this),
      evaluator(*this)
{
    // Headless mode (unit tests): no SFML window, no asset loading, no replay, no HUD. Only the pure
    // round/state machine is initialized so a full game can be driven deterministically from tests.
    if (!headless)
        return;

    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;
    runningCount = 0;
    backgroundRunningCount = 0;
    trueCount = 0.f;
    gameState = GameState::Started;
    roundState = RoundState::PlayerTurn;
    activePlayerIndex = 0;
    initialBet = 0;
    shoeCumulativeWinLoss = 0;
    sessionCumulativeWinLoss = 0;
}

// Replace the shoe with an exact card order so tests can deal a fully determined sequence. `cards` is in
// natural DEAL order (first drawn first); Deck::draw() pops from the back, so we reverse before storing.
void Game::setDeck(const std::vector<Card>& cards)
{
    deck.setCards(std::vector<Card>(cards.rbegin(), cards.rend()));
}

const Hand& Game::testHand(size_t playerIndex, size_t handIndex) const
{
    return players[playerIndex].hands[handIndex];
}

// Set up a single in-memory player with a minimum bet and begin the round (deals the initial two cards
// plus the dealer up-card from the injected shoe). Leaves roundState at PlayerTurn when the dealer's
// up-card is not an ace, so split/hit/stand can be exercised end-to-end without SFML or disk profiles.
void Game::startTestRound()
{
    // In-memory single seat (NPC constructor: no disk read/write), so tests stay hermetic.
    players.clear();
    players.emplace_back("William", GameConfig::STARTING_BANKROLL);

    npcActionTimer = 0.0f;
    npcActionDelay = 1.0f;

    dealer.clear();
    runningCount = 0;
    backgroundRunningCount = 0;
    trueCount = 0.f;
    gameState = GameState::Started;
    roundState = RoundState::PlaceBets;
    activePlayerIndex = 0;
    initialBet = Constants::MINIMUM_BET;

    handActions.clear();
    handActions.resize(1);

    players[0].resetRound(Constants::MINIMUM_BET); // one hand, minimum bet (>0) so it gets dealt
    deal();                                        // initial two cards + dealer up card from the injected shoe
}

// Player actions

void Game::hit()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    if (hand.isFinished())
        return;

    // Split aces receive exactly one extra card and then stand — they may not be hit again.
    if (hand.isSplitAce() && !hand.splitAceNeedsMoreCards())
    {
        Logger::info() << p.getName() << " stands on " << hand.getValue() << " after splitting aces.";
        hand.setFinished(true);
        handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("S");
        hudState.basicStrategyHint.clear();
        return;
    }

    logStrategyDeviation('H');
    dealCardToPlayer(hand);

    Logger::info() << p.getName() << " hits. Has " << hand.getValue();

    if (hand.isBust())
    {
        Logger::info() << p.getName() << " busts!";
        hand.setFinished(true);
    }
    else if (hand.getValue() == 21)
    {
        Logger::info() << p.getName() << " has 21!";
        hand.setFinished(true);
    }

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("H");
    hudState.basicStrategyHint.clear();
}

void Game::stand()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();
    Logger::info() << p.getName() << " stands on " << hand.getValue() << ".";
    logStrategyDeviation('S');
    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("S");
    hudState.basicStrategyHint.clear();
}

bool Game::doubleDown()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();
    int initialBet = hand.getBet();

    if (hand.isFinished())
    {
        Logger::warn() << p.getName() << " tried to double down but hand is finished.";
        return false;
    }

    // A split ace receives exactly one card and then stands, so it may never be doubled. This guard makes
    // double-down consistent with the forced-stand behavior of hit()/stand() on split aces.
    if (hand.isSplitAce())
    {
        Logger::warn() << p.getName() << " tried to double down but cannot double a split ace.";
        return false;
    }

    if (!hand.canDouble())
    {
        Logger::warn() << p.getName() << " tried to double down but hand cannot double (not 2 cards).";
        return false;
    }

    logStrategyDeviation('D');
    hand.setDoubledDown(true);
    if (!p.placeBet(initialBet))
    {
        Logger::warn() << p.getName() << " tried to double down but cannot afford the bet.";
        return false;
    }

    hand.setBet(initialBet * 2);
    dealCardToPlayer(hand);

    PlayerStats& stats = getStats();
    stats.doubles++;
    stats.betsPlaced++;
    stats.betSum += initialBet;

    Logger::info() << p.getName() << " doubles " << initialBet << " to " << hand.getBet() << " on a " << hand.getValue() << ".";

    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("D");
    hudState.basicStrategyHint.clear();
    return true;
}

bool Game::split()
{
    auto& p = activePlayer();
    int handIndex = p.currentHand;

    if (p.hands[handIndex].isFinished())
    {
        Logger::warn() << p.getName() << " tried to split but hand is finished.";
        return false;
    }

    if (p.hands.size() >= Constants::MAX_SPLIT_HANDS)
    {
        Logger::warn() << p.getName() << " tried to split but has reached max split hands (" << Constants::MAX_SPLIT_HANDS << ").";
        return false;
    }

    if (!p.hands[handIndex].canSplit())
    {
        Logger::warn() << p.getName() << " tried to split but hand cannot split (not a pair or already hit).";
        return false;
    }

    logStrategyDeviation('P');
    Hand newHand;
    Card splitCard = p.hands[handIndex].takeLastCard();
    // Value of the card left in the first hand (the remaining ace for a pair of aces -> 11). Computed once
    // so both hands are flagged identically.
    const int splitValue = p.hands[handIndex].getCard(0).getBlackjackValue();
    Logger::info() << p.getName() << " splits " << splitCard.getBlackjackValueToString() << "s.";
    newHand.addCard(splitCard);
    newHand.setBet(p.hands[handIndex].getBet());
    // Flag BEFORE pushing: push_back copies newHand into the vector, so setting flags on the local copy
    // afterwards would leave the stored hand unflagged. That exact ordering bug meant the second split
    // hand never became a split-ace, so RoundEngine could not auto-finish it and the round stalled.
    newHand.setSplit(true, splitValue);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("P");
    hudState.basicStrategyHint.clear();

    // Each split hand is left with exactly one card here. RoundEngine then deals the single permitted
    // card to every split hand uniformly (auto-finish / re-split for aces; first card then act otherwise).
    // Pre-dealing into only this hand broke that symmetry: the first split-ace hand ended up size 2 but
    // unfinished, so it was left for the player to hit and even double down.
    p.hands.push_back(newHand);
    handActions.push_back({}); // new action vector for the split hand
    p.hands[handIndex].setSplit(true, splitValue);

    PlayerStats& stats = getStats();
    stats.splits++;
    stats.betsPlaced++;
    stats.betSum += p.hands[handIndex].getBet(); // new hand inherits bet amount
    return true;
}

bool Game::surrender()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    // Cannot surrender a finished hand
    if (hand.isFinished())
    {
        Logger::warn() << p.getName() << " tried to surrender but hand is finished.";
        return false;
    }

    // Only allowed on the first two cards
    if (!hand.canDouble())
    {
        Logger::warn() << p.getName() << " tried to surrender but hand cannot surrender (already hit or not 2 cards).";
        return false;
    }

    logStrategyDeviation('R');
    Logger::info() << p.getName() << " surrenders. Loses half their bet.";

    hand.setSurrendered(true);
    hand.setFinished(true);

    handActions[getGlobalHandIndexForPlayer(activePlayerIndex, p.currentHand)].push_back("R");
    hudState.basicStrategyHint.clear();
    return true;
}

void Game::logStrategyDeviation(char actionTaken)
{
    auto& p = players[activePlayerIndex];
    Hand& hand = p.hands[p.currentHand];
    int playerValue = hand.getValue();
    int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
    bool hasAce = hand.isSoft();
    bool isPair = hand.size() == 2 &&
                  hand.getCard(0).getBlackjackValue() ==
                      hand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? hand.getCard(0).getBlackjackValue() : 0;

    char correct = BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);

    PlayerStats& stats = getStats();
    stats.strategyTotal++;

    if (actionTaken == correct)
    {
        stats.strategyHits++;
    }
    else
    {
        stats.strategyDeviations++;
        Logger::info() << "Strategy deviation: took " << actionTaken << " but book says " << correct << ".";
        DeviationRecord record;
        record.playerValue = playerValue;
        record.dealerUpValue = dealerUpValue;
        record.wasSoft = hasAce;
        record.wasPair = isPair;
        record.pairValue = pairValue;
        record.actionTaken = actionTaken;
        record.correctAction = correct;
        record.bet = hand.getBet();
        record.won = false; // filled in later at evaluation time
        stats.recentDeviations.push_back(record);
        if (stats.recentDeviations.size() > 10)
            stats.recentDeviations.erase(stats.recentDeviations.begin());
    }
}

char Game::giveBasicStrategyAdvice(Hand& playerHand, Hand& dealerHand)
{
    int playerValue = playerHand.getValue();
    int dealerUpValue = dealerHand.getDealerUpCard().getBlackjackValue();
    bool hasAce = playerHand.isSoft();
    bool isPair = playerHand.size() == 2 &&
                  playerHand.getCard(0).getBlackjackValue() ==
                      playerHand.getCard(1).getBlackjackValue();
    int pairValue = isPair ? playerHand.getCard(0).getBlackjackValue() : 0;

    return BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);
}

size_t Game::getGlobalHandIndexForPlayer(size_t playerIdx, size_t handIdx)
{
    size_t globalIndex = 0;
    for (size_t i = 0; i < playerIdx; i++)
    {
        globalIndex += players[i].hands.size();
    }
    globalIndex += handIdx;
    return globalIndex;
}

int Game::calculateOptimalBet(float trueCount, float kellyFraction, int bankroll, int minBet, int maxBet) const
{
    return BasicStrategy::calculateKellyBet(trueCount, kellyFraction, bankroll, minBet, maxBet);
}

void Game::confirmBet()
{
    int currentBet = players[0].hands[0].getBet();
    if (currentBet < Constants::MINIMUM_BET || currentBet > Constants::MAXIMUM_BET)
        return;

    if (!players[0].placeBet(currentBet))
    {
        Logger::warn() << "Not enough bankroll. " << players[0].getBankroll() << " is your max. Or " << players[0].getBankroll() / 2 << " if you want to double.";
        return;
    }

    initialBet = currentBet;

    // Track bet stats
    PlayerStats& stats = getStats();
    stats.betsPlaced++;
    stats.betSum += currentBet;

    Logger::info() << players[0].getName() << " bets " << players[0].hands[players[0].currentHand].getBet() << ". Remaining bankroll: " << players[0].getBankroll() << ". Running count: " << runningCount << ".";

    // NPCs bet according to their strategy
    for (size_t i = 1; i < players.size(); i++)
    {
        // Skip NPCs who can't afford the minimum bet
        if (players[i].getBankroll() < Constants::MINIMUM_BET)
        {
            Logger::info() << players[i].getName() << " is out of chips and sits out this round.";
            players[i].hands[0].setBet(0);
            continue;
        }

        int npcBet = players[i].determineRoundBet(BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size())));

        // Apply table limits, then cap at bankroll
        npcBet = std::max(npcBet, Constants::MINIMUM_BET);
        npcBet = std::min(npcBet, Constants::MAXIMUM_BET);
        npcBet = std::min(npcBet, players[i].getBankroll());

        if (players[i].placeBet(npcBet))
        {
            players[i].hands[0].setBet(npcBet);
            Logger::info() << players[i].getName() << " bets " << npcBet << ".";
        }
        else
        {
            // Shouldn't happen after the checks above, but be safe
            Logger::warn() << players[i].getName() << " cannot place bet of " << npcBet << ".";
            players[i].hands[0].setBet(0);
        }
    }

    deal();
}

// Advance + wrap active player index at insurance-loop boundaries.
void Game::advanceToNextInsurancePlayer()
{
    ++activePlayerIndex;
    if (activePlayerIndex >= players.size())
    {
        activePlayerIndex = 0;
        evaluator.evaluateBlackjack();
    }
}

// Insurance
void Game::offerInsurance()
{
    auto& p = activePlayer();
    int bet = p.hands[p.currentHand].getBet();

    // Skip players who didn't bet
    if (bet == 0)
    {
        advanceToNextInsurancePlayer();
        return;
    }

    // Auto-decide for NPCs and auto-play human
    if (activePlayerIndex > 0 || gameSpeed == GameSpeed::AutoPlay)
    {
        float tc = BasicStrategy::computeTrueCount(runningCount, static_cast<int>(deck.size()));
        int maxIns = bet / 2;

        if (p.shouldTakeInsurance(bet, tc, countingStyle))
            p.takeInsurance(maxIns);
        else
            Logger::info() << p.getName() << " declines insurance (TC=" << tc << ")";

        advanceToNextInsurancePlayer();
    }
    // else: human player in manual mode - wait for input (handled in InputHandler)
}

void Game::confirmInsurance()
{
    auto& p = players[activePlayerIndex];
    int insBet = p.hands[p.currentHand].getInsuranceBet();
    if (insBet > 0)
    {
        p.placeBet(insBet);
        Logger::info() << p.getName() << " places insurance bet of " << insBet << ".";
    }

    advanceToNextInsurancePlayer();
}

void Game::declineInsurance()
{
    auto& p = players[activePlayerIndex];
    p.hands[p.currentHand].setInsuranceBet(0);
    Logger::info() << p.getName() << " declines insurance.";

    advanceToNextInsurancePlayer();
}

// Deck / Deal
void Game::deal()
{
    for (auto& p : players)
    {
        if (p.hands[0].getBet() > 0)
        {
            dealCardToPlayer(p.hands[0]);
        }
    }
    dealCardToDealer(); // dealer hole card
    for (auto& p : players)
    {
        if (p.hands[0].getBet() > 0)
        {
            dealCardToPlayer(p.hands[0]);
        }
    }
    dealCardToDealer(); // dealer up card

    roundState = RoundState::PlayerTurn;

    if (dealer.getDealerUpCard().isAce())
    {
        Logger::info("Dealer shows an Ace. Insurance offered.");
        roundState = RoundState::OfferInsurance;
    }
    else
        evaluator.evaluateBlackjack();

    Logger::info() << players[0].getName() << " has a " << players[0].hands[0].getValue() << " against dealer's " << dealer.getDealerUpCardToString() << ".";
}

void Game::npcTakeTurn()
{
    auto& p = activePlayer();
    Hand& hand = activeHand();

    if (hand.isFinished())
        return;

    Logger::debug(p.getName() + " is thinking...");
    char action = giveBasicStrategyAdvice(hand, dealer);

    // If strategy says split but we've reached max split hands,
    // fall back to hard hand strategy
    if (action == 'P' && p.hands.size() >= Constants::MAX_SPLIT_HANDS)
    {
        int playerValue = hand.getValue();
        int dealerUpValue = dealer.getDealerUpCard().getBlackjackValue();
        bool hasAce = hand.isSoft();
        action = BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, false, 0);
    }

    switch (action)
    {
    case 'H':
        hit();
        break;
    case 'S':
        stand();
        break;
    case 'D':
        if (!doubleDown())
            hit(); // Can't double, hit instead
        break;
    case 'P':
        if (!split())
            hit(); // Can't split, hit instead
        break;
    case 'R':
        if (!surrender())
            hit();
        break;
    default:
        stand();
        break;
    }
}
void Game::afterDraw(Card card)
{
    // Advance BOTH counting systems every draw so toggling between them is instant — neither count is ever
    // lost or recomputed when the user switches. `runningCount` stays mapped to the active style;
    // `backgroundRunningCount` tracks the other one. See toggleCountingStyle().
    runningCount = BasicStrategy::updateCount(runningCount, card.getBlackjackValue(), countingStyle);

    const BasicStrategy::CountingStyle other = (countingStyle == BasicStrategy::CountingStyle::REKO)
                                                   ? BasicStrategy::CountingStyle::HI_LO
                                                   : BasicStrategy::CountingStyle::REKO;
    backgroundRunningCount = BasicStrategy::updateCount(backgroundRunningCount, card.getBlackjackValue(), other);

    if (deck.reachedCutCard())
    {
        Logger::info("Cut card reached. Last hand of shoe.");
        gameState = GameState::LastHand;
    }
}

// Toggle which counting system is shown. Both systems are always counted in the background (see afterDraw);
// this swaps which buffer feeds the active count so switching between them is instant, and flips the active
// style that drives display, deviations and betting.
void Game::toggleCountingStyle()
{
    const int tmp = runningCount;
    runningCount = backgroundRunningCount;
    backgroundRunningCount = tmp;

    countingStyle = (countingStyle == BasicStrategy::CountingStyle::REKO)
                        ? BasicStrategy::CountingStyle::HI_LO
                        : BasicStrategy::CountingStyle::REKO;
}

// Collect all cards currently on the table (dealer + all player hands)
std::vector<Card> Game::collectTableCards() const
{
    std::vector<Card> tableCards;

    // Dealer's cards
    for (size_t i = 0; i < dealer.size(); i++)
    {
        tableCards.push_back(dealer.getCard(i));
    }

    // All player hands
    for (const auto& p : players)
    {
        for (const auto& hand : p.hands)
        {
            for (size_t i = 0; i < hand.size(); i++)
            {
                tableCards.push_back(hand.getCard(i));
            }
        }
    }

    return tableCards;
}
// Handle the rare case where the deck runs out before the cut card
void Game::handleEmptyDeck()
{
    Logger::warn() << "Deck empty before cut card - reshuffling remaining cards.";

    // Collect all cards currently on the table (these are dealt from the new shoe)
    std::vector<Card> tableCards = collectTableCards();

    // Save the current (incomplete) shoe record to the profile before reshuffling
    currentShoeRecord.completed = false;
    PlayerProfile profile = players[0].getProfile();
    profile.addShoeRecord(currentShoeRecord);
    players[0].updateProfile(profile);
    players[0].saveProfile();

    // Rebuild the shoe (full NUM_DECKS set minus what's been dealt) and continue dealing from it.
    deck.reshuffleWithRemaining(tableCards);

    // Start fresh shoe record, marked as continuation
    currentShoeRecord = {};
    currentShoeRecord.initialDeck = deck.getCards();
    currentShoeRecord.isContinuation = true;

    // Reset running count for the new shoe. Reko is unbalanced and compensated by starting at -4 per deck.
    runningCount = (countingStyle == BasicStrategy::CountingStyle::REKO) ? -4 * Constants::NUM_DECKS : 0;
    gameState = GameState::Started;

    Logger::info() << "New shoe created with " << deck.size() << " cards.";
}

void Game::dealCardToPlayer(Hand& hand)
{
    if (deck.empty())
        handleEmptyDeck();

    Card card = deck.draw();
    afterDraw(card);
    hand.addCard(card);
}

void Game::dealCardToDealer()
{
    if (deck.empty())
        handleEmptyDeck();

    Card card = deck.draw();
    afterDraw(card);
    dealer.addCard(card);
}

// Dealer
void Game::dealerTurn()
{
    bool anyUnresolved = false;
    for (auto& p : players)
    {
        for (const Hand& hand : p.hands)
        {
            if (hand.isBust())
                continue;
            if (hand.isBlackjack() && !hand.isSplit())
                continue;
            anyUnresolved = true;
        }
    }

    bool doNotDraw = !anyUnresolved;

    // Should the dealer draw another card right now? Hit below soft-17 / hard-17, but never when every
    // player hand is already resolved (a standing-only dealer draws no cards).
    bool shouldHit = ((dealer.getValue() < Constants::DEALER_STAND ||
                       (dealer.getValue() == Constants::DEALER_STAND && dealer.isSoft())) &&
                      !doNotDraw);

    if (!dealerTurnStarted)
    {
        // Turn just began: arm the anticipation beat before the first card.
        dealerTurnStarted = true;
        dealerStartDelayTimer = GameTiming::DEALER_START_DELAY;
        return;
    }
    if (shouldHit)
    {
        dealerStartDelayTimer -= deltaTime;
        if (dealerStartDelayTimer > 0.0f)
        {
            return; // wait before revealing this card
        }

        Logger::debug() << "Dealer has " << dealer.getValue();

        dealCardToDealer();

        // Arm the beat before the next card so cards come out one at a time, not all at once.
        dealerStartDelayTimer = GameTiming::DEALER_ACTION_DELAY;
        return;
    }

    // Dealer stands: no more cards will be drawn. Reveal is complete — settle the round now and reset the
    // pacing flags for the next round.
    dealerTurnStarted = false;
    dealerStartDelayTimer = 0.0f;

    if (dealer.isBust())
    {
        Logger::info() << "Dealer busted with " << dealer.getValue() << ". Too many!";
    }
    else
    {
        switch (dealer.getValue())
        {
        case 17:
            Logger::info() << "Dealer stands on the edge of " << dealer.getValue() << ".";
            break;
        case 18:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ".";
            break;
        case 19:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". It's bigger than average.";
            break;
        case 20:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". Dealer always has 20.";
            break;
        case 21:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ". That's the name of the game folks.";
            break;
        default:
            Logger::info() << "Dealer stands on " << dealer.getValue() << ".";
            break;
        }
    }

    evaluator.evaluateHands();

    if (gameState ==
        GameState::LastHand)
    {
        gameState =
            GameState::GameOver;

        // End of shoe stats
        getStats().shoesFinished++;
        shoeCumulativeWinLoss = 0; // reset for next shoe

        PlayerProfile profile = players[0].getProfile();

        profile.addShoeRecord(currentShoeRecord);
        players[0].updateProfile(profile);
        players[0].saveProfile();

        currentShoeRecord = {};
        Logger::info() << "Shoe completed. Final shoe profit: " << shoeCumulativeWinLoss << ".";

        if (gameSpeed == GameSpeed::AutoPlay)
        {
            Logger::info() << "Auto-play: Starting a new shoe.";
            hudState.afterShoeTimer = GameTiming::afterShoeWait(gameSpeed);
        }
        else
        {
            Logger::info() << "New shoe required. Press R.";
        }
    }
}
#include "Replay.hpp"

#include <algorithm>  // std::max for the reveal-clock reset detection in update() below
#include "Constants.hpp"  // DEAL_STAGGER_SECONDS: per-card reveal staggering for playback card-deal animation

// Card-deal animation (Feature 8-style, mirrored from GameRenderer): seed + return the reveal-start time for a card slot so
// arriving cards ease into their resting slot over playback instead of popping. On first sight a slot's reveal time is `now
// + ordinal*stagger` (earlier deal-ordinal -> earlier start), then frozen so its progress grows smoothly; settled frames reach
// the end exactly, matching the pre-animation layout. Presentation-only: seeding happens lazily during draw via ReplayView.
float Replay::dealerRevealAt(int i)
{
    auto it = dealerRevealAt_.find(i);
    if (it != dealerRevealAt_.end())
        return it->second;
    const float revealAt = replayDealClock_ + static_cast<float>(i) * Constants::DEAL_STAGGER_SECONDS;
    dealerRevealAt_[i] = revealAt;
    return revealAt;
}

float Replay::playerRevealAt(int handIndex, int cardIndex)
{
    const auto key = std::make_pair(handIndex, cardIndex);
    auto it = playerRevealAt_.find(key);
    if (it != playerRevealAt_.end())
        return it->second;
    // Stable deal-order ordinal: all earlier hands' cards come first, then this card's index, so earlier hands sweep in first.
    int ordinal = 0;
    for (int k = 0; k < handIndex; ++k)
        ordinal += static_cast<int>(currentState.playerHands[k].cards.size());
    ordinal += cardIndex;
    const float revealAt = replayDealClock_ + static_cast<float>(ordinal) * Constants::DEAL_STAGGER_SECONDS;
    playerRevealAt_[key] = revealAt;
    return revealAt;
}
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include "Graphics.hpp"
#include "FontManager.hpp"
#include "Hand.hpp"
#include "Card.hpp"
#include "ReplayView.hpp"
#include "ReplayLayout.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

Replay::Replay(sf::RenderWindow &window,
               sf::Texture &cardTexture,
               sf::Font &font)
    : window(window),
      cardTexture(cardTexture),
      font(font)
{
    mode = Mode::HistoryList;
}

void Replay::loadHistory(const std::vector<ShoeRecord> &shoes)
{
    allShoes = shoes; // store full records so we can load them later
    historyEntries.clear();
    selectedHistoryIndex = 0;
    mode = Mode::HistoryList;

    int runningShoeCount = 0;
    int cumulativeCardsDealt = 0;

    for (size_t i = 0; i < shoes.size(); ++i)
    {
        const auto &shoe = shoes[i];
        int net = 0;
        int cards = 0;

        for (const auto &round : shoe.rounds)
        {
            net += round.netProfit();
            cards += static_cast<int>(round.dealerCards.size());
            for (const auto &hand : round.playerHands)
                cards += static_cast<int>(hand.cards.size());
        }

        ReplayHistoryEntry entry;
        entry.index = static_cast<int>(i);
        entry.roundCount = static_cast<int>(shoe.rounds.size());
        entry.cardsDealt = cards;
        entry.netProfit = net;
        entry.completed = shoe.completed;
        entry.timestamp = (!shoe.rounds.empty())
                              ? shoe.rounds.back().timestamp
                              : 0;
        entry.startingCount = runningShoeCount;

        historyEntries.push_back(entry);

        // For the next shoe's startingCount we'd need to track
        // runningCount through the full shoe - currently we approximate
        // by using the last round's runningCount if available.
        if (!shoe.rounds.empty())
        {
            runningShoeCount = shoe.rounds.back().runningCount;
        }
        cumulativeCardsDealt += cards;
    }

    if (!historyEntries.empty())
    {
        selectedHistoryIndex = static_cast<int>(historyEntries.size()) - 1;
    }
}

void Replay::loadShoe(const ShoeRecord &shoe, int shoeIndex)
{
    if (shoe.rounds.empty())
        return;

    currentShoe = shoe;
    currentShoeIndex = shoeIndex;
    steps = buildSteps(shoe);
    currentStepIndex = -1; // before first step
    autoPlay = false;
    autoPlayTimer = 0.0f;

    roundInfos.clear();
    for (const auto &round : shoe.rounds)
    {
        RoundInfo info;
        info.netProfit = round.netProfit();
        info.maxTrueCount = round.trueCount;
        info.handCount = static_cast<int>(round.playerHands.size());
        roundInfos.push_back(info);
    }

    // Start before first step so the first stepForward() shows the first card dealt
    currentStepIndex = -1;
    currentState = ReplayInternals::ReplayState();
    currentState.totalSteps = static_cast<int>(steps.size());
    currentState.totalRounds = static_cast<int>(shoe.rounds.size());

    mode = Mode::Playing;
}

void Replay::exitReplay()
{
    mode = Mode::HistoryList;
    currentShoe = {};
    steps.clear();
    currentStepIndex = -1;
    autoPlay = false;
}

std::vector<ReplayInternals::ReplayStep>
Replay::buildSteps(const ShoeRecord &shoe)
{
    std::vector<ReplayInternals::ReplayStep> result;
    int shoeProfit = 0;
    int runningCount = 0; // track count incrementally
    int totalCards = static_cast<int>(shoe.initialDeck.size());
    int cardsDealtSoFar = 0;

    // Helper to add a step and update count after a card is dealt
    auto addCardStep = [&](ReplayInternals::ReplayStep &step)
    {
        step.runningCount = runningCount;
        step.cardsRemaining = totalCards - cardsDealtSoFar;
        step.trueCount = BasicStrategy::computeTrueCount(runningCount, step.cardsRemaining);
        step.cumulativeProfit = shoeProfit;
        result.push_back(step);
        runningCount += BasicStrategy::calculateHiLoValue(step.card.getBlackjackValue()); // update after card dealt
        cardsDealtSoFar++;
    };

    // Helper for non-card steps
    auto addNonCardStep = [&](ReplayInternals::ReplayStep &step)
    {
        step.runningCount = runningCount;
        step.cardsRemaining = totalCards - cardsDealtSoFar;
        step.trueCount = BasicStrategy::computeTrueCount(runningCount, step.cardsRemaining);
        step.cumulativeProfit = shoeProfit;
        result.push_back(step);
    };

    for (int r = 0; r < static_cast<int>(shoe.rounds.size()); ++r)
    {
        const auto &round = shoe.rounds[r];

        for (int h = 0; h < static_cast<int>(round.playerHands.size()); ++h)
        {
            const auto &handOutcome = round.playerHands[h];
            if (handOutcome.cards.size() >= 1)
            {
                ReplayInternals::ReplayStep s;
                s.roundIndex = r;
                s.type = ReplayInternals::ReplayStep::Type::DealCard;
                s.handIndex = h;
                s.card = handOutcome.cards[0];
                addCardStep(s);
            }
        }

        // Dealer hole card
        if (!round.dealerCards.empty())
        {
            ReplayInternals::ReplayStep holeStep;
            holeStep.roundIndex = r;
            holeStep.type = ReplayInternals::ReplayStep::Type::DealCard;
            holeStep.isDealerCard = true;
            holeStep.card = round.dealerCards[0];
            addCardStep(holeStep);
        }

        for (int h = 0; h < static_cast<int>(round.playerHands.size()); ++h)
        {
            const auto &handOutcome = round.playerHands[h];
            if (handOutcome.cards.size() >= 2)
            {
                ReplayInternals::ReplayStep s;
                s.roundIndex = r;
                s.type = ReplayInternals::ReplayStep::Type::DealCard;
                s.handIndex = h;
                s.card = handOutcome.cards[1];
                addCardStep(s);
            }
        }

        // Dealer up card
        if (round.dealerCards.size() >= 2)
        {
            ReplayInternals::ReplayStep s;
            s.roundIndex = r;
            s.type = ReplayInternals::ReplayStep::Type::DealCard;
            s.isDealerCard = true;
            s.card = round.dealerCards[1];
            addCardStep(s);
        }

        // --- Insurance check ---
        if (round.dealerCards.size() >= 1 && round.dealerCards[1].isAce())
        {
            ReplayInternals::ReplayStep offerStep;
            offerStep.roundIndex = r;
            offerStep.type = ReplayInternals::ReplayStep::Type::InsuranceOffer;
            addNonCardStep(offerStep);

            // Infer insurance decision from actions of first hand
            bool insuranceTaken = false;
            int insAmount = 0;
            if (!round.playerHands.empty())
            {
                insuranceTaken = round.playerHands[0].insuranceTaken;
                insAmount = round.playerHands[0].insuranceBet;
            }

            ReplayInternals::ReplayStep decStep;
            decStep.roundIndex = r;
            decStep.type = ReplayInternals::ReplayStep::Type::InsuranceDecision;
            decStep.insuranceTaken = insuranceTaken;
            decStep.insuranceAmount = insAmount;
            decStep.action = insuranceTaken ? "I" : "N";
            addNonCardStep(decStep);
        }

        // --- Player turns (hand by hand) ---
        for (size_t h = 0; h < round.playerHands.size(); ++h)
        {
            const auto &hand = round.playerHands[h];

            // Deal additional cards beyond initial two
            for (size_t c = 2; c < hand.cards.size(); ++c)
            {
                // Determine the action that caused this card
                std::string action = "H"; // default: hit

                int hitCount = 0;
                int cardIndex = 2;
                for (const auto &a : hand.actions)
                {
                    if (a == "H")
                    {
                        if (hitCount == static_cast<int>(c) - 2)
                        {
                            action = "H";
                            break;
                        }
                        hitCount++;
                    }
                }

                // Preceding decision step
                ReplayInternals::ReplayStep decStep;
                decStep.roundIndex = r;
                decStep.type = ReplayInternals::ReplayStep::Type::PlayerDecision;
                decStep.handIndex = static_cast<int>(h);
                decStep.action = action;
                addNonCardStep(decStep);

                // Card step
                ReplayInternals::ReplayStep cardStep;
                cardStep.roundIndex = r;
                cardStep.type = ReplayInternals::ReplayStep::Type::DealCard;
                cardStep.handIndex = static_cast<int>(h);
                cardStep.card = hand.cards[c];
                addCardStep(cardStep);
            }

            // Final action (Stand/Double/Split/Surrender)
            if (!hand.actions.empty())
            {
                const std::string &lastAction = hand.actions.back();

                ReplayInternals::ReplayStep decStep;
                decStep.roundIndex = r;
                decStep.type = ReplayInternals::ReplayStep::Type::PlayerDecision;
                decStep.handIndex = static_cast<int>(h);
                decStep.action = lastAction;
                addNonCardStep(decStep);
            }
        }

        // --- Dealer turn ---
        for (size_t c = 2; c < round.dealerCards.size(); ++c)
        {
            ReplayInternals::ReplayStep s;
            s.roundIndex = r;
            s.type = ReplayInternals::ReplayStep::Type::DealerDraw;
            s.isDealerCard = true;
            s.card = round.dealerCards[c];
            s.dealerHoleCardVisible = true;
            addCardStep(s);
        }

        // --- Round end ---
        shoeProfit += round.netProfit();
        ReplayInternals::ReplayStep endStep;
        endStep.roundIndex = r;
        endStep.type = ReplayInternals::ReplayStep::Type::RoundEnd;
        endStep.cumulativeProfit = shoeProfit;
        endStep.dealerHoleCardVisible = true;
        addNonCardStep(endStep);
    }

    return result;
}

ReplayInternals::ReplayState Replay::buildStateAtStep(int stepIndex) const
{
    ReplayInternals::ReplayState state;
    state.currentStepIndex = stepIndex;
    state.totalSteps = static_cast<int>(steps.size());
    state.totalRounds = static_cast<int>(currentShoe.rounds.size());

    if (steps.empty() || stepIndex < 0)
        return state;

    const auto &currentStep = steps[stepIndex];
    state.roundIndex = currentStep.roundIndex;
    state.runningCount = currentStep.runningCount;
    state.trueCount = currentStep.trueCount;
    state.cardsRemaining = currentStep.cardsRemaining;
    state.dealerHoleCardVisible = currentStep.dealerHoleCardVisible;
    state.isLastRound = (state.roundIndex == state.totalRounds - 1);
    state.shoeCompleted = currentShoe.completed;

    const auto &round = currentShoe.rounds[state.roundIndex];

    // --- Count how many dealer cards have been dealt up to this step ---
    int dealerCardsDealt = 0;
    for (int i = 0; i <= stepIndex; ++i)
    {
        const auto &s = steps[i];
        if (s.roundIndex == state.roundIndex && s.isDealerCard &&
            (s.type == ReplayInternals::ReplayStep::Type::DealCard ||
             s.type == ReplayInternals::ReplayStep::Type::DealerDraw))
        {
            dealerCardsDealt++;
        }
    }

    // Build dealer Hand from the slice of recorded dealer cards
    if (dealerCardsDealt > 0)
    {
        std::vector<Card> dealerSlice(
            round.dealerCards.begin(),
            round.dealerCards.begin() + std::min(dealerCardsDealt, static_cast<int>(round.dealerCards.size())));
        state.dealerHand = Hand(dealerSlice);
    }

    // --- Build each player Hand from HandOutcome cards ---
    for (size_t h = 0; h < round.playerHands.size(); ++h)
    {
        const auto &ho = round.playerHands[h];

        // Count how many cards have been dealt to this hand up to stepIndex
        int cardsDealtToHand = 0;
        for (int i = 0; i <= stepIndex; ++i)
        {
            const auto &s = steps[i];
            if (s.roundIndex == state.roundIndex &&
                s.handIndex == static_cast<int>(h) &&
                !s.isDealerCard &&
                s.type == ReplayInternals::ReplayStep::Type::DealCard)
            {
                cardsDealtToHand++;
            }
        }

        // Build HandSnapshot using HandOutcome data + Hand constructor
        ReplayInternals::ReplayState::HandSnapshot hs;
        hs.bet = ho.wager;
        hs.playerName = ho.playerName;
        hs.actionsTaken = ho.actions;
        hs.isFinished = true; // will be refined if mid-hand
        hs.isSplit = false;   // infer if needed
        hs.isDoubledDown = false;
        hs.isSurrendered = false;

        // Construct Hand from the appropriate slice of HandOutcome cards
        if (cardsDealtToHand > 0)
        {
            int count = std::min(cardsDealtToHand, static_cast<int>(ho.cards.size()));
            std::vector<Card> handSlice(ho.cards.begin(), ho.cards.begin() + count);
            hs.cards = Hand(handSlice);
        }

        // Metadata from HandOutcome
        if (!ho.actions.empty())
        {
            const std::string &lastAction = ho.actions.back();
            if (lastAction == "D")
                hs.isDoubledDown = true;
            if (lastAction == "R")
                hs.isSurrendered = true;
        }

        state.playerHands.push_back(hs);
    }

    // --- Current hand index ---
    state.currentHandIndex = currentStep.handIndex;
    if (state.currentHandIndex >= static_cast<int>(state.playerHands.size()))
        state.currentHandIndex = 0;

    // --- Profit tracking ---
    state.shoeProfit = currentStep.cumulativeProfit;
    state.roundProfit = round.netProfit();

    // --- Insurance ---
    if (round.dealerCards.size() >= 2 && round.dealerCards[1].isAce())
    {
        state.insuranceActive = true;
        // For now, insuranceBet is not tracked in records; leave at 0.
    }

    return state;
}

int Replay::findRoundStartStep(int roundIndex) const
{
    for (int i = 0; i < static_cast<int>(steps.size()); ++i)
    {
        if (steps[i].roundIndex == roundIndex)
            return i;
    }
    return 0;
}

int Replay::findHandStartStep(int roundIndex, int handIndex) const
{
    for (int i = 0; i < static_cast<int>(steps.size()); ++i)
    {
        const auto &s = steps[i];
        if (s.roundIndex == roundIndex && s.handIndex == handIndex &&
            s.type == ReplayInternals::ReplayStep::Type::DealCard &&
            !s.isDealerCard)
        {
            return i;
        }
    }
    return findRoundStartStep(roundIndex);
}

int Replay::findRoundEndStep(int roundIndex) const
{
    for (int i = static_cast<int>(steps.size()) - 1; i >= 0; --i)
    {
        if (steps[i].roundIndex == roundIndex &&
            steps[i].type == ReplayInternals::ReplayStep::Type::RoundEnd)
        {
            return i;
        }
    }
    return static_cast<int>(steps.size()) - 1;
}

int Replay::findNextDecisionStep(int fromStepIndex) const
{
    for (int i = fromStepIndex + 1; i < static_cast<int>(steps.size()); ++i)
    {
        if (steps[i].type == ReplayInternals::ReplayStep::Type::PlayerDecision)
            return i;
    }
    return static_cast<int>(steps.size()) - 1;
}

void Replay::stepForward()
{
    if (currentStepIndex < static_cast<int>(steps.size()) - 1)
    {
        ++currentStepIndex;
        currentState = buildStateAtStep(currentStepIndex);
        currentState.currentStepIndex = currentStepIndex;
    }
    else
    {
        autoPlay = false; // stop at end
    }
}

void Replay::stepBackward()
{
    if (currentStepIndex > 0)
    {
        --currentStepIndex;
        currentState = buildStateAtStep(currentStepIndex);
        currentState.currentStepIndex = currentStepIndex;
    }
}

void Replay::goToRound(int roundIndex)
{
    if (roundIndex < 0 || roundIndex >= static_cast<int>(currentShoe.rounds.size()))
        return;

    int targetStep = findRoundStartStep(roundIndex);
    currentStepIndex = targetStep;
    currentState = buildStateAtStep(currentStepIndex);
    currentState.currentStepIndex = currentStepIndex;
}

void Replay::skipHand()
{
    // Jump to the next hand's start, or to dealer turn if no more hands.
    const auto &state = getCurrentState();
    int nextHand = state.currentHandIndex + 1;

    if (nextHand < static_cast<int>(currentShoe.rounds[state.roundIndex].playerHands.size()))
    {
        currentStepIndex = findHandStartStep(state.roundIndex, nextHand);
    }
    else
    {
        // Skip to dealer turn: find first dealer draw after last player decision,
        // or the first step where all player hands are done.
        int searchFrom = currentStepIndex + 1;
        while (searchFrom < static_cast<int>(steps.size()) &&
               steps[searchFrom].type != ReplayInternals::ReplayStep::Type::DealerDraw)
        {
            ++searchFrom;
        }
        if (searchFrom < static_cast<int>(steps.size()))
        {
            currentStepIndex = searchFrom;
        }
    }

    currentState = buildStateAtStep(currentStepIndex);
    currentState.currentStepIndex = currentStepIndex;
}

void Replay::skipRound()
{
    int endStep = findRoundEndStep(currentState.roundIndex);
    if (endStep < static_cast<int>(steps.size()))
    {
        currentStepIndex = endStep;
        currentState = buildStateAtStep(currentStepIndex);
        currentState.currentStepIndex = currentStepIndex;
    }
}

void Replay::setAutoPlay(bool enable, float stepsPerSecond)
{
    autoPlay = enable;
    autoPlayInterval = (stepsPerSecond > 0.0f) ? (1.0f / stepsPerSecond) : 0.5f;
    autoPlayTimer = 0.0f;
}

void Replay::selectPreviousShoe()
{
    if (selectedHistoryIndex > 0)
    {
        --selectedHistoryIndex;
        // Keep the newly selected row within view while scrolling with the arrow keys.
        adjustHistorySelectionVisibility();
    }
}

void Replay::selectNextShoe()
{
    if (selectedHistoryIndex < static_cast<int>(historyEntries.size()) - 1)
    {
        ++selectedHistoryIndex;
        // Keep the newly selected row within view while scrolling with the arrow keys.
        adjustHistorySelectionVisibility();
    }
}

// Index of the list row under `mousePos` (window pixels), or -1 if none. Row geometry is shared
// with ReplayView::drawHistoryList via ReplayLayout so a drawn row and its hover target never drift.
int Replay::hoveredHistoryRow(sf::Vector2i mousePos) const
{
    if (historyEntries.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(historyEntries.size()); ++i)
    {
        // Scroll offset shifts rows up so off-screen entries become hoverable and clickable.
        float rowTop = ReplayLayout::historyRowTop(i, historyScrollOffset);

        // Match the draw clip exactly: only rows actually drawn below the header are selectable,
        // so a row that has scrolled into the title is not hoverable either (helper returns -1).
        if (rowTop < 0.f) continue;

        bool inX = ReplayLayout::inSelectableXBand(static_cast<float>(mousePos.x));
        bool inY = (mousePos.y >= static_cast<int>(rowTop - 5.f)) &&
                   (mousePos.y <= static_cast<int>(rowTop + ReplayLayout::kRowHeight));
        if (inX && inY)
            return i;
    }

    return -1;
}

int Replay::historyVisibleRowCount() const
{
    int windowHeight = window.getSize().y;
    return ReplayLayout::visibleHistoryRows(windowHeight);
}

int Replay::maxHistoryScrollOffset() const
{
    int maxOffset = static_cast<int>(historyEntries.size()) - historyVisibleRowCount();
    return maxOffset > 0 ? maxOffset : 0;
}

void Replay::adjustHistorySelectionVisibility()
{
    int visible = historyVisibleRowCount();
    if (visible < 1)
        return;

    // Bring the selected row back into view when it scrolls above or below the visible window.
    if (selectedHistoryIndex < historyScrollOffset)
        historyScrollOffset = selectedHistoryIndex;                       // scrolled up past selection
    else if (selectedHistoryIndex >= historyScrollOffset + visible)
        historyScrollOffset = selectedHistoryIndex - visible + 1;         // scrolled down past selection

    int maxOffset = maxHistoryScrollOffset();
    if (historyScrollOffset < 0)
        historyScrollOffset = 0;
    if (historyScrollOffset > maxOffset)
        historyScrollOffset = maxOffset;
}

void Replay::setStartingBankroll(int bankroll) { startingBankroll = bankroll; currentBankroll = bankroll; }

Replay::Mode Replay::getMode() const { return mode; }

const ReplayInternals::ReplayState &Replay::getCurrentState() const { return currentState; }

const std::vector<ReplayHistoryEntry> &Replay::getHistoryEntries() const { return historyEntries; }

int Replay::getSelectedHistoryIndex() const { return selectedHistoryIndex; }

bool Replay::isAtStart() const { return currentStepIndex <= 0; }

bool Replay::isAtEnd() const { return currentStepIndex >= static_cast<int>(steps.size()) - 1; }

void Replay::processEvents(sf::Event &event)
{
    if (event.is<sf::Event::Closed>())
    {
        // Let Game handle shutdown; just exit replay mode.
        exitReplay();
        return;
    }

    // Track cursor position for history-list hover highlighting and click selection.
    if (auto mouseMove = event.getIf<sf::Event::MouseMoved>())
    {
        replayCursorPos = sf::Vector2i(mouseMove->position.x, mouseMove->position.y);
    }

    // Left-click a row to load it, matching the Enter/Right keyboard path above so the history
    // list selects exactly like every other menu: hover to highlight, left-click to pick.
    if (auto clicked = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (mode == Mode::HistoryList &&
            clicked->button == sf::Mouse::Button::Left)
        {
            int hoveredIndex = hoveredHistoryRow(replayCursorPos);
            if (hoveredIndex >= 0 && !allShoes.empty())
            {
                selectedHistoryIndex = hoveredIndex;
                loadShoe(allShoes[selectedHistoryIndex], selectedHistoryIndex);
            }
        }
    }

    // Mouse wheel scrolls the shoe list: up reveals earlier entries, down reveals later ones. This is
    // one path shared with keyboard Up/Down and the hover/click selection above -- off-screen rows are
    // selectable once scrolled into view. A positive delta scrolls up (fewer rows past the top).
    if (auto wheel = event.getIf<sf::Event::MouseWheelScrolled>())
    {
        if (mode == Mode::HistoryList && !historyEntries.empty())
        {
            int offsetDelta = wheel->delta > 0.f ? -1 : +1;
            historyScrollOffset += offsetDelta;

            int maxOffset = maxHistoryScrollOffset();
            if (historyScrollOffset < 0)
                historyScrollOffset = 0;
            if (historyScrollOffset > maxOffset)
                historyScrollOffset = maxOffset;
        }
    }

    // Handle KeyPressed events
    if (event.is<sf::Event::KeyPressed>())
    {
        const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
        if (keyEvent)
        {
            if (mode == Mode::HistoryList)
            {
                switch (keyEvent->code)
                {
                case sf::Keyboard::Key::Up:
                    selectPreviousShoe();
                    break;
                case sf::Keyboard::Key::Down:
                    selectNextShoe();
                    break;
                case sf::Keyboard::Key::Enter:
                case sf::Keyboard::Key::Right:
                    if (!historyEntries.empty())
                    {
                        const auto &entry = historyEntries[selectedHistoryIndex];
                        loadShoe(allShoes[entry.index], entry.index);
                    }
                    break;
                case sf::Keyboard::Key::Escape:
                    exitReplay();
                    break;
                default:
                    break;
                }
            }
            else // Playing mode
            {
                switch (keyEvent->code)
                {
                case sf::Keyboard::Key::Right:
                    stepForward();
                    break;
                case sf::Keyboard::Key::Left:
                    stepBackward();
                    break;
                case sf::Keyboard::Key::Space:
                    setAutoPlay(!autoPlay);
                    break;
                case sf::Keyboard::Key::H:
                    skipHand();
                    break;
                case sf::Keyboard::Key::R:
                    skipRound();
                    break;
                case sf::Keyboard::Key::Escape:
                case sf::Keyboard::Key::Backspace:
                    exitReplay();
                    break;
                case sf::Keyboard::Key::Num1:
                    if (currentState.totalRounds > 0)
                        goToRound(std::min(1, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num2:
                    if (currentState.totalRounds > 1)
                        goToRound(std::min(2, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num3:
                    if (currentState.totalRounds > 2)
                        goToRound(std::min(3, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num4:
                    if (currentState.totalRounds > 3)
                        goToRound(std::min(4, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num5:
                    if (currentState.totalRounds > 4)
                        goToRound(std::min(5, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num6:
                    if (currentState.totalRounds > 5)
                        goToRound(std::min(6, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num7:
                    if (currentState.totalRounds > 6)
                        goToRound(std::min(7, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num8:
                    if (currentState.totalRounds > 7)
                        goToRound(std::min(8, currentState.totalRounds - 1));
                    break;
                case sf::Keyboard::Key::Num9:
                    if (currentState.totalRounds > 8)
                        goToRound(std::min(9, currentState.totalRounds - 1));
                    break;
                default:
                    break;
                }
            }
        }
        return;
    }

    // Handle MouseButtonPressed events
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        const auto* mouseEvent = event.getIf<sf::Event::MouseButtonPressed>();
        if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left)
        {
            sf::Vector2f mousePos(mouseEvent->position.x, mouseEvent->position.y);
            if (sliderDragging || sliderBounds.contains(mousePos))
            {
                sliderDragging = true;
            }
        }
        return;
    }

    // Handle MouseButtonReleased events
    if (event.is<sf::Event::MouseButtonReleased>())
    {
        const auto* mouseEvent = event.getIf<sf::Event::MouseButtonReleased>();
        if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left)
        {
            sliderDragging = false;
        }
        return;
    }

    // Handle MouseMoved events
    if (event.is<sf::Event::MouseMoved>() && sliderDragging)
    {
        const auto* mouseEvent = event.getIf<sf::Event::MouseMoved>();
        if (mouseEvent)
        {
            sf::Vector2f mousePos(mouseEvent->position.x, mouseEvent->position.y);
            float frac = (mousePos.x - sliderBounds.position.x) / sliderBounds.size.x;
            frac = std::max(0.0f, std::min(1.0f, frac));

            int targetRound = static_cast<int>(frac * (currentState.totalRounds - 1));
            goToRound(targetRound);
        }
    }
}

void Replay::update(float deltaTime)
{
    if (mode != Mode::Playing)
        return;

    if (autoPlay)
    {
        autoPlayTimer += deltaTime;
        if (autoPlayTimer >= autoPlayInterval)
        {
            autoPlayTimer -= autoPlayInterval;
            stepForward();
        }
    }

    // Update slider bounds each frame so it stays correct if window resizes
    sf::Vector2u size = window.getSize();
    sliderBounds.position.x = 60.0f;
    sliderBounds.position.y = static_cast<float>(size.y) - 150.0f;
    sliderBounds.size.x = static_cast<float>(size.x - 120);
    sliderBounds.size.y = 16.0f;

    // Feature 8-style card-deal animation for replay playback: advance a shared reveal clock each frame so arriving cards
    // slide/flip into their resting slot over time instead of popping. Mirror GameRenderer's model exactly -- one clock, per-slot
    // reveal times seeded on first sight (see dealerRevealAt()/playerRevealAt()), reset only when the shown-card count drops
    // (stepping back) so re-appearing cards sweep in rather than pop. Presentation-only: the step/tick logic above is untouched.
    int total = static_cast<int>(currentState.dealerHand.size());
    for (const auto& ph : currentState.playerHands)
        total += static_cast<int>(ph.cards.size());
    if (prevTotalCards_ >= 0 && total < prevTotalCards_)
    {
        dealerRevealAt_.clear();
        playerRevealAt_.clear();
    }
    prevTotalCards_ = std::max(prevTotalCards_, total);
    replayDealClock_ += deltaTime;
}

void Replay::draw()
{
    window.clear(sf::Color(20, 20, 30));

    // The view layer owns all rendering; build it from this instance's resources and delegate.
    ReplayView view(window, cardTexture, font, *this);
    if (mode == Mode::HistoryList)
    {
        view.drawHistoryList();
    }
    else
    {
        view.drawReplayView();
    }

    window.display();
}

char Replay::getBasicStrategyAction(const ReplayInternals::ReplayState &state) const
{
    if (state.playerHands.empty() || state.dealerHand.size() < 2)
        return 'H';

    const auto &hand = state.playerHands[state.currentHandIndex];

    int playerValue = hand.getValue();
    int dealerUpValue = state.dealerHand.getCard(0).getBlackjackValue();

    bool hasAce = hand.cards.isSoft();

    bool isPair = hand.cards.size() == 2 &&
                  hand.cards.getCard(0).getBlackjackValue() ==
                      hand.cards.getCard(1).getBlackjackValue();

    int pairValue = isPair ? hand.cards.getCard(0).getBlackjackValue() : 0;

    return BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);
}

// src/Replay.cpp

#include "Replay.hpp"

#include "Constants.hpp"
#include "Graphics.hpp"
#include "FontManager.hpp"
#include "Hand.hpp"
#include "Card.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

// --------------------------------------------------
// Construction / Initialization
// --------------------------------------------------

Replay::Replay(sf::RenderWindow& window,
               sf::Texture& cardTexture,
               sf::Font& font)
    : window(window),
      cardTexture(cardTexture),
      font(font)
{
    mode = Mode::HistoryList;
}

void Replay::loadHistory(const std::vector<ShoeRecord>& shoes)
{
    allShoes = shoes;  // store full records so we can load them later
    historyEntries.clear();
    selectedHistoryIndex = 0;
    mode = Mode::HistoryList;

    int runningShoeCount = 0;
    int cumulativeCardsDealt = 0;

    for (size_t i = 0; i < shoes.size(); ++i)
    {
        const auto& shoe = shoes[i];
        int net = 0;
        int cards = 0;

        for (const auto& round : shoe.rounds)
        {
            net += round.netProfit();
            cards += static_cast<int>(round.dealerCards.size());
            for (const auto& hand : round.playerHands)
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
        // runningCount through the full shoe — currently we approximate
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

void Replay::loadShoe(const ShoeRecord& shoe, int shoeIndex)
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
    for (const auto& round : shoe.rounds)
    {
        RoundInfo info;
        info.netProfit = round.netProfit();
        info.maxTrueCount = round.trueCount;
        info.handCount = static_cast<int>(round.playerHands.size());
        roundInfos.push_back(info);
    }

    // Start before first step so the first stepForward() shows the first card dealt
    currentStepIndex = -1;
    currentState = ReplayState();
    // currentStepIndex already set to -1 above
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

// --------------------------------------------------
// Step Building
// --------------------------------------------------

std::vector<ReplayInternals::ReplayStep>
Replay::buildSteps(const ShoeRecord& shoe)
{
    std::vector<ReplayInternals::ReplayStep> result;
    int shoeProfit = 0;
    int runningCount = 0;        // track count incrementally
    int totalCards = static_cast<int>(shoe.initialDeck.size());
    int cardsDealtSoFar = 0;

    // Helper to add a step and update count after a card is dealt
    auto addCardStep = [&](ReplayInternals::ReplayStep& step) {
        step.runningCount = runningCount;
        step.cardsRemaining = totalCards - cardsDealtSoFar;
        step.trueCount = computeTrueCount(runningCount, step.cardsRemaining);
        step.cumulativeProfit = shoeProfit;
        result.push_back(step);
        runningCount += step.card.getHiLoValue();  // update after card dealt
        cardsDealtSoFar++;
    };

    // Helper for non-card steps
    auto addNonCardStep = [&](ReplayInternals::ReplayStep& step) {
        step.runningCount = runningCount;
        step.cardsRemaining = totalCards - cardsDealtSoFar;
        step.trueCount = computeTrueCount(runningCount, step.cardsRemaining);
        step.cumulativeProfit = shoeProfit;
        result.push_back(step);
    };

    for (int r = 0; r < static_cast<int>(shoe.rounds.size()); ++r)
    {
        const auto& round = shoe.rounds[r];

        // --- Initial deal: player hand 0 card 0, dealer hole, player hand 0 card 1, dealer up ---
        // We replay the exact deal order from Game::deal().
        // Player hand 0 card 0
        if (!round.playerHands.empty())
        {
            ReplayInternals::ReplayStep s;
            s.roundIndex = r;
            s.type = ReplayInternals::ReplayStep::Type::DealCard;
            s.handIndex = 0;
            s.card = round.playerHands[0].cards[0];
            addCardStep(s);
        }

        // Dealer hole card
        {
            ReplayInternals::ReplayStep holeStep;
            holeStep.roundIndex = r;
            holeStep.type = ReplayInternals::ReplayStep::Type::DealCard;
            holeStep.isDealerCard = true;
            holeStep.card = round.dealerCards[0];
            addCardStep(holeStep);
        }

        // Player hand 0 card 1
        if (!round.playerHands.empty() && round.playerHands[0].cards.size() >= 2)
        {
            ReplayInternals::ReplayStep s;
            s.roundIndex = r;
            s.type = ReplayInternals::ReplayStep::Type::DealCard;
            s.handIndex = 0;
            s.card = round.playerHands[0].cards[1];
            addCardStep(s);
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
                const auto& actions = round.playerHands[0].actions;
                // Insurance is tracked as first action "I" or "N" if we ever record it.
                // For now, we'll skip explicit insurance step — can be enhanced later.
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
            const auto& hand = round.playerHands[h];

            // Deal additional cards beyond initial two
            for (size_t c = 2; c < hand.cards.size(); ++c)
            {
                // Determine the action that caused this card
                std::string action = "H"; // default: hit

                // Find matching action from actions list
                int hitCount = 0;
                int cardIndex = 2;
                for (const auto& a : hand.actions)
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
                const std::string& lastAction = hand.actions.back();

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

// --------------------------------------------------
// State Reconstruction
// --------------------------------------------------

ReplayInternals::ReplayState Replay::buildStateAtStep(int stepIndex) const
{
    ReplayInternals::ReplayState state;
    state.currentStepIndex = stepIndex;
    state.totalSteps = static_cast<int>(steps.size());
    state.totalRounds = static_cast<int>(currentShoe.rounds.size());

    if (steps.empty() || stepIndex < 0)
        return state;

    const auto& currentStep = steps[stepIndex];
    state.roundIndex = currentStep.roundIndex;
    state.runningCount = currentStep.runningCount;
    state.trueCount = currentStep.trueCount;
    state.cardsRemaining = currentStep.cardsRemaining;
    state.dealerHoleCardVisible = currentStep.dealerHoleCardVisible;
    state.isLastRound = (state.roundIndex == state.totalRounds - 1);
    state.shoeCompleted = currentShoe.completed;

    const auto& round = currentShoe.rounds[state.roundIndex];

    // --- Count how many dealer cards have been dealt up to this step ---
    int dealerCardsDealt = 0;
    for (int i = 0; i <= stepIndex; ++i)
    {
        const auto& s = steps[i];
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
            round.dealerCards.begin() + std::min(dealerCardsDealt, static_cast<int>(round.dealerCards.size()))
        );
        state.dealerHand = Hand(dealerSlice);
    }

    // --- Build each player Hand from HandOutcome cards ---
    for (size_t h = 0; h < round.playerHands.size(); ++h)
    {
        const auto& ho = round.playerHands[h];

        // Count how many cards have been dealt to this hand up to stepIndex
        int cardsDealtToHand = 0;
        for (int i = 0; i <= stepIndex; ++i)
        {
            const auto& s = steps[i];
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
        hs.actionsTaken = ho.actions;
        hs.isFinished = true;  // will be refined if mid-hand
        hs.isSplit = false;    // infer if needed
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
            const std::string& lastAction = ho.actions.back();
            if (lastAction == "D") hs.isDoubledDown = true;
            if (lastAction == "R") hs.isSurrendered = true;
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

// --------------------------------------------------
// Navigation Helpers
// --------------------------------------------------

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
        const auto& s = steps[i];
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

// --------------------------------------------------
// Navigation Commands
// --------------------------------------------------

// src/Replay.cpp (continued from stepForward())

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
    const auto& state = getCurrentState();
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

// --------------------------------------------------
// History List Navigation
// --------------------------------------------------

void Replay::selectPreviousShoe()
{
    if (selectedHistoryIndex > 0)
    {
        --selectedHistoryIndex;
    }
}

void Replay::selectNextShoe()
{
    if (selectedHistoryIndex < static_cast<int>(historyEntries.size()) - 1)
    {
        ++selectedHistoryIndex;
    }
}

void Replay::setStartingBankroll(int bankroll)
{
    startingBankroll = bankroll;
    currentBankroll = bankroll;
}

// --------------------------------------------------
// Queries
// --------------------------------------------------

Replay::Mode Replay::getMode() const
{
    return mode;
}

const ReplayInternals::ReplayState& Replay::getCurrentState() const
{
    return currentState;
}

const std::vector<ReplayHistoryEntry>& Replay::getHistoryEntries() const
{
    return historyEntries;
}

int Replay::getSelectedHistoryIndex() const
{
    return selectedHistoryIndex;
}

bool Replay::isAtStart() const
{
    return currentStepIndex <= 0;
}

bool Replay::isAtEnd() const
{
    return currentStepIndex >= static_cast<int>(steps.size()) - 1;
}

// --------------------------------------------------
// Event Processing
// --------------------------------------------------

void Replay::processEvents(sf::Event& event)
{
    if (event.type != sf::Event::KeyPressed &&
        event.type != sf::Event::MouseButtonPressed &&
        event.type != sf::Event::MouseButtonReleased &&
        event.type != sf::Event::MouseMoved)
    {
        return;
    }

    if (event.type == sf::Event::Closed)
    {
        // Let Game handle shutdown; just exit replay mode.
        exitReplay();
        return;
    }

    if (mode == Mode::HistoryList)
    {
        if (event.type == sf::Event::KeyPressed)
        {
            switch (event.key.code)
            {
                case sf::Keyboard::Up:
                    selectPreviousShoe();
                    break;
                case sf::Keyboard::Down:
                    selectNextShoe();
                    break;
                case sf::Keyboard::Enter:
                case sf::Keyboard::Return:
                case sf::Keyboard::Right:
                    if (!historyEntries.empty())
                    {
                        const auto& entry = historyEntries[selectedHistoryIndex];
                        loadShoe(allShoes[entry.index], entry.index);  // load from stored records
                    }
                    break;
                case sf::Keyboard::Escape:
                    exitReplay();
                    break;
                default:
                    break;
            }
        }
        return;
    }

    // --- Playing mode ---

    if (event.type == sf::Event::MouseButtonPressed &&
        event.mouseButton.button == sf::Mouse::Left)
    {
        sf::Vector2f mousePos(
            static_cast<float>(event.mouseButton.x),
            static_cast<float>(event.mouseButton.y)
        );
        if (sliderDragging || sliderBounds.contains(mousePos))
        {
            sliderDragging = true;
        }
    }

    if (event.type == sf::Event::MouseButtonReleased &&
        event.mouseButton.button == sf::Mouse::Left)
    {
        sliderDragging = false;
    }

    if (event.type == sf::Event::MouseMoved && sliderDragging)
    {
        sf::Vector2f mousePos(
            static_cast<float>(event.mouseMove.x),
            static_cast<float>(event.mouseMove.y)
        );

        float frac = (mousePos.x - sliderBounds.left) / sliderBounds.width;
        frac = std::max(0.0f, std::min(1.0f, frac));

        int targetRound = static_cast<int>(frac * (currentState.totalRounds - 1));
        goToRound(targetRound);
        return;
    }

    if (event.type == sf::Event::KeyPressed)
    {
        switch (event.key.code)
        {
            case sf::Keyboard::Right:
            case sf::Keyboard::ArrowRight:
                stepForward();
                break;

            case sf::Keyboard::Left:
            case sf::Keyboard::ArrowLeft:
                stepBackward();
                break;

            case sf::Keyboard::Space:
                setAutoPlay(!autoPlay);
                break;

            case sf::Keyboard::H:
                skipHand();
                break;

            case sf::Keyboard::R:
                skipRound();
                break;

            case sf::Keyboard::Escape:
            case sf::Keyboard::Backspace:
                exitReplay();
                break;

            // Jump to specific round by number (1-9)
            case sf::Keyboard::Num1:
            case sf::Keyboard::Numpad1:
                if (currentState.totalRounds > 0)
                    goToRound(std::min(1, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num2:
            case sf::Keyboard::Numpad2:
                if (currentState.totalRounds > 1)
                    goToRound(std::min(2, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num3:
            case sf::Keyboard::Numpad3:
                if (currentState.totalRounds > 2)
                    goToRound(std::min(3, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num4:
            case sf::Keyboard::Numpad4:
                if (currentState.totalRounds > 3)
                    goToRound(std::min(4, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num5:
            case sf::Keyboard::Numpad5:
                if (currentState.totalRounds > 4)
                    goToRound(std::min(5, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num6:
            case sf::Keyboard::Numpad6:
                if (currentState.totalRounds > 5)
                    goToRound(std::min(6, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num7:
            case sf::Keyboard::Numpad7:
                if (currentState.totalRounds > 6)
                    goToRound(std::min(7, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num8:
            case sf::Keyboard::Numpad8:
                if (currentState.totalRounds > 7)
                    goToRound(std::min(8, currentState.totalRounds - 1));
                break;
            case sf::Keyboard::Num9:
            case sf::Keyboard::Numpad9:
                if (currentState.totalRounds > 8)
                    goToRound(std::min(9, currentState.totalRounds - 1));
                break;

            default:
                break;
        }
    }
}

// --------------------------------------------------
// Update
// --------------------------------------------------

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
    sliderBounds.left = 60.0f;
    sliderBounds.top = static_cast<float>(size.y) - 130.0f;
    sliderBounds.width = static_cast<float>(size.x - 120);
    sliderBounds.height = 16.0f;
}

// --------------------------------------------------
// Drawing
// --------------------------------------------------

void Replay::draw()
{
    window.clear(sf::Color(20, 20, 30));

    if (mode == Mode::HistoryList)
    {
        drawHistoryList();
    }
    else
    {
        drawReplayView();
    }

    window.display();
}

void Replay::drawHistoryList()
{
    sf::Text title;
    title.setFont(font);
    title.setCharacterSize(28);
    title.setFillColor(sf::Color::White);
    title.setString("Review History — Select a Shoe");
    title.setPosition(60.f, 30.f);
    window.draw(title);

    sf::Text instructions;
    instructions.setFont(font);
    instructions.setCharacterSize(16);
    instructions.setFillColor(sf::Color(180, 180, 180));
    instructions.setString("↑/↓ navigate  •  Enter: replay  •  Esc: exit");
    instructions.setPosition(60.f, 70.f);
    window.draw(instructions);

    if (historyEntries.empty())
    {
        sf::Text none;
        none.setFont(font);
        none.setCharacterSize(20);
        none.setFillColor(sf::Color(150, 150, 150));
        none.setString("No shoes recorded yet.");
        none.setPosition(60.f, 120.f);
        window.draw(none);
        return;
    }

    float y = 120.f;
    const float lineHeight = 40.f;

    for (int i = 0; i < static_cast<int>(historyEntries.size()); ++i)
    {
        const auto& entry = historyEntries[i];

        bool selected = (i == selectedHistoryIndex);

        std::ostringstream oss;
        oss << "Shoe #" << (i + 1) << " — Rounds: " << entry.roundCount
            << " | Cards: " << entry.cardsDealt
            << " | Net: " << (entry.netProfit >= 0 ? "+" : "") << entry.netProfit
            << " | " << (entry.completed ? "Completed" : "Incomplete");

        sf::Text line;
        line.setFont(font);
        line.setCharacterSize(18);
        line.setFillColor(selected ? sf::Color(255, 215, 0) : sf::Color(200, 200, 200));
        line.setString(oss.str());

        if (selected)
        {
            sf::RectangleShape bg(sf::Vector2f(1200.f, lineHeight));
            bg.setPosition(50.f, y - 5.f);
            bg.setFillColor(sf::Color(30, 30, 50));
            bg.setOutlineColor(sf::Color(255, 215, 0));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
        }

        line.setPosition(60.f, y);
        window.draw(line);

        y += lineHeight;
    }
}

void Replay::drawReplayView()
{
    const auto& state = currentState;

    // Background (reuse game's table feel)
    sf::RectangleShape table(sf::Vector2f(window.getSize().x, window.getSize().y));
    table.setFillColor(sf::Color(15, 50, 30));
    window.draw(table);

    // Title bar
    sf::Text title;
    title.setFont(font);
    title.setCharacterSize(20);
    title.setFillColor(sf::Color::White);
    title.setString("Replay — Shoe #" + std::to_string(currentShoeIndex + 1) +
                    " | Round " + std::to_string(state.roundIndex + 1) +
                    " of " + std::to_string(state.totalRounds));
    title.setPosition(60.f, 15.f);
    window.draw(title);

    // Dealer cards (top center)
    drawTable(state);

    // Right panel: analysis overlay
    drawAnalysisOverlay(state);

    // Bottom: round slider
    drawRoundSlider(state);

    // Bottom: controls
    drawControls(state);

    // Step summary
    drawStepSummary(state);
}

void Replay::drawTable(const ReplayInternals::ReplayState& state)
{
    sf::Vector2u size = window.getSize();
    float centerX = size.x / 2.0f - 200.0f;
    float dealerY = 100.f;
    float playerY = size.y - 300.f;

    // --- Dealer Cards ---
    for (size_t i = 0; i < state.dealerHand.size(); ++i)
    {
        float x = centerX + static_cast<float>(i) * 60.f;

        if (i == 0 && !state.dealerHoleCardVisible)
        {
            sf::Sprite back = Graphics::makeCardBackSprite(cardTexture);
            back.setPosition(x, dealerY);
            window.draw(back);
        }
        else
        {
            sf::Sprite card = Graphics::makeCardSprite(state.dealerHand.getCard(i), cardTexture);
            card.setPosition(x, dealerY);
            window.draw(card);
        }
    }


    // Dealer total label
    if (state.dealerHand.size() > 0)
    {
        int dealerValue = state.dealerHand.getValue();
        sf::Text label;
        label.setFont(font);
        label.setCharacterSize(16);
        label.setFillColor(state.dealerHoleCardVisible ? sf::Color::White : sf::Color(150, 150, 150));
        label.setString(state.dealerHoleCardVisible
                            ? "Dealer: " + std::to_string(dealerValue)
                            : "Dealer: ?");
        label.setPosition(centerX + state.dealerHand.size() * 60.f + 10.f, dealerY + 20.f);
        window.draw(label);
    }

    // --- Player hands ---
    for (size_t h = 0; h < state.playerHands.size(); ++h)
    {
        const auto& hand = state.playerHands[h];
        float handX = centerX - 100.f + static_cast<float>(h) * 140.f;

        for (size_t c = 0; c < hand.cards.size(); ++c)
        {
            sf::Sprite card = Graphics::makeCardSprite(hand.cards.getCard(c), cardTexture);
            card.setPosition(handX + static_cast<float>(c) * 16.f, playerY - c * 16.f);
            window.draw(card);
        }

        sf::Text label;
        label.setFont(font);
        label.setCharacterSize(14);
        std::string statusStr = std::to_string(hand.getValue());
        if (hand.isBlackjack())
            statusStr += " BJ";
        else if (hand.isBust())
            statusStr += " BUST";
        else if (hand.isSurrendered)
            statusStr += " S";

        label.setFillColor((h == static_cast<size_t>(state.currentHandIndex))
                               // src/Replay.cpp (continued from drawTable label color)

                               ? sf::Color(255, 215, 0)
                               : sf::Color(200, 200, 200));
        label.setString(statusStr + " | $" + std::to_string(hand.bet));
        label.setPosition(handX, playerY - hand.cards.size() * 16.f - 20.f);
        window.draw(label);
    }
}

void Replay::drawAnalysisOverlay(const ReplayInternals::ReplayState& state)
{
    sf::Vector2u size = window.getSize();
    float panelX = size.x - 280.f;
    float y = 30.f;
    const float lineHeight = 22.f;
    int bankrollAtStep = startingBankroll + state.shoeProfit;

    // Panel background
    sf::RectangleShape bg(sf::Vector2f(260.f, size.y - 60.f));
    bg.setPosition(panelX, 10.f);
    bg.setFillColor(sf::Color(15, 15, 25));
    bg.setOutlineColor(sf::Color(60, 60, 90));
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    // Title
    sf::Text title;
    title.setFont(font);
    title.setCharacterSize(16);
    title.setFillColor(sf::Color(255, 215, 0));
    title.setString("Analysis");
    title.setPosition(panelX + 10.f, y);
    window.draw(title);
    y += lineHeight + 5.f;

    // --- Count overlay ---
    sf::Text countLabel;
    countLabel.setFont(font);
    countLabel.setCharacterSize(14);
    countLabel.setFillColor(sf::Color(200, 200, 200));
    countLabel.setString("Running Count: " + std::to_string(state.runningCount));
    countLabel.setPosition(panelX + 10.f, y);
    window.draw(countLabel);
    y += lineHeight;

    sf::Text tcLabel;
    tcLabel.setFont(font);
    tcLabel.setCharacterSize(14);
    tcLabel.setFillColor(sf::Color(200, 200, 200));
    std::ostringstream tcStream;
    tcStream << std::fixed << std::setprecision(1);
    tcStream << "True Count: " << state.trueCount;
    tcLabel.setString(tcStream.str());
    tcLabel.setPosition(panelX + 10.f, y);
    window.draw(tcLabel);
    y += lineHeight;

    sf::Text remLabel;
    remLabel.setFont(font);
    remLabel.setCharacterSize(14);
    remLabel.setFillColor(sf::Color(160, 160, 160));
    remLabel.setString("Cards Remaining: " + std::to_string(state.cardsRemaining));
    remLabel.setPosition(panelX + 10.f, y);
    window.draw(remLabel);
    y += lineHeight + 8.f;

    // --- Basic Strategy overlay ---
    if (!state.playerHands.empty() && state.dealerHand.size() >= 2 &&
        !state.playerHands[0].isFinished)
    {
        sf::Text bsTitle;
        bsTitle.setFont(font);
        bsTitle.setCharacterSize(14);
        bsTitle.setFillColor(sf::Color(180, 180, 180));
        bsTitle.setString("Basic Strategy:");
        bsTitle.setPosition(panelX + 10.f, y);
        window.draw(bsTitle);
        y += lineHeight;

        char bookAction = getBasicStrategyAction(state);
        sf::Text bsAction;
        bsAction.setFont(font);
        bsAction.setCharacterSize(18);
        bsAction.setFillColor(sf::Color(255, 215, 0));

        std::string actionStr;
        switch (bookAction)
        {
            case 'H': actionStr = "Hit"; break;
            case 'S': actionStr = "Stand"; break;
            case 'D': actionStr = "Double"; break;
            case 'P':
            case 'A': actionStr = "Split"; break;
            case 'R': actionStr = "Surrender"; break;
            default: actionStr = "—"; break;
        }
        bsAction.setString(actionStr);
        bsAction.setPosition(panelX + 10.f, y);
        window.draw(bsAction);
        y += lineHeight + 8.f;

        // Current actual action
        if (!state.playerHands[0].actionsTaken.empty())
        {
            const std::string& actual = state.playerHands[0].actionsTaken.back();
            sf::Text actualLabel;
            actualLabel.setFont(font);
            actualLabel.setCharacterSize(13);
            actualLabel.setFillColor(sf::Color(160, 160, 160));

            std::string actualStr;
            if (actual == "H") actualStr = "Hit";
            else if (actual == "S") actualStr = "Stand";
            else if (actual == "D") actualStr = "Double";
            else if (actual == "P") actualStr = "Split";
            else if (actual == "R") actualStr = "Surrender";
            else actualStr = actual;

            bool matches = (bookAction == actual[0] || bookAction == tolower(actual[0]));
            actualLabel.setFillColor(matches ? sf::Color(100, 200, 100) : sf::Color(255, 200, 50));
            actualLabel.setString("Actual: " + actualStr +
                                  (matches ? " ✓" : " (deviation)"));
            actualLabel.setPosition(panelX + 10.f, y);
            window.draw(actualLabel);
            y += lineHeight + 8.f;
        }
    }
    
    // --- Kelly Bet overlay ---
    sf::Text kellyTitle;
    kellyTitle.setFont(font);
    kellyTitle.setCharacterSize(14);
    kellyTitle.setFillColor(sf::Color(180, 180, 180));
    kellyTitle.setString("Kelly Optimal Bet:");
    kellyTitle.setPosition(panelX + 10.f, y);
    window.draw(kellyTitle);
    y += lineHeight;

    float kellyFraction = 0.33f;
    int kellyBet = calculateKellyBet(
        state.trueCount,
        kellyFraction,
        bankrollAtStep,
        Constants::MINIMUM_BET,
        Constants::MAXIMUM_BET
    );

    sf::Text kellyLabel;
    kellyLabel.setFont(font);
    kellyLabel.setCharacterSize(16);
    kellyLabel.setFillColor(sf::Color(150, 220, 255));
    kellyLabel.setString("$" + std::to_string(kellyBet));
    kellyLabel.setPosition(panelX + 10.f, y);
    window.draw(kellyLabel);
    y += lineHeight + 8.f;
    

    // --- Profit tracking ---
    sf::Text profitTitle;
    profitTitle.setFont(font);
    profitTitle.setCharacterSize(14);
    profitTitle.setFillColor(sf::Color(180, 180, 180));
    profitTitle.setString("Profit");
    profitTitle.setPosition(panelX + 10.f, y);
    window.draw(profitTitle);
    y += lineHeight;

    sf::Text roundProfitLabel;
    roundProfitLabel.setFont(font);
    roundProfitLabel.setCharacterSize(14);
    roundProfitLabel.setFillColor(state.roundProfit >= 0 ? sf::Color(100, 200, 100)
                                                        : sf::Color(220, 100, 100));
    roundProfitLabel.setString("This Round: " +
                               (state.roundProfit >= 0 ? "+" : "") +
                               std::to_string(state.roundProfit));
    roundProfitLabel.setPosition(panelX + 10.f, y);
    window.draw(roundProfitLabel);
    y += lineHeight;

    sf::Text shoeProfitLabel;
    shoeProfitLabel.setFont(font);
    shoeProfitLabel.setCharacterSize(14);
    shoeProfitLabel.setFillColor(state.shoeProfit >= 0 ? sf::Color(100, 200, 100)
                                                      : sf::Color(220, 100, 100));
    shoeProfitLabel.setString("Shoe So Far: " +
                              (state.shoeProfit >= 0 ? "+" : "") +
                              std::to_string(state.shoeProfit));
    shoeProfitLabel.setPosition(panelX + 10.f, y);
    window.draw(shoeProfitLabel);
    y += lineHeight + 8.f;

    // --- Session context ---
    sf::Text contextLabel;
    contextLabel.setFont(font);
    contextLabel.setCharacterSize(12);
    contextLabel.setFillColor(sf::Color(140, 140, 140));
    contextLabel.setString("Shoe " + std::to_string(currentShoeIndex + 1));
    contextLabel.setPosition(panelX + 10.f, y);
    window.draw(contextLabel);
    y += lineHeight;
}

void Replay::drawRoundSlider(const ReplayInternals::ReplayState& state)
{
    sf::Vector2u size = window.getSize();
    sliderBounds.left = 60.0f;
    sliderBounds.top = static_cast<float>(size.y) - 130.0f;
    sliderBounds.width = static_cast<float>(size.x - 120);
    sliderBounds.height = 16.0f;

    // Track background
    sf::RectangleShape track(sliderBounds.getSize());
    track.setPosition(sliderBounds.left, sliderBounds.top);
    track.setFillColor(sf::Color(40, 40, 50));
    track.setOutlineColor(sf::Color(80, 80, 100));
    track.setOutlineThickness(1.f);
    window.draw(track);

    if (state.totalRounds <= 1)
        return;

    // Round markers
    float markerSpacing = sliderBounds.width / static_cast<float>(state.totalRounds - 1);
    for (int r = 0; r < state.totalRounds; ++r)
    {
        float x = sliderBounds.left + r * markerSpacing;
        sf::RectangleShape marker(sf::Vector2f(3.f, sliderBounds.height));
        marker.setPosition(x - 1.5f, sliderBounds.top);

        const auto& info = roundInfos[r];
        if (info.netProfit > 0)
            marker.setFillColor(sf::Color(80, 180, 80));
        else if (info.netProfit < 0)
            marker.setFillColor(sf::Color(180, 80, 80));
        else
            marker.setFillColor(sf::Color(120, 120, 120));

        window.draw(marker);
    }

    // Current position handle
    float frac = state.totalRounds > 1
                     ? static_cast<float>(state.roundIndex) / static_cast<float>(state.totalRounds - 1)
                     : 0.0f;
    float handleX = sliderBounds.left + frac * sliderBounds.width;

    sf::CircleShape handle(8.f);
    handle.setPosition(handleX - 8.f, sliderBounds.top + sliderBounds.height / 2.f - 8.f);
    handle.setFillColor(sf::Color(255, 215, 0));
    handle.setOutlineColor(sf::Color::White);
    handle.setOutlineThickness(2.f);
    window.draw(handle);

    // Round label below slider
    sf::Text roundLabel;
    roundLabel.setFont(font);
    roundLabel.setCharacterSize(12);
    roundLabel.setFillColor(sf::Color(180, 180, 180));
    roundLabel.setString("Round " + std::to_string(state.roundIndex + 1) + " of " +
                         std::to_string(state.totalRounds));
    sf::FloatRect lb = roundLabel.getLocalBounds();
    roundLabel.setOrigin(lb.width / 2.f, 0.f);
    roundLabel.setPosition(handleX, sliderBounds.top + sliderBounds.height + 4.f);
    window.draw(roundLabel);

    // Hover info (if on a round marker)
    if (!roundInfos.empty() && state.roundIndex >= 0 &&
        state.roundIndex < static_cast<int>(roundInfos.size()))
    {
        const auto& info = roundInfos[state.roundIndex];
        sf::Text hoverInfo;
        hoverInfo.setFont(font);
        hoverInfo.setCharacterSize(11);
        hoverInfo.setFillColor(sf::Color(160, 160, 160));
        hoverInfo.setString("Net: " + (info.netProfit >= 0 ? "+" : "") +
                            std::to_string(info.netProfit) +
                            " | Hands: " + std::to_string(info.handCount) +
                            " | Max TC: " + std::to_string(static_cast<int>(info.maxTrueCount)));
        hoverInfo.setPosition(handleX, sliderBounds.top + sliderBounds.height + 20.f);
        window.draw(hoverInfo);
    }
}

void Replay::drawControls(const ReplayInternals::ReplayState& state)
{
    sf::Vector2u size = window.getSize();
    float y = static_cast<float>(size.y) - 80.f;
    float centerX = size.x / 2.0f;

    // Control bar background
    sf::RectangleShape bar(sf::Vector2f(size.x - 120.f, 60.f));
    bar.setPosition(60.f, y - 55.f);
    bar.setFillColor(sf::Color(20, 20, 30));
    bar.setOutlineColor(sf::Color(60, 60, 80));
    bar.setOutlineThickness(1.f);
    window.draw(bar);

    // Status: play/pause indicator
    sf::Text statusText;
    statusText.setFont(font);
    statusText.setCharacterSize(16);
    statusText.setFillColor(autoPlay ? sf::Color(100, 200, 100) : sf::Color(180, 180, 180));
    statusText.setString(autoPlay ? "▶ Playing" : "⏸ Paused");
    statusText.setPosition(70.f, y - 15.f);
    window.draw(statusText);

    // Step info
    sf::Text stepInfo;
    stepInfo.setFont(font);
    stepInfo.setCharacterSize(12);
    stepInfo.setFillColor(sf::Color(140, 140, 140));
    stepInfo.setString("Step " + std::to_string(state.currentStepIndex + 1) +
                       " / " + std::to_string(state.totalSteps));
    stepInfo.setPosition(70.f, y + 5.f);
    window.draw(stepInfo);

    // Keyboard shortcuts legend (right side)
    sf::Text legend;
    legend.setFont(font);
    legend.setCharacterSize(12);
    legend.setFillColor(sf::Color(140, 140, 140));
    legend.setString("←/→ Step  •  Space Play/Pause  •  H Skip Hand  •  R Skip Round  •  Esc Exit");
    sf::FloatRect lb = legend.getLocalBounds();
    legend.setPosition(size.x - 70.f - lb.width, y - 15.f);
    window.draw(legend);
}

void Replay::drawStepSummary(const ReplayInternals::ReplayState& state)
{
    if (steps.empty() || state.currentStepIndex < 0 ||
        state.currentStepIndex >= static_cast<int>(steps.size()))
        return;

    const auto& step = steps[state.currentStepIndex];

    std::string summary;
    switch (step.type)
    {
        case ReplayInternals::ReplayStep::Type::DealCard:
            summary = step.isDealerCard
                          ? "Dealer receives " + step.card.getBlackjackValueToString()
                          : "Player hand " + std::to_string(step.handIndex + 1) +
                                " receives " + step.card.getBlackjackValueToString();
            break;
        case ReplayInternals::ReplayStep::Type::PlayerDecision:
        {
            std::string actionName;
            if (step.action == "H") actionName = "Hit";
            else if (step.action == "S") actionName = "Stand";
            else if (step.action == "D") actionName = "Double";
            else if (step.action == "P") actionName = "Split";
            else if (step.action == "R") actionName = "Surrender";
            else actionName = step.action;
            summary = "Player hand " + std::to_string(step.handIndex + 1) + " " + actionName;
            break;
        }
        case ReplayInternals::ReplayStep::Type::InsuranceOffer:
            summary = "Insurance offered (Dealer shows Ace)";
            break;
        case ReplayInternals::ReplayStep::Type::InsuranceDecision:
            summary = step.insuranceTaken
                          ? "Insurance taken ($" + std::to_string(step.insuranceAmount) + ")"
                          : "Insurance declined";
            break;
        case ReplayInternals::ReplayStep::Type::DealerDraw:
            summary = "Dealer draws " + step.card.getBlackjackValueToString();
            break;
        case ReplayInternals::ReplayStep::Type::RoundEnd:
            summary = "Round " + std::to_string(step.roundIndex + 1) + " complete";
            break;
    }

    sf::Text summaryText;
    summaryText.setFont(font);
    // src/Replay.cpp (continued from drawStepSummary)

    summaryText.setCharacterSize(14);
    summaryText.setFillColor(sf::Color(255, 215, 0));
    summaryText.setString(summary);

    sf::Vector2u size = window.getSize();
    summaryText.setPosition(60.f, size.y - 160.f);
    window.draw(summaryText);
}

// --------------------------------------------------
// Basic Strategy Helper
// --------------------------------------------------
char Replay::getBasicStrategyAction(const ReplayInternals::ReplayState& state) const
{
    if (state.playerHands.empty() || state.dealerHand.size() < 2)
        return 'H';

    const auto& hand = state.playerHands[0];

    int playerValue = hand.getValue();
    int dealerUpValue = state.dealerHand.getCard(0).getBlackjackValue();

    bool hasAce = hand.cards.isSoft();

    bool isPair = hand.cards.size() == 2 &&
                  hand.cards.getCard(0).getBlackjackValue() ==
                  hand.cards.getCard(1).getBlackjackValue();

    int pairValue = isPair ? hand.cards.getCard(0).getBlackjackValue() : 0;

    return BasicStrategy::getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);
}
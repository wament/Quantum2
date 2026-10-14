// View layer for replay playback. The rendering methods below were moved from the Replay::draw*
// functions into this class; every access to the owning engine's members is qualified with
// `replay.` while window/font/cardTexture are this view's own resources.

#include "ReplayView.hpp"
#include "Replay.hpp"
#include "Graphics.hpp"
#include "Constants.hpp"
#include "BasicStrategy.hpp"
#include <sstream>
#include <iomanip>
#include <string>

ReplayView::ReplayView(sf::RenderWindow &window, sf::Texture &cardTexture, sf::Font &font, Replay &replay)
    : window(window), cardTexture(cardTexture), font(font), replay(replay)
{
}

void ReplayView::drawHistoryList()
{
    sf::Text title(font, "Review History - Select a Shoe", 28);
    title.setString("Review History - Select a Shoe");
    title.setPosition({60.f, 30.f});
    window.draw(title);

    sf::Text instructions(font, "Up/Down navigate  *  Enter: replay  *  Esc: exit", 16);
    instructions.setString("Up/Down navigate  *  Enter: replay  *  Esc: exit");
    instructions.setPosition({60.f, 70.f});
    window.draw(instructions);

    if (replay.historyEntries.empty())
    {
        sf::Text none(font, "No shoes recorded yet.", 20);
        none.setString("No shoes recorded yet.");
        none.setPosition({60.f, 120.f});
        window.draw(none);
        return;
    }

    const float topStart = 120.f; // row origin matches replay.hoveredHistoryRow
    const float lineHeight = 40.f;
    int maxOffset = replay.maxHistoryScrollOffset();

    int hovered = replay.hoveredHistoryRow(replay.replayCursorPos);

    for (int i = 0; i < static_cast<int>(replay.historyEntries.size()); ++i)
    {
        const auto &entry = replay.historyEntries[i];

        // Scroll offset shifts every row up so off-screen entries are drawn and selectable in view.
        float rowTop = topStart + static_cast<float>(i) * lineHeight -
                       static_cast<float>(replay.historyScrollOffset) * lineHeight;

        // Clip rows that have scrolled up into the header: once a row's top reaches the top of
        // the scroll area it disappears, so list text never draws over the title/instructions.
        if (rowTop < topStart) continue;

        // Highlight both the keyboard-selected row and whichever row is under the cursor so a
        // hover then left-click gives the same feedback as every other menu.
        bool selected = (i == replay.selectedHistoryIndex || i == hovered);

        std::ostringstream oss;
        oss << "Shoe #" << (i + 1) << " - Rounds: " << entry.roundCount
            << " | Cards: " << entry.cardsDealt
            << " | Net: " << (entry.netProfit >= 0 ? "+" : "") << entry.netProfit
            << " | " << (entry.completed ? "Completed" : "Incomplete");

        sf::Text line(font, oss.str(), 18);
        line.setString(oss.str());

        if (selected)
        {
            sf::RectangleShape bg(sf::Vector2f(1200.f, lineHeight));
            bg.setPosition({50.f, rowTop - 5.f});
            bg.setFillColor(sf::Color(30, 30, 50));
            bg.setOutlineColor(sf::Color(255, 215, 0));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
        }

        line.setPosition({60.f, rowTop});
        window.draw(line);
    }

    // Vertical scrollbar: only drawn when the list overflows so the scroll range is discoverable.
    if (maxOffset > 0 && replay.historyVisibleRowCount() > 0)
    {
        float trackX = 1258.f;
        float trackW = 10.f;
        float trackH = window.getSize().y - topStart;

        sf::RectangleShape track(sf::Vector2f(trackW, trackH));
        track.setPosition({trackX, topStart});
        track.setFillColor(sf::Color(60, 60, 80));
        window.draw(track);

        float thumbFrac = static_cast<float>(replay.historyVisibleRowCount()) /
                          static_cast<float>(replay.historyEntries.size());
        float scrollFrac = maxOffset > 0 ? static_cast<float>(replay.historyScrollOffset) /
                                           static_cast<float>(maxOffset) : 0.f;
        float thumbH = std::max(16.f, trackH * thumbFrac);
        float thumbTop = (trackH - thumbH) > 0.f ? topStart + scrollFrac * (trackH - thumbH) : topStart;

        sf::RectangleShape thumb(sf::Vector2f(trackW, thumbH));
        thumb.setPosition({trackX, thumbTop});
        thumb.setFillColor(sf::Color(255, 215, 0));
        window.draw(thumb);
    }
}

void ReplayView::drawReplayView()
{
    const auto &state = replay.currentState;

    // Background (reuse game's table feel)
    sf::RectangleShape table(sf::Vector2f(window.getSize().x, window.getSize().y));
    table.setFillColor(sf::Color(15, 50, 30));
    window.draw(table);

    // Title bar
    sf::Text title(font, "", 20);
    title.setString("Replay - Shoe #" + std::to_string(replay.currentShoeIndex + 1) +
                    " | Round " + std::to_string(state.roundIndex + 1) +
                    " of " + std::to_string(state.totalRounds));
    title.setPosition({60.f, 15.f});
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

void ReplayView::drawTable(const ReplayInternals::ReplayState &state)
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
            back.setPosition({x, dealerY});
            window.draw(back);
        }
        else
        {
            sf::Sprite card = Graphics::makeCardSprite(state.dealerHand.getCard(i), cardTexture);
            card.setPosition({x, dealerY});
            window.draw(card);
        }
    }

    // Dealer total label
    if (state.dealerHand.size() > 0)
    {
        int dealerValue = state.dealerHand.getValue();
        sf::Text label(font, "", 16);
        label.setString(state.dealerHoleCardVisible
                            ? "Dealer: " + std::to_string(dealerValue)
                            : "Dealer: ?");
        label.setPosition({centerX + state.dealerHand.size() * 60.f + 10.f, dealerY + 20.f});
        window.draw(label);
    }

    // --- Player hands ---
    for (size_t h = 0; h < state.playerHands.size(); ++h)
    {
        const auto &hand = state.playerHands[h];
        float handX = centerX - 100.f + static_cast<float>(h) * 140.f;

        for (size_t c = 0; c < hand.cards.size(); ++c)
        {
            sf::Sprite card = Graphics::makeCardSprite(hand.cards.getCard(c), cardTexture);
            card.setPosition({handX + static_cast<float>(c) * 16.f, playerY - c * 16.f});
            window.draw(card);
        }

        sf::Text label(font, "", 14);
        std::string statusStr = std::to_string(hand.getValue());
        if (hand.isBlackjack())
            statusStr += " BJ";
        else if (hand.isBust())
            statusStr += " BUST";
        else if (hand.isSurrendered)
            statusStr += " S";

        label.setFillColor((h == static_cast<size_t>(state.currentHandIndex))

                               ? sf::Color(255, 215, 0)
                               : sf::Color(200, 200, 200));
        std::string seatName = hand.playerName.empty() ? "Player" : hand.playerName;
        label.setString(seatName + ": " + statusStr + " | $" + std::to_string(hand.bet));
        label.setPosition({handX, playerY - hand.cards.size() * 16.f - 20.f});
        window.draw(label);
    }
}

void ReplayView::drawAnalysisOverlay(const ReplayInternals::ReplayState &state)
{
    sf::Vector2u size = window.getSize();
    float panelX = size.x - 280.f;
    float y = 45.f;
    const float lineHeight = 22.f;
    int bankrollAtStep = replay.startingBankroll + state.shoeProfit;

    // Panel background
    sf::RectangleShape bg(sf::Vector2f(260.f, size.y - 60.f));
    bg.setPosition({panelX, 10.f});
    bg.setFillColor(sf::Color(15, 15, 25));
    bg.setOutlineColor(sf::Color(60, 60, 90));
    bg.setOutlineThickness(1.f);
    window.draw(bg);

    // Title
    sf::Text title(font, "Analysis", 16);
    title.setString("Analysis");
    title.setPosition({panelX + 10.f, y});
    window.draw(title);
    y += lineHeight + 5.f;

    // --- Count overlay ---
    sf::Text countLabel(font, "Running Count: " + std::to_string(state.runningCount), 14);
    countLabel.setString("Running Count: " + std::to_string(state.runningCount));
    countLabel.setPosition({panelX + 10.f, y});
    window.draw(countLabel);
    y += lineHeight;

    sf::Text tcLabel(font, "", 14);
    tcLabel.setFillColor(sf::Color(200, 200, 200));
    std::ostringstream tcStream;
    tcStream << std::fixed << std::setprecision(1);
    tcStream << "True Count: " << state.trueCount;
    tcLabel.setString(tcStream.str());
    tcLabel.setPosition({panelX + 10.f, y});
    window.draw(tcLabel);
    y += lineHeight;

    sf::Text remLabel(font, "Cards Remaining: " + std::to_string(state.cardsRemaining), 14);
    remLabel.setString("Cards Remaining: " + std::to_string(state.cardsRemaining));
    remLabel.setPosition({panelX + 10.f, y});
    window.draw(remLabel);
    y += lineHeight + 8.f;

    // --- Basic Strategy overlay ---
    if (!state.playerHands.empty() && state.dealerHand.size() >= 2 &&
        state.currentHandIndex >= 0 &&
        state.currentHandIndex < static_cast<int>(state.playerHands.size()) &&
        !state.playerHands[state.currentHandIndex].isFinished)
    {
        sf::Text bsTitle(font, "Basic Strategy:", 14);
        bsTitle.setString("Basic Strategy:");
        bsTitle.setPosition({panelX + 10.f, y});
        window.draw(bsTitle);
        y += lineHeight;

        char bookAction = replay.getBasicStrategyAction(state);
        sf::Text bsAction(font, "", 18);
        bsAction.setFillColor(sf::Color(255, 215, 0));

        std::string actionStr;
        switch (bookAction)
        {
        case 'H':
            actionStr = "Hit";
            break;
        case 'S':
            actionStr = "Stand";
            break;
        case 'D':
            actionStr = "Double";
            break;
        case 'P':
        case 'A':
            actionStr = "Split";
            break;
        case 'R':
            actionStr = "Surrender";
            break;
        default:
            actionStr = "-";
            break;
        }
        bsAction.setString(actionStr);
        bsAction.setPosition({panelX + 10.f, y});
        window.draw(bsAction);
        y += lineHeight + 8.f;

        // Current actual action
        int handIdx = state.currentHandIndex;
        if (handIdx >= 0 && handIdx < static_cast<int>(state.playerHands.size()) &&
            !state.playerHands[handIdx].actionsTaken.empty())
        {
            const std::string &actual = state.playerHands[handIdx].actionsTaken.back();
            sf::Text actualLabel(font, "", 13);
            actualLabel.setFillColor(sf::Color(160, 160, 160));

            std::string actualStr;
            if (actual == "H")
                actualStr = "Hit";
            else if (actual == "S")
                actualStr = "Stand";
            else if (actual == "D")
                actualStr = "Double";
            else if (actual == "P")
                actualStr = "Split";
            else if (actual == "R")
                actualStr = "Surrender";
            else
                actualStr = actual;

            bool matches = (bookAction == actual[0] || bookAction == tolower(actual[0]));
            actualLabel.setFillColor(matches ? sf::Color(100, 200, 100) : sf::Color(255, 200, 50));
            actualLabel.setString("Actual: " + actualStr +
                                  (matches ? " [ok]" : " (deviation)"));
            actualLabel.setPosition({panelX + 10.f, y});
            window.draw(actualLabel);
            y += lineHeight + 8.f;
        }
    }

    // --- Kelly Bet overlay ---
    sf::Text kellyTitle(font, "Kelly Optimal Bet:", 14);
    kellyTitle.setString("Kelly Optimal Bet:");
    kellyTitle.setPosition({panelX + 10.f, y});
    window.draw(kellyTitle);
    y += lineHeight;

    float kellyFraction = 0.33f;
    int kellyBet = BasicStrategy::calculateKellyBet(
        state.trueCount,
        kellyFraction,
        bankrollAtStep,
        Constants::MINIMUM_BET,
        Constants::MAXIMUM_BET);

    sf::Text kellyLabel(font, "$" + std::to_string(kellyBet), 16);
    kellyLabel.setString("$" + std::to_string(kellyBet));
    kellyLabel.setPosition({panelX + 10.f, y});
    window.draw(kellyLabel);
    y += lineHeight + 8.f;

    if (!state.playerHands.empty())
    {
        int actualBet = 0;
        for (const auto &h : state.playerHands)
            actualBet += h.bet;
        bool betMatches = (actualBet == kellyBet || actualBet == 0);
        sf::Text kellyCompare(font, "", 12);
        kellyCompare.setString(std::string("Actual: $") + std::to_string(actualBet) +
                               (betMatches ? " [ok]" : " (mismatch)"));
        kellyCompare.setPosition({panelX + 10.f, y});
        window.draw(kellyCompare);
        y += lineHeight;
    }

    // --- Profit tracking ---
    sf::Text profitTitle(font, "Profit", 14);
    profitTitle.setString("Profit");
    profitTitle.setPosition({panelX + 10.f, y});
    window.draw(profitTitle);
    y += lineHeight;

    sf::Text roundProfitLabel(font, "", 14);
    roundProfitLabel.setFillColor(state.roundProfit >= 0 ? sf::Color(100, 200, 100)
                                                         : sf::Color(220, 100, 100));
    roundProfitLabel.setString(std::string("This Round: ") +
                               (state.roundProfit >= 0 ? "+" : "") +
                               std::to_string(state.roundProfit));

    roundProfitLabel.setPosition({panelX + 10.f, y});
    window.draw(roundProfitLabel);
    y += lineHeight;

    sf::Text shoeProfitLabel(font, "", 14);
    shoeProfitLabel.setFillColor(state.shoeProfit >= 0 ? sf::Color(100, 200, 100)
                                                       : sf::Color(220, 100, 100));
    shoeProfitLabel.setString(std::string("Shoe So Far: ") +
                              (state.shoeProfit >= 0 ? "+" : "") +
                              std::to_string(state.shoeProfit));
    shoeProfitLabel.setPosition({panelX + 10.f, y});
    window.draw(shoeProfitLabel);
    y += lineHeight + 8.f;

    // --- Session context ---
    sf::Text contextLabel(font, "Shoe " + std::to_string(replay.currentShoeIndex + 1), 12);
    contextLabel.setString("Shoe " + std::to_string(replay.currentShoeIndex + 1));
    contextLabel.setPosition({panelX + 10.f, y});
    window.draw(contextLabel);
    y += lineHeight;
}

void ReplayView::drawRoundSlider(const ReplayInternals::ReplayState &state)
{
    sf::Vector2u size = window.getSize();
    replay.sliderBounds.position.x = 60.0f;
    replay.sliderBounds.position.y = static_cast<float>(size.y) - 150.0f;
    replay.sliderBounds.size.x = static_cast<float>(size.x - 120);
    replay.sliderBounds.size.y = 16.0f;

    // Track background
    sf::RectangleShape track(sf::Vector2f(replay.sliderBounds.size.x, replay.sliderBounds.size.y));
    track.setPosition({replay.sliderBounds.position.x, replay.sliderBounds.position.y});
    track.setFillColor(sf::Color(40, 40, 50));
    track.setOutlineColor(sf::Color(80, 80, 100));
    track.setOutlineThickness(1.f);
    window.draw(track);

    if (state.totalRounds <= 1)
        return;

    // Round markers
    float markerSpacing = replay.sliderBounds.size.x / static_cast<float>(state.totalRounds - 1);
    for (int r = 0; r < state.totalRounds; ++r)
    {
        float x = replay.sliderBounds.position.x + r * markerSpacing;
        sf::RectangleShape marker(sf::Vector2f(3.f, replay.sliderBounds.size.y));
        marker.setPosition({x - 1.5f, replay.sliderBounds.position.y});

        const auto &info = replay.roundInfos[r];
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
    float handleX = replay.sliderBounds.position.x + frac * replay.sliderBounds.size.x;

    sf::CircleShape handle(8.f);
    handle.setPosition({handleX - 8.f, replay.sliderBounds.position.y + replay.sliderBounds.size.y / 2.f - 8.f});
    handle.setFillColor(sf::Color(255, 215, 0));
    handle.setOutlineColor(sf::Color::White);
    handle.setOutlineThickness(2.f);
    window.draw(handle);

    // Round label below slider
    sf::Text roundLabel(font, "", 12);
    roundLabel.setString("Round " + std::to_string(state.roundIndex + 1) + " of " +
                         std::to_string(state.totalRounds));
    sf::FloatRect lb = roundLabel.getLocalBounds();
    roundLabel.setOrigin({lb.size.x / 2.f, 0.f});
    roundLabel.setPosition({handleX, replay.sliderBounds.position.y + replay.sliderBounds.size.y + 4.f});
    window.draw(roundLabel);

    // Hover info (if on a round marker)
    if (!replay.roundInfos.empty() && state.roundIndex >= 0 &&
        state.roundIndex < static_cast<int>(replay.roundInfos.size()))
    {
        const auto &info = replay.roundInfos[state.roundIndex];
        sf::Text hoverInfo(font, "", 11);
        hoverInfo.setString(std::string("Net: ") +
                            (info.netProfit >= 0 ? "+" : "") +
                            std::to_string(info.netProfit) +
                            " | Hands: " + std::to_string(info.handCount) +
                            " | Max TC: " + std::to_string(static_cast<int>(info.maxTrueCount)));
        hoverInfo.setPosition({handleX, replay.sliderBounds.position.y + replay.sliderBounds.size.y + 20.f});
        window.draw(hoverInfo);
    }
}

void ReplayView::drawControls(const ReplayInternals::ReplayState &state)
{
    sf::Vector2u size = window.getSize();
    float y = static_cast<float>(size.y) - 60.f;
    float centerX = size.x / 2.0f;

    // Control bar background
    sf::RectangleShape bar(sf::Vector2f(size.x - 120.f, 60.f));
    bar.setPosition({60.f, y - 55.f});
    bar.setFillColor(sf::Color(20, 20, 30));
    bar.setOutlineColor(sf::Color(60, 60, 80));
    bar.setOutlineThickness(1.f);
    window.draw(bar);

    // Status: play/pause indicator
    sf::Text statusText(font, replay.autoPlay ? "Playing" : "Paused", 16);
    statusText.setString(replay.autoPlay ? "Playing" : "Paused");
    statusText.setPosition({70.f, y - 15.f});
    window.draw(statusText);

    // Step info
    sf::Text stepInfo(font, "", 12);
    stepInfo.setString("Step " + std::to_string(state.currentStepIndex + 1) +
                       " / " + std::to_string(state.totalSteps));
    stepInfo.setPosition({70.f, y + 5.f});
    window.draw(stepInfo);

    // Keyboard shortcuts legend (right side)
    sf::Text legend(font, "Left/Right Step  *  Space Play/Pause  *  H Skip Hand  *  R Skip Round  *  Esc Exit", 12);
    legend.setString("Left/Right Step  *  Space Play/Pause  *  H Skip Hand  *  R Skip Round  *  Esc Exit");
    sf::FloatRect lb = legend.getLocalBounds();
    legend.setPosition({size.x - 70.f - lb.size.x, y - 15.f});
    window.draw(legend);
}

void ReplayView::drawStepSummary(const ReplayInternals::ReplayState &state)
{
    if (replay.steps.empty() || state.currentStepIndex < 0 ||
        state.currentStepIndex >= static_cast<int>(replay.steps.size()))
        return;

    const auto &step = replay.steps[state.currentStepIndex];

    // Seat identity for this step (falls back to "Player" for legacy records)
    std::string seatName = "Player";
    if (!state.playerHands.empty() &&
        static_cast<size_t>(step.handIndex) < state.playerHands.size())
        seatName = state.playerHands[step.handIndex].playerName;
    if (seatName.empty()) seatName = "Player";

    std::string summary;
    switch (step.type)
    {
    case ReplayInternals::ReplayStep::Type::DealCard:
        summary = step.isDealerCard
                      ? "Dealer receives " + step.card.getBlackjackValueToString()
                      : seatName + " receives " + step.card.getBlackjackValueToString();
        break;
    case ReplayInternals::ReplayStep::Type::PlayerDecision:
    {
        std::string actionName;
        if (step.action == "H")
            actionName = "Hit";
        else if (step.action == "S")
            actionName = "Stand";
        else if (step.action == "D")
            actionName = "Double";
        else if (step.action == "P")
            actionName = "Split";
        else if (step.action == "R")
            actionName = "Surrender";
        else
            actionName = step.action;
        summary = seatName + " " + actionName;
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

    sf::Text summaryText(font, summary, 14);
    summaryText.setFillColor(sf::Color(255, 215, 0));

    sf::Vector2u size = window.getSize();
    summaryText.setPosition({60.f, size.y - 180.f});
    window.draw(summaryText);
}

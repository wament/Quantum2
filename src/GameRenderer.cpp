#include <cmath>
#include "Game.hpp"
#include "GameRenderer.hpp"
#include "SideBetLabels.hpp"
#include "Constants.hpp"
#include "Graphics.hpp"
#include "Replay.hpp"
#include "StrategyTrainer.hpp"
#include "Tournament.hpp"
// drawn via game.statsDashboard->draw(); the renderer needs the complete type, just as
// it already fully-includes Replay/StrategyTrainer/Tournament above.
#include "StatsDashboard.hpp"
#include "FontManager.hpp"

#include <algorithm>
#include <functional>
#include <optional>

// Single source of truth for draw()'s AppMode->{backdrop, render} dispatch: one table replaces the old backdrop
// if/else-if chain + separate dispatch switch
// Per-mode modules still call their own ->draw(window); this only routes to them after clearing. Render lambdas use the
// enclosing GameRenderer via implicit `this`
// are non-const and must be called on that live renderer).
const std::map<AppMode, ModeEntry>& GameRenderer::modeTable()
{
    // Single source of truth for draw()'s AppMode->{backdrop, render} dispatch: one table replaces the old
    // backdrop if/else-if chain + separate dispatch switch
    // drift apart. Built once on first use; the render lambdas capture this directly (we are in the GameRenderer member), so
    // they call the non-const draw* methods on that live instance
    static std::map<AppMode, ModeEntry> table;
    if (!table.empty())
        return table;

    // Populating for the first time: one row per AppMode. `t[X] = {backdrop, render}` keeps each entry's backdrop color and
    // its render fn on the same line so they can't drift apart either.
    table[AppMode::MainMenu] = {std::nullopt, [=](Game& g)
                                { drawMainMenu(g); }};
    // Shared felt table + HUD + betting/side-bet/summary/achievement readouts, shared by Playing and Tournament (so a tournament
    // renders exactly like an ordinary game with no duplicate rendering). Drawn once per frame by whichever mode is active.
    table[AppMode::Playing] = {std::nullopt, [=](Game& g)
                               { drawPlaySession(g); }};
    // Paused: same felt + HUD readout as Playing but with the pause menu instead of betting/side-bet overlays.
    table[AppMode::Paused] = {std::nullopt, [=](Game& g)
                              {
                                  drawGameTable(g);
                                  g.updateHUDState();
                                  g.hud.update(g.hudState);
                                  g.hud.draw(g.window);
                                  drawSoundStatus(g);
                                  drawPauseMenu(g);
                              }};
    table[AppMode::StrategyTrainer] = {sf::Color(96, 0, 128), [=](Game& g)
                                       { g.trainer->draw(g.window, g.cardTexture); }};
    // Tournament: reuses the felt table + HUD exactly like Playing (maximum code reuse -- you play an ordinary shoe with real
    // decisions), then layers on the tournament banner during play and the end-of-tournament leaderboard once it is complete.
    table[AppMode::Tournament] = {std::nullopt, [=](Game& g)
                                  {
                                      drawPlaySession(g);
                                      if (g.tournament && !g.tournament->isComplete())
                                          drawTournamentBanner(g);
                                      else if (g.tournament && g.tournament->isComplete())
                                          drawTournamentLeaderboard(g);
                                  }};
    table[AppMode::Achievements] = {sf::Color(18, 28, 24), [=](Game& g)
                                    { drawAchievementList(g); }};
    table[AppMode::GameRules] = {sf::Color(30, 30, 40), [=](Game& g)
                                 { drawGameRules(g); }};
    table[AppMode::StatsDashboard] = {sf::Color(15, 22, 30), [=](Game& g)
                                      { if (g.statsDashboard) g.statsDashboard->draw(g.window); }};
    // ReplayHistory + ReplayPlaying share one render fn; felt cleared by the default branch above.
    table[AppMode::ReplayHistory] = {std::nullopt, [=](Game& g)
                                     { g.replay->draw(); }};
    table[AppMode::ReplayPlaying] = {std::nullopt, [=](Game& g)
                                     { g.replay->draw(); }};

    return table;
}

void GameRenderer::draw(Game& game)
{
    // Unknown mode: default backdrop (plain clear + backgroundSprite) and no render
    static const ModeEntry fallback{std::nullopt, [](Game&) {}};
    auto it = modeTable().find(game.appMode);
    const ModeEntry& entry = (it != modeTable().end()) ? it->second : fallback;

    if (entry.backdrop)
        game.window.clear(*entry.backdrop);
    else
    {
        game.window.clear();
        game.window.draw(*game.backgroundSprite);
    }

    entry.render(game);

    game.window.display();
}

ModeEntry GameRenderer::modeEntryForTest(AppMode appMode)
{
    static const ModeEntry fallback{std::nullopt, [](Game&) {}}; // nullopt backdrop (=> default clear + backgroundSprite), no-op render for an unknown mode
    auto it = modeTable().find(appMode);
    return (it != modeTable().end()) ? it->second : fallback;
}

// render the full achievement list: each entry shows its unlocked/locked state plus name and
// description. Pure presentation; reads game.achievementManager_ through the shared Game friend link, exactly like drawSoundStatus
// reads sound_. The Escape-to-menu control lives in InputHandler::processAchievementEvents.
void GameRenderer::drawAchievementList(Game& game)
{
    const auto& defs = game.achievementManager_.all();
    const float w = static_cast<float>(game.window.getSize().x);
    const float h = static_cast<float>(game.window.getSize().y);
    const float cx = w / 2.0f;

    sf::Text title(FontManager::get(), "ACHIEVEMENTS", 34);
    title.setFillColor(sf::Color(255, 215, 0));
    const auto tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    title.setPosition(sf::Vector2f(cx, 40.f));
    game.window.draw(title);

    float y = 96.f;
    const float rowH = 60.f; // state + name line and a description line per achievement
    for (const auto& def : defs)
    {
        sf::Text state(FontManager::get(), def.unlocked ? "[X]" : "[ ]", 22);
        state.setFillColor(def.unlocked ? sf::Color(103, 255, 103) : sf::Color(160, 160, 160));
        state.setPosition(sf::Vector2f(cx - 224.f, y));
        game.window.draw(state);

        sf::Text name(FontManager::get(), def.name, 20);
        name.setFillColor(def.unlocked ? sf::Color::White : sf::Color(190, 190, 190));
        name.setPosition(sf::Vector2f(cx - 184.f, y));
        game.window.draw(name);

        sf::Text desc(FontManager::get(), def.description, 14);
        desc.setFillColor(sf::Color(160, 160, 160));
        desc.setPosition(sf::Vector2f(cx - 184.f, y + 26.f));
        game.window.draw(desc);

        y += rowH;
    }

    sf::Text hint(FontManager::get(), "Esc: back", 14);
    hint.setFillColor(sf::Color(140, 140, 140));
    hint.setPosition(sf::Vector2f(cx, h - 28.f));
    game.window.draw(hint);
}

// brief popup for the most recently unlocked achievement during play. Shown only while HUDState has a name set
// (Game sets it at settlement and clears it next round); drawn just above the end-of-round summary line.
void GameRenderer::drawPendingAchievement(Game& game)
{
    if (game.hudState.pendingAchievementName.empty())
        return;

    const float w = static_cast<float>(game.window.getSize().x);
    sf::Text banner(FontManager::get(), std::string("NEW! ") + game.hudState.pendingAchievementName, 18);
    banner.setFillColor(sf::Color(255, 215, 0));
    const auto bb = banner.getLocalBounds();
    banner.setOrigin(sf::Vector2f(bb.size.x / 2.0f, 0));
    banner.setPosition(sf::Vector2f(w / 2.0f, 452.f));
    game.window.draw(banner);
}

// Shared table-draw for Playing and Paused states (dealer + players + chips).
void GameRenderer::drawGameTable(Game& game)
{
    // advance the reveal clock by one frame and clear stale reveal state if a round reset is detected, before
    // any cards are drawn so every card in this table share one consistent animation timeline. Presentation only.
    advanceRevealClock(game);

    // draw the shoe stack first so it sits behind every dealt card. Cards slide out from this
    // single visible deck near the dealer
    drawDeck(game);

    // Lay out split hands once so cards, chips and the active-player highlight all line up.
    std::vector<PlayerHandLayout> lanes = allocatePlayerLanes(game);

    drawDealer(game);
    drawPlayers(game, lanes);
    drawChips(game, lanes);
}

// thin sound-status indicator at the bottom-right corner
void GameRenderer::drawSoundStatus(Game& game) const
{
    const SoundManager::Settings s = game.sound_.settings();
    std::string label = s.muted ? "Sound: Muted" : "Sound: On";

    sf::Text text(FontManager::get());
    text.setString(label);
    text.setCharacterSize(14);
    text.setFillColor(sf::Color::White);

    const float x = static_cast<float>(Constants::WINDOW_WIDTH) - 96.f;
    const float y = static_cast<float>(Constants::WINDOW_HEIGHT) - 26.f;
    text.setPosition(sf::Vector2f{x, y});

    game.window.draw(text);
}

// Split hands fan up-and-to-the-right from each seat's betting circle; the vertical step keeps them from piling on
// one another or onto a neighbour, whose group is nudged outward when their footprints collide (below).
std::vector<PlayerHandLayout> GameRenderer::allocatePlayerLanes(Game& game) const
{
    const std::size_t n = game.players.size();
    if (n == 0)
        return {};

    const float spreadX = Constants::SPLIT_SPREAD_X;
    const float spreadY = Constants::SPLIT_SPREAD_Y;
    const float leftBound = Constants::HANDS_LEFT_MARGIN;
    const float rightBound = Constants::HANDS_RIGHT_MARGIN;

    // Cascade step per split hand (0 for a single hand; else j-(hands-2)): newest takes the up-right slot, earlier ones
    // shift one step down-left; <=2 hands reproduce the original staircase exactly.
    auto stepAt = [](int j, int hands) -> float
    {
        if (hands <= 1)
            return 0.f;
        return static_cast<float>(j - (hands - 2));
    };

    // Track each seat's horizontal footprint (card left..right edge + padding) for neighbour collisions; for 3-4 hands
    // it extends left of the anchor because earlier split hands step down-left.
    std::vector<float> cx(n), leftEdge(n), rightEdge(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        cx[i] = Constants::PLAYER_X + getPlayerPosition(game, i, n).x;

        const int hands = static_cast<int>(game.players[i].hands.size());
        float minX = cx[i], maxX = cx[i];
        for (int j = 0; j < hands; ++j)
        {
            const float xj = cx[i] + stepAt(j, hands) * spreadX;
            minX = std::min(minX, xj);
            maxX = std::max(maxX, xj + Constants::CARD_WIDTH);
        }
        leftEdge[i] = minX;
        rightEdge[i] = maxX + Constants::BOX_PAD;
    }

    // Split each seat overlap and cascade the push outward
    // colliders); repeated passes settle it within MAX_SPLIT_HANDS+1 seats, leaving uncollided ones exactly put.
    for (std::size_t pass = 0; pass < n; ++pass)
    {
        bool changed = false;
        for (std::size_t i = 0; i + 1 < n; ++i)
        {
            const float overlap = rightEdge[i] - leftEdge[i + 1];
            if (overlap > 0.0f)
            {
                const float half = overlap * 0.5f;
                cx[i] -= half;
                leftEdge[i] -= half;
                rightEdge[i] -= half;
                cx[i + 1] += half;
                leftEdge[i + 1] += half;
                rightEdge[i + 1] += half;
                changed = true;
            }
        }
        if (!changed)
            break;
    }

    // Clamp the row into the usable band: a no-op when seats fit, else nudge the whole row to clear an overflowing
    // margin (right on left-edge overflow, left on right). We only move on actual overflow
    // is room on one side would drag hands and chips off their rings in normal play.
    float shift = 0.0f;
    if (leftEdge[0] < leftBound)
        shift = leftBound - leftEdge[0]; // too far left: nudge right
    else if (rightEdge[n - 1] > rightBound)
        shift = rightBound - rightEdge[n - 1]; // too far right: nudge left
    for (std::size_t i = 0; i < n; ++i)
    {
        cx[i] += shift;
        leftEdge[i] += shift;
        rightEdge[i] += shift;
    }

    // Convert seats to per-hand x/y. drawChips reads the same leftX/offsetY
    // own slot (e.g. the 3rd of 4 hands sits at step 0 = its original betting circle).
    std::vector<PlayerHandLayout> lanes(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const int hands = static_cast<int>(game.players[i].hands.size());
        PlayerHandLayout& lane = lanes[i];
        lane.cx = cx[i];
        lane.cw = Constants::CARD_WIDTH + static_cast<float>((hands > 0 ? hands : 1) - 1) * spreadX + Constants::BOX_PAD;
        for (int j = 0; j < hands; ++j)
        {
            const float step = stepAt(j, hands);
            lane.leftX.push_back(cx[i] + step * spreadX);
            lane.offsetY.push_back(-step * spreadY);
        }
    }

    return lanes;
}

void GameRenderer::resizeView(sf::RenderWindow& window, sf::View& view)
{
    float windowRatio = static_cast<float>(window.getSize().x) /
                        static_cast<float>(window.getSize().y);
    float viewRatio = view.getSize().x / view.getSize().y;

    if (windowRatio > viewRatio)
    {
        view.setViewport(sf::FloatRect(
            {(1.f - viewRatio / windowRatio) / 2.f, 0.f},
            {viewRatio / windowRatio, 1.f}));
    }
    else
    {
        view.setViewport(sf::FloatRect(
            {0.f, (1.f - windowRatio / viewRatio) / 2.f},
            {1.f, windowRatio / viewRatio}));
    }

    window.setView(view);
}

void GameRenderer::drawDealer(Game& game)
{
    if (game.roundState == RoundState::PlayerTurn || game.roundState == RoundState::OfferInsurance)
    {
        // slide the up card and the (face-down) hole card in from the shoe over their staggered reveal window.
        // The two end-points match the settled DealerTurn layout (DEALER_X, then +card spacing)
        // a seamless flip rather than a jump
        Graphics::CardDealTween upTween;
        upTween.start = sf::Vector2f(Constants::SHOE_X, Constants::SHOE_Y);
        upTween.end = sf::Vector2f(Constants::DEALER_X, Constants::DEALER_Y);
        upTween.duration = dealSlideSeconds(game);
        upTween.elapsed = std::max(0.f, dealClock_ - dealerRevealAtFor(game, 0, dealClock_));
        auto up = Graphics::makeCardSprite(game.dealer.getCard(0), game.cardTexture);
        up.setPosition(upTween.position());
        game.window.draw(up);

        Graphics::CardDealTween holeTween;
        holeTween.start = sf::Vector2f(Constants::SHOE_X, Constants::SHOE_Y);
        holeTween.end = sf::Vector2f(Constants::DEALER_X + Constants::CARD_WIDTH + 8.f, Constants::DEALER_Y);
        holeTween.duration = dealSlideSeconds(game);
        holeTween.elapsed = std::max(0.f, dealClock_ - dealerRevealAtFor(game, 1, dealClock_));
        auto back = Graphics::makeCardBackSprite(game.cardTexture);
        back.setPosition(holeTween.position());
        game.window.draw(back);
        return;
    }

    // seed the hole-card flip the first time this round's full dealer hand is revealed (the transition into
    // DealerTurn / Cleanup). The turn then eases that card from an edge-on pose to flat
    if (!dealerFlipStarted_ && game.dealer.size() >= 2)
    {
        dealerFlipStarted_ = true;
        dealerFlipStart_ = dealClock_;
    }

    for (std::size_t i = 0; i < game.dealer.size(); i++)
    {
        // slide each revealed card in from the shoe over its staggered reveal window. Cards are spaced one
        // width plus a small gap apart, matching the PlayerTurn end-points
        Graphics::CardDealTween tween;
        tween.start = sf::Vector2f(Constants::SHOE_X, Constants::SHOE_Y);
        tween.end = sf::Vector2f(Constants::DEALER_X + i * (Constants::CARD_WIDTH + 8.f), Constants::DEALER_Y);
        tween.duration = dealSlideSeconds(game);
        tween.elapsed = std::max(0.f, dealClock_ - dealerRevealAtFor(game, (int)i, dealClock_));

        auto sprite = Graphics::makeCardSprite(game.dealer.getCard(i), game.cardTexture);
        sprite.setPosition(tween.position());

        // flip the hole card (index 1) as it is revealed
        // pivot is set to the card's centre first, then positioned by an extra half-card
        // slot (identical to the non-flipped cards), while rotating around the geometric centre --
        // always settles exactly upright instead of drifting off-slot or getting stuck edge-on.
        if (i == 1 && dealerFlipStarted_ && dealerFlipStart_ >= 0.f)
        {
            tween.flipping = true;
            tween.flipDuration = holeFlipSeconds(game);
            tween.flipElapsed = std::max(0.f, dealClock_ - dealerFlipStart_);
            float rotationDeg = 90.f * (1.f - tween.flipProgress());

            sprite.setOrigin(sf::Vector2f(Constants::CARD_WIDTH / 2.f, Constants::CARD_HEIGHT / 2.f));
            sprite.setPosition(sf::Vector2f(tween.position().x + Constants::CARD_WIDTH / 2.f,
                                            tween.position().y + Constants::CARD_HEIGHT / 2.f));
            sprite.rotate(sf::degrees(rotationDeg));
        }

        game.window.draw(sprite);
    }
    // Draw the dealer's total to the left of their hand, once the hand has finished.
    if (game.roundState == RoundState::DealerTurn || game.roundState == RoundState::Cleanup)
    {
        if (game.dealer.size() > 0)
            drawTotal(game, game.dealer.getValue(), Constants::DEALER_X - 40.f,
                      Constants::DEALER_Y + Constants::CARD_HEIGHT / 2.f, true);

        // Center the label horizontally over the dealer's card row and vertically on the cards' midline.
        int n = static_cast<int>(game.dealer.size());
        float cx = Constants::DEALER_X + static_cast<float>(n - 1) * 30.f + Constants::CARD_WIDTH / 2.f;
        float cy = Constants::DEALER_Y + Constants::CARD_HEIGHT / 2.f;

        // Draw BUSTED label centered over the busted dealer hand.
        if (game.dealer.isBust())
            drawStatusLabel(game, "BUSTED", cx, cy, sf::Color(255, 0, 0));

        // Draw BLACKJACK! callout centered over a blackjack dealer hand.
        if (game.dealer.isBlackjack())
            drawStatusLabel(game, "BLACKJACK!", cx, cy, sf::Color(0, 180, 0));
    }
}

sf::Vector2f GameRenderer::getPlayerPosition(Game& game, size_t playerIndex, size_t totalPlayers) const
{
    // Seats map onto fixed betting-circle rings (0-indexed); the six-row layout below is that projection and smaller
    // seat counts reuse a subset, so every hand stays on a ring.
    static const int slotMap[7][6] = {
        {},
        {2},               // 1 player -> circle 3 (centre)
        {2, 4},            // 2 players -> circles 2 and 4
        {1, 2, 3},         // 3 players -> circles 1, 3, 5
        {1, 2, 3, 4},      // 4 players -> circles 1, 3, 4, 6
        {0, 1, 2, 3, 4},   // 5 players -> every ring except circle 6
        {0, 1, 2, 3, 4, 5} // 6 players -> all rings (unchanged)
    };

    const size_t slot = slotMap[totalPlayers][playerIndex];

    // Manual per-player offsets for fine-tuning placement
    static const sf::Vector2f playerOffsets[6] = {
        {-21.f, -1.f}, // Player 0
        {-3.f, -9.f},  // Player 1
        {-1.f, 0.f},   // Player 2
        {8.f, 0.f},    // Player 3
        {7.f, -3.f},   // Player 4
        {6.f, 7.f}     // Player 5
    };
    // Arrange players in an arc around the table. The felt
    // with the window, while card sprites stay a fixed 64px
    // and fitted so each seat's chip stack lands in its printed ring: radiusX tracks the window width (the rings
    // spread horizontally), radiusY the height, and centerY absorbs the fixed card-height + chip offset that
    // sits between a seat anchor and its ring.
    float centerX = static_cast<float>(Constants::WINDOW_WIDTH) / 2.f - 12;
    float centerY = static_cast<float>(Constants::WINDOW_HEIGHT) * 0.888f;
    float radiusX = static_cast<float>(Constants::WINDOW_WIDTH) * 0.286f;
    float radiusY = static_cast<float>(Constants::WINDOW_HEIGHT) * 0.5f;

    // Fixed six-ring projection: reuse the 6-player ring centers
    const size_t n = 6;
    float arcSpan = 60.f + (n - 2) * 15.f;
    float startAngle = -arcSpan / 2.f;
    float angleStep = arcSpan / (n - 1);

    float angle = startAngle + static_cast<float>(slot) * angleStep;
    float angleRad = angle * 3.14159f / 180.f;

    float x = centerX + radiusX * sin(angleRad);
    float y = centerY - radiusY * cosh(0.78 * angleRad);

    sf::Vector2f offset = (slot < 6) ? playerOffsets[slot] : sf::Vector2f(0.f, 0.f);
    return {x - Constants::PLAYER_X + offset.x, y - Constants::PLAYER_Y + offset.y};
}

// Draws a numeric hand total in white (16pt). With rightAlign the number's right edge sits at x
// (used for the dealer, flush left of its cards); otherwise it is centered on the anchor (player
// hands above each card fan).
void GameRenderer::drawTotal(Game& game, int value, float x, float y, bool rightAlign) const
{
    sf::Text text(FontManager::get(), std::to_string(value), 16.f);
    text.setFillColor(sf::Color::White);
    sf::FloatRect bounds = text.getLocalBounds();
    if (rightAlign)
        text.setOrigin(sf::Vector2f(bounds.size.x, bounds.size.y / 2.f));
    else
        text.setOrigin(sf::Vector2f(bounds.size.x / 2.f, bounds.size.y / 2.f));
    text.setPosition(sf::Vector2f(x, y));
    game.window.draw(text);
}

// Draw a status label ("BUSTED"/"BLACKJACK!") centered horizontally and vertically over a hand's anchor
// point so it sits squarely on the cards instead of flush to one corner. 18pt bold text; color is chosen
// by the caller (red for busted, green for "Blackjack!").
void GameRenderer::drawStatusLabel(Game& game, const std::string& label, float cx, float cy, const sf::Color& color) const
{
    sf::Text text(FontManager::get(), label, 18.f);
    text.setStyle(sf::Text::Bold);
    text.setFillColor(color);
    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(sf::Vector2f(bounds.size.x / 2.f, bounds.size.y / 2.f));
    text.setPosition(sf::Vector2f(cx, cy));
    game.window.draw(text);
}

void GameRenderer::drawPlayers(Game& game, const std::vector<PlayerHandLayout>& lanes)
{
    for (size_t playerIndex = 0; playerIndex < game.players.size(); playerIndex++)
    {
        auto& p = game.players[playerIndex];
        sf::Vector2f pos = getPlayerPosition(game, playerIndex, game.players.size());
        float xOffset = pos.x;
        float yOffset = pos.y;

        // Draw player name and bankroll above their hand
        std::string playerLabel = p.getName() + "  $" + std::to_string(p.getBankroll());
        sf::Text bankrollText(FontManager::get(), playerLabel, 16.f);
        bankrollText.setFillColor(sf::Color(180, 255, 180));
        bankrollText.setPosition(sf::Vector2f(Constants::PLAYER_X + xOffset - 20, Constants::PLAYER_Y + yOffset + 160.f));
        game.window.draw(bankrollText);

        if (playerIndex == game.activePlayerIndex && game.roundState == RoundState::PlayerTurn)
        {
            const PlayerHandLayout& lane = lanes[playerIndex];
            sf::RectangleShape highlight(sf::Vector2f(lane.cw + 80.f, 260.f));
            highlight.setFillColor(sf::Color(255, 255, 0, 30));
            highlight.setOutlineColor(sf::Color(255, 255, 0, 150));
            highlight.setOutlineThickness(2.f);
            highlight.setPosition(sf::Vector2f(lane.cx - (lane.cw) * 0.5f, Constants::PLAYER_Y + yOffset - 40.f));
            game.window.draw(highlight);

            sf::Text turnLabel(FontManager::get(), "YOUR TURN", 14.f);
            turnLabel.setFillColor(sf::Color(255, 255, 0));
            turnLabel.setPosition(sf::Vector2f(lane.cx, Constants::PLAYER_Y + yOffset + 180.f));
            game.window.draw(turnLabel);
        }

        for (std::size_t handIndex = 0; handIndex < p.hands.size(); handIndex++)
        {
            const Hand& hand = p.hands[handIndex];
            float handX = lanes[playerIndex].leftX[handIndex];
            float handY = Constants::PLAYER_Y + yOffset + lanes[playerIndex].offsetY[handIndex];

            // Rebalance so the bet stays centered on the hand instead of fanning right into the neighbour. This applies to
            // any long fan (>=4 cards, even or odd) and to doubled-down hands; -8*N centers the whole row on the chips (the
            // two middle cards for even counts, the single middle card for odd). A single card, the initial 2-card deal and
            // non-doubled 3-card hands stay put. Note: odd long hands rebalance too
            // them snap back to the original position before.
            const int nCards = static_cast<int>(hand.size());
            float cardShift = 0.f;
            if (nCards > 2 && (nCards >= 4 || hand.isDoubledDown()))
                cardShift = -8.f * static_cast<float>(nCards);
            float fanLeftX = handX + cardShift; // where card 0 actually sits after rebalancing

            // Safety clamp: keep the whole row inside the side-panel band
            // long rebalanced / doubled-down fan. Normal single and double hands sit well within the band and are left
            // exactly put; only an overflowing fan is nudged back in-bounds (preserving its anchored seat).
            const float fanWidth = static_cast<float>(nCards - 1) * 16.f + Constants::CARD_WIDTH;
            if (fanLeftX < Constants::HANDS_LEFT_MARGIN)
                fanLeftX = Constants::HANDS_LEFT_MARGIN;
            else
            {
                float rightmost = fanLeftX + fanWidth;
                if (rightmost > Constants::HANDS_RIGHT_MARGIN)
                    fanLeftX = Constants::HANDS_RIGHT_MARGIN - fanWidth;
            }

            // Draw cards for this hand
            for (std::size_t i = 0; i < hand.size(); i++)
            {
                float cardX = fanLeftX + i * 16;
                float cardY = handY - i * 16;

                // ease this card in from the shoe over its staggered reveal window. A fully-arrived card sits
                // exactly on (cardX, cardY), so settled frames are pixel-for-pixel identical to the pre-animation layout.
                Graphics::CardDealTween tween;
                tween.start = sf::Vector2f(Constants::SHOE_X, Constants::SHOE_Y);
                tween.end = sf::Vector2f(cardX, cardY);
                tween.duration = dealSlideSeconds(game);
                tween.elapsed = std::max(0.f, dealClock_ - playerRevealAtFor(game, (int)playerIndex, (int)handIndex, (int)i, dealClock_));

                // Only rotate the third card (double down card) 90 degrees
                if (hand.isDoubledDown() && i == 2)
                {
                    auto sprite = Graphics::makeCardSprite(hand.getCard(i), game.cardTexture);
                    sprite.setPosition(tween.position());
                    sprite.setOrigin(sf::Vector2f(Constants::CARD_WIDTH / 2.f, Constants::CARD_HEIGHT / 2.f));
                    sprite.move(sf::Vector2f(Constants::CARD_WIDTH / 2.f, Constants::CARD_HEIGHT / 2.f));
                    sprite.rotate(sf::degrees(90.f));
                    game.window.draw(sprite);
                }
                else
                {
                    auto sprite = Graphics::makeCardSprite(hand.getCard(i), game.cardTexture);
                    sprite.setPosition(tween.position());
                    game.window.draw(sprite);
                }
            }

            // Draw the hand's total above it, once the hand has finished.
            if ((game.roundState == RoundState::DealerTurn ||
                 game.roundState == RoundState::Cleanup) &&
                hand.size() > 0)
            {
                const std::size_t n = hand.size();
                float fanRight = static_cast<float>(n - 1) * 16.f + Constants::CARD_WIDTH;                // right edge of the card row
                float cx = fanLeftX + fanRight / 2.f;                                                     // center horizontally over this hand's rebalanced cards
                float cy = handY - Constants::CARD_HEIGHT / 2.f - 16.f - static_cast<float>(n - 1) * 8.f; // sit above the rising cards
                drawTotal(game, hand.getValue(), cx, cy, false);
            }

            // Draw BUSTED label on top of busted hands
            if (hand.isBust())
            {
                sf::Text bustedText(FontManager::get(), "BUSTED", 18.f);
                bustedText.setFillColor(sf::Color(255, 0, 0));
                bustedText.setStyle(sf::Text::Bold);
                bustedText.setPosition(sf::Vector2f(fanLeftX, handY + 5.f));
                game.window.draw(bustedText);
            }

            // Draw BLACKJACK! callout centered over a blackjack hand (green).
            if (hand.isBlackjack() && hand.size() > 0)
            {
                int pn = static_cast<int>(hand.size());
                float fanRight = static_cast<float>(pn - 1) * 16.f + Constants::CARD_WIDTH;
                float pcx = fanLeftX + fanRight / 2.f;                                               // center horizontally over the rebalanced card row
                float pcy = handY - static_cast<float>(pn - 1) * 8.f + Constants::CARD_HEIGHT / 2.f; // vertical center of the rising cards
                drawStatusLabel(game, "BLACKJACK!", pcx, pcy, sf::Color(0, 180, 0));
            }

            // Draw white flag for surrendered hands
            if (hand.isSurrendered())
            {
                sf::RectangleShape pole(sf::Vector2f(3.f, 30.f));
                pole.setFillColor(sf::Color(128, 128, 128));
                pole.setPosition(sf::Vector2f(fanLeftX + 10, handY - 40));

                sf::RectangleShape flag(sf::Vector2f(20.f, 12.f));
                flag.setFillColor(sf::Color::White);
                flag.setOutlineColor(sf::Color::Black);
                flag.setOutlineThickness(1.f);
                flag.setPosition(sf::Vector2f(fanLeftX + 13, handY - 40));

                game.window.draw(pole);
                game.window.draw(flag);
            }
        }
    }
}

void GameRenderer::drawChips(Game& game, const std::vector<PlayerHandLayout>& lanes)
{
    struct Chip
    {
        int value;
        sf::Color color;
    };

    const std::vector<Chip> denominations =
        {
            {50000, sf::Color(128, 0, 128)},
            {10000, sf::Color::Black},
            {2500, sf::Color(0, 180, 0)},
            {500, sf::Color::Red},
            {250, sf::Color(255, 105, 180)},
            {100, sf::Color::White}};

    for (size_t playerIndex = 0; playerIndex < game.players.size(); playerIndex++)
    {
        auto& p = game.players[playerIndex];
        sf::Vector2f pos = getPlayerPosition(game, playerIndex, game.players.size());
        float xOffset = pos.x;
        float yOffset = pos.y;

        int handIndex = 0;
        for (Hand& hand : p.hands)
        {
            int totalBet = hand.getBet();
            int stackBet = hand.isDoubledDown() ? totalBet / 2 : totalBet;

            if (totalBet <= 0)
            {
                handIndex++;
                continue;
            }

            float chipX = lanes[playerIndex].leftX[handIndex] + Constants::CHIP_BET_X_OFFSET; // sit centered in the felt betting ring, ahead of each hand's first card
            float chipY = Constants::PLAYER_Y + yOffset + lanes[playerIndex].offsetY[handIndex] + Constants::CARD_HEIGHT + 45.f;

            int remaining = stackBet * 100;
            std::vector<Chip> stack;

            for (const auto& chip : denominations)
            {
                while (remaining >= chip.value)
                {
                    stack.push_back(chip);
                    remaining -= chip.value;
                }
            }

            for (std::size_t i = 0; i < stack.size(); i++)
            {
                sf::CircleShape chipShape(Constants::CHIP_RADIUS);
                chipShape.setFillColor(stack[i].color);
                chipShape.setOutlineColor(sf::Color::White);
                chipShape.setOutlineThickness(1.f);
                chipShape.setOrigin(sf::Vector2f(Constants::CHIP_RADIUS, Constants::CHIP_RADIUS));
                chipShape.setPosition(sf::Vector2f(chipX, chipY - i * 3.f));
                game.window.draw(chipShape);
            }

            // Use black text when the top chip is white for contrast
            sf::Color textColor = sf::Color::White;
            if (!stack.empty() && stack.back().color == sf::Color::White)
            {
                textColor = sf::Color::Black;
            }
            sf::Text text(FontManager::get(), "$" + std::to_string(totalBet), 16.f);
            text.setFillColor(textColor);
            sf::FloatRect bounds = text.getLocalBounds();
            text.setOrigin(sf::Vector2f(bounds.size.x / 2.f, bounds.size.y / 2.f));
            text.setPosition(sf::Vector2f(chipX, chipY - stack.size() * 3.f));
            game.window.draw(text);

            if (hand.isDoubledDown())
            {
                float secondX = chipX + 35.f;
                for (std::size_t i = 0; i < stack.size(); i++)
                {
                    sf::CircleShape chipShape(Constants::CHIP_RADIUS);
                    chipShape.setFillColor(stack[i].color);
                    chipShape.setOutlineColor(sf::Color::White);
                    chipShape.setOutlineThickness(1.f);
                    chipShape.setOrigin(sf::Vector2f(Constants::CHIP_RADIUS, Constants::CHIP_RADIUS));
                    chipShape.setPosition(sf::Vector2f(secondX, chipY - i * 3.f));
                    game.window.draw(chipShape);
                }
            }
            handIndex++;
        }
    }
}

void GameRenderer::drawDeck(Game& game)
{
    // Only show the top few cards of the shoe as a stacked face-down edge, positioned at the shoe. Drawing all 52+
    // cards would sprawl across the screen; a short stack reads clearly as a deck while still being "near the dealer".
    if (game.deck.size() == 0)
        return;

    const int show = std::min<int>(game.deck.size(), 6);
    for (int i = 0; i < show; ++i)
    {
        auto back = Graphics::makeCardBackSprite(game.cardTexture);
        back.setPosition(sf::Vector2f(Constants::SHOE_X + static_cast<float>(i) * 3.f,
                                      Constants::SHOE_Y + static_cast<float>(i) * 3.f));
        game.window.draw(back);
    }
}

void GameRenderer::drawInsurance(Game& game)
{
    auto& p = game.players[game.activePlayerIndex];
    sf::Text text(FontManager::get(),
                  "Dealer shows an Ace\n\n"
                  "Insurance Bet: $" +
                      std::to_string(p.hands[p.currentHand].getInsuranceBet()) +
                      "\n\n"
                      "Up/Down : Change\n"
                      "Enter : Confirm\n"
                      "Esc : Decline",
                  12.f);
    text.setFillColor(sf::Color::White);
    text.setPosition(sf::Vector2f(400.f, 500.f));
    game.window.draw(text);
}

static std::string sideBetKindName(SideBetKind kind)
{
    switch (kind)
    {
    case SideBetKind::PerfectPairs:
        return "Perfect Pairs";
    case SideBetKind::TwentyOneThree:
        return "21+3";
    default:
        return "None";
    }
}

// presentation-only reveal-timing helpers. They own NO game state -- only animation
// clocks keyed by a stable card slot, advanced one frame at a time from game.deltaTime and cleared on round reset. All
// three are pure presentation; the logic/tick path never calls them

void GameRenderer::advanceRevealClock(Game& game)
{
    // Total cards can only grow within a round; any drop means the previous reveal state is stale (a new round begins
    // with empty hands), so clear it and reset flip state before starting a fresh sweep.
    int total = 0;
    for (auto& p : game.players)
        for (const Hand& hh : p.hands)
            total += static_cast<int>(hh.size());
    total += static_cast<int>(game.dealer.size());

    if (total < prevTotalCards_)
    {
        playerRevealAt_.clear();
        dealerRevealAt_.clear();
        dealerFlipStarted_ = false;
        dealerFlipStart_ = -1.f;
        // Re-baseline on the current (low) count instead of retaining the previous round's peak. If we kept the old
        // maximum here, every later round whose card count stayed below that retained peak would keep tripping this
        // test and re-clear the freshly-assigned reveals EVERY frame -- which froze each card's progress near 0
        // never finished sliding into its slot (the "cards get stuck / wrong spot after round 1" bug). A single drop
        // still clears exactly once per round boundary; subsequent frames in that round only clear on a real new one.
        prevTotalCards_ = total;
    }
    else
    {
        // Never let the running count go backwards mid-round
        prevTotalCards_ = std::max(prevTotalCards_, total);
    }

    // Advance the shared animation clock by this frame, in seconds (game.deltaTime), matching the rest of the timing
    // system. Rendering-only: the logic/tick path is untouched.
    dealClock_ += game.deltaTime;
}

float GameRenderer::playerRevealAtFor(Game& game, int p, int h, int i, float now)
{
    auto key = std::make_tuple(p, h, i);
    auto it = playerRevealAt_.find(key);
    if (it != playerRevealAt_.end())
        return it->second; // already animating / arrived: keep the original start so progress continues smoothly

    // Flattened deal-order index -- all earlier seats' and hands' cards come first, then this hand's earlier cards.
    // Captured once at first sight, so a later split in an earlier hand shifts nothing visually (progress only grows).
    int ordinal = 0;
    for (int q = 0; q < p; ++q)
        for (const Hand& hh : game.players[q].hands)
            ordinal += static_cast<int>(hh.size());
    for (std::size_t k = 0; k < static_cast<std::size_t>(h); ++k)
        ordinal += static_cast<int>(game.players[p].hands[k].size());
    ordinal += i;

    // Reveal time INCREASES with deal order
    // crucially, every card starts at progress ~0 on its first sighting being past DEAL_SLIDE_SECONDS.
    // high-ordinal slots (later seats / NPCs / dealer) that exceed their slide duration
    // pop into place without animation — later deals reveal after earlier ones.
    float revealAt = now + static_cast<float>(ordinal) * dealStaggerSeconds(game);
    playerRevealAt_[key] = revealAt;
    return revealAt;
}

float GameRenderer::dealerRevealAtFor(Game& game, int i, float now)
{
    auto it = dealerRevealAt_.find(i);
    if (it != dealerRevealAt_.end())
        return it->second;

    int totalPlayerCards = 0;
    for (auto& p : game.players)
        for (const Hand& hh : p.hands)
            totalPlayerCards += static_cast<int>(hh.size());
    // Dealer cards reveal after the player's
    // ordinal maps to a later reveal time
    float revealAt = now + static_cast<float>(totalPlayerCards + i) * dealStaggerSeconds(game);
    dealerRevealAt_[i] = revealAt;
    return revealAt;
}

// effective durations for the selected game speed. Each base Feature-8
// constant is scaled by GameTiming::dealDurationScale(game.gameSpeed)
// Hyper/Gamer collapse to 0 (Graphics::CardDealTween::progress() then returns exactly 1 -> no visible animation). Presentation-only.
float GameRenderer::dealSlideSeconds(Game& game) const { return Constants::DEAL_SLIDE_SECONDS * GameTiming::dealDurationScale(game.gameSpeed); }
float GameRenderer::dealStaggerSeconds(Game& game) const { return Constants::DEAL_STAGGER_SECONDS * GameTiming::dealDurationScale(game.gameSpeed); }
float GameRenderer::holeFlipSeconds(Game& game) const { return Constants::DEALER_HOLE_FLIP_SECONDS * GameTiming::dealDurationScale(game.gameSpeed); }

// Felt table + HUD + betting/side-bet/summary/achievement readouts shared by Playing and Tournament, so both modes render the
// exact same table without duplicating the call sequence in their dispatch entries. Pure presentation; reads live game state.
void GameRenderer::drawPlaySession(Game& g)
{
    drawGameTable(g);
    if (g.roundState == RoundState::OfferInsurance)
        drawInsurance(g);

    g.updateHUDState();
    g.hud.update(g.hudState);
    g.hud.draw(g.window);
    drawSideBet(g);
    drawSoundStatus(g);
    drawSummaryOverlay(g);
    // brief popup for the most recently unlocked achievement (set by Game at settlement). No-op when none.
    drawPendingAchievement(g);

    // betting-phase control hints, drawn just above the main controls strip in the central play band so they sit clear of the
    // left-panel stats charts. The general main-bet controls are shown throughout PlaceBets; the side-bet line is shown only when
    // the human seat can actually place bets interactively (Game::canPlaceBets).
    if (g.roundState == RoundState::PlaceBets)
    {
        constexpr float controlsY = static_cast<float>(Constants::WINDOW_HEIGHT) - Constants::Hud::BOTTOM_HEIGHT - 24.f; // row of the main controls strip
        const float hintX = Constants::HANDS_LEFT_MARGIN;                                                              // just inside the play band, past the left-panel charts

        sf::Text hint(FontManager::get(),
                      "Up/Down: set bet   Left: optimal   Right: confirm", 11.f);
        hint.setFillColor(sf::Color(180, 180, 180));
        hint.setPosition(sf::Vector2f(hintX, controlsY - 30.f));
        g.window.draw(hint);

        if (g.canPlaceBets())
        {
            sf::Text sideHint(FontManager::get(), "Z: side bet type   X/V: adjust side-bet wager", 11.f);
            sideHint.setFillColor(sf::Color(255, 215, 0));
            sideHint.setPosition(sf::Vector2f(hintX, controlsY - 14.f));
            g.window.draw(sideHint);
        }
    }
}

// Live tournament banner during play: a top-centred "Tournament -- Stage X of N" with this stage's minimum bet, so the marathon's
// progress and escalating pressure are always visible on top of the ordinary felt table. Pure presentation; no-op unless a live,
// incomplete tournament is running.
void GameRenderer::drawTournamentBanner(Game& g)
{
    if (!g.tournamentActive_ || !g.tournament || g.tournament->isComplete())
        return;

    const float cx = g.window.getSize().x / 2.0f;
    auto center = [&](const std::string& label, int size, sf::Color color, float y)
    {
        sf::Text t(FontManager::get(), label, static_cast<unsigned int>(size));
        t.setFillColor(color);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, y));
        g.window.draw(t);
    };

    center("TOURNAMENT", 22, sf::Color(255, 215, 0), 34.f);
    center("Stage " + std::to_string(g.tournament->currentStage()) + " of " +
               std::to_string(g.tournament->stages()) +
               "   |   Stage min bet: " + std::to_string(g.activeMinBet()),
           16, sf::Color(220, 220, 220), 54.f);
}

// End-of-tournament ranked leaderboard + winner, drawn once the final stage has run out. Seats are listed richest-first by bankroll
// (the tournament's real scoring) with the winner highlighted; Escape returns to the menu. Pure presentation.
void GameRenderer::drawTournamentLeaderboard(Game& g)
{
    if (!g.tournamentActive_ || !g.tournament || !g.tournament->isComplete())
        return;

    const float cx = g.window.getSize().x / 2.0f;
    auto center = [&](const std::string& label, int size, sf::Color color, float y)
    {
        sf::Text t(FontManager::get(), label, static_cast<unsigned int>(size));
        t.setFillColor(color);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, y));
        g.window.draw(t);
    };

    std::vector<std::string> names;
    std::vector<int> chips;
    for (const auto& p : g.players)
    {
        names.push_back(p.getName());
        chips.push_back(p.getBankroll());
    }
    const auto order = g.tournament->rankOrder(names, chips);

    center("TOURNAMENT COMPLETE", 34, sf::Color(255, 215, 0), 70.f);

    float y = 120.f;
    for (std::size_t rank = 0; rank < order.size(); ++rank)
    {
        const std::size_t seat = order[rank];
        center("#" + std::to_string(rank + 1) + "  " + names[seat] + ": " + std::to_string(chips[seat]),
               22, rank == 0 ? sf::Color(120, 255, 160) : sf::Color(230, 230, 230), y);
        y += 30.f;
    }

    center("Winner: " + names[order.front()] + " (" + std::to_string(chips[order.front()]) + " chips)",
           26, sf::Color(255, 215, 0), y + 10.f);
    center("Press Escape to return to the menu", 14, sf::Color(150, 150, 150), y + 38.f);
}
// show the human seat's current round side bet in its betting area while it is active, plus the
// resolved outcome once the round settles (DealerTurn/Cleanup). Reads hudState
// the live hand. Only drawn when a side bet was actually placed this round.
void GameRenderer::drawSideBet(Game& game)
{
    if (!game.hudState.hasSideBet || game.players.empty())
        return;

    const auto pos = getPlayerPosition(game, 0, game.players.size());
    float x = Constants::PLAYER_X + pos.x + Constants::CARD_WIDTH / 2.f - 45.f;
    // The seat name + bankroll is drawn at PLAYER_Y + yOffset + 160; the side-bet label shared that exact row
    // (CARD_HEIGHT + 96 == 160) so its text ran straight into the name. Sit it a clear row beneath the name, past the
    // chip stack and the "YOUR TURN" turn-label (+180), so neither overlaps.
    float y = Constants::PLAYER_Y + pos.y + Constants::CARD_HEIGHT + 150.f;

    std::string label = "Side Bet: " + sideBetKindName(game.hudState.sideBetKind) + " $" + std::to_string(game.hudState.sideBetWager);
    if (game.roundState == RoundState::DealerTurn || game.roundState == RoundState::Cleanup)
    {
        label += sidebet::resultLabel(game.hudState.sideBetWager, game.hudState.sideBetPayout);
    }

    sf::Text text(FontManager::get(), label, 12.f);
    text.setFillColor(sf::Color(255, 215, 0));
    text.setPosition(sf::Vector2f(x, y));
    game.window.draw(text);
}

void GameRenderer::drawSummaryOverlay(Game& game)
{
    if (game.hudState.playerSummaries.empty() || game.hudState.summaryTimer <= 0.0f)
        return;

    // Presentation-only pacing: ramp opacity across a ~6s window
    // The display duration itself is decided in GameTiming (game logic); this is purely how it renders.
    constexpr float summaryFadeFactorPerSecond = 255.0f / 6.0f; // == 42.5, full opacity over ~6s of remaining time
    float alpha = static_cast<float>(std::min(255.0f, game.hudState.summaryTimer * summaryFadeFactorPerSecond));
    sf::Color textColor(255, 255, 255, static_cast<unsigned char>(alpha));

    for (size_t i = 0; i < game.hudState.playerSummaries.size(); i++)
    {
        sf::Vector2f pos = getPlayerPosition(game, i, game.players.size());
        float handWidth = Constants::CARD_WIDTH + (game.players[i].hands.size() - 1) * 16;
        float summaryX = Constants::PLAYER_X + pos.x + handWidth / 2.f - 50.f;
        float summaryY = Constants::PLAYER_Y + pos.y + Constants::CARD_HEIGHT + 120.f;

        sf::Text text(FontManager::get(), game.hudState.playerSummaries[i], 14.f);
        text.setFillColor(textColor);
        text.setPosition(sf::Vector2f(summaryX, summaryY));
        game.window.draw(text);
    }
}

void GameRenderer::drawMenuItem(Game& game, const std::string& label, bool selected, float y, bool hovered)
{
    sf::Text text(FontManager::get(), label, 24);

    // Hovered and keyboard-selected options are both drawn in gold; hovering is
    // additionally bolded so the option under the cursor stands out clearly.
    if (hovered || selected)
        text.setFillColor(sf::Color(255, 215, 0));
    else
        text.setFillColor(sf::Color(200, 200, 200));

    if (hovered)
        text.setStyle(sf::Text::Bold);

    sf::FloatRect bounds = text.getLocalBounds();
    text.setOrigin(sf::Vector2f(bounds.size.x / 2.0f, bounds.size.y / 2.0f));
    text.setPosition(sf::Vector2f(static_cast<float>(game.window.getSize().x) / 2.0f, y));
    game.window.draw(text);
}

int GameRenderer::getHoveredMenuItemIndex(Game& game) const
{
    float cx = static_cast<float>(game.window.getSize().x) / 2.0f;
    sf::Vector2f mousePos = game.window.mapPixelToCoords(sf::Mouse::getPosition(game.window));

    int hoveredIndex = -1;
    const float rowHalfHeight = 30.f; // comfortably covers a size-24 menu line
    float bestDistance = rowHalfHeight;
    for (int i = 0; i < static_cast<int>(mainMenuItems().size()); ++i)
    {
        const MainMenuItem& item = mainMenuItems()[i];
        float distance = std::abs(mousePos.y - item.y);
        if (distance < bestDistance && std::abs(mousePos.x - cx) < 250.f)
        {
            // Keep the option whose centre is closest to the cursor so that a
            // point between two rows resolves unambiguously to the nearer one.
            bestDistance = distance;
            hoveredIndex = i;
        }
    }
    return hoveredIndex;
}

// rows sit at startY = h*0.30 + i*spacing (see drawGameRules), so this hit-test matches the
// drawn rows exactly -- one source of truth for hover->selection that processMenuEvents()/processRulesEvents() both consume,
// keeping pointing-at-a-row consistent across menus. Same pattern as getHoveredMenuItemIndex().
int GameRenderer::getHoveredGameRulesRowIndex(Game& game) const
{
    float cx = static_cast<float>(game.window.getSize().x) / 2.0f;
    sf::Vector2f mousePos = game.window.mapPixelToCoords(sf::Mouse::getPosition(game.window));

    int hoveredIndex = -1;
    const float rulesRowHalfHeight = 21.f; // under half the 46px row spacing, so adjacent rows stay unambiguous
    const float startY = static_cast<float>(game.window.getSize().y) * 0.30f;
    const float spacing = 46.f;
    const auto& rows = gameRuleRows();
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        // Rows are uniformly spaced, so the first row whose centre is close enough to the cursor wins. The horizontal band
        // is wider than getHoveredMenuItemIndex()'s because rules rows also carry a longer description column.
        float distance = std::abs(mousePos.y - (startY + i * spacing));
        if (distance < rulesRowHalfHeight && std::abs(mousePos.x - cx) < 420.f)
        {
            hoveredIndex = i;
            break;
        }
    }
    return hoveredIndex;
}

// Vertical pixel-centre of pause-menu row `index` at the current window height. Uses the
// fractional positions from pauseMenuItems() so it matches drawPauseMenu and the click/hover
// hit-testing exactly -- one source of truth for where those rows sit.
float GameRenderer::pauseRowY(Game& game, int index) const
{
    if (index < 0 || index >= static_cast<int>(pauseMenuItems().size()))
        return -1.f;

    float height = game.window.getSize().y;
    return height * pauseMenuItems()[index].yFrac;
}

// Index of the pause-menu row currently under the mouse cursor, or -1 if none. Nearest-centre
// within each row's half-height and centred horizontally around the screen centre
// hover highlighting (drawPauseMenu) and click selection (processPauseEvents) so they agree.
int GameRenderer::getHoveredPauseMenuItemIndex(Game& game) const
{
    constexpr float rowHalfHeight = 20.f;

    sf::Vector2i mousePos = sf::Mouse::getPosition(game.window);
    int hoveredIndex = -1;
    double bestDistance = 1e9;
    for (int i = 0; i < static_cast<int>(pauseMenuItems().size()); ++i)
    {
        float y = pauseRowY(game, i);
        if (y <= 0.f)
            continue;

        // Rows are horizontally centred around the screen's horizontal centre.
        float cx = game.window.getSize().x / 2.0f;
        if (std::abs(static_cast<float>(mousePos.x) - cx) > 150.f)
            continue;

        double distance = std::abs(static_cast<double>(static_cast<float>(mousePos.y) - y));
        if (distance <= static_cast<double>(rowHalfHeight) && distance < bestDistance)
        {
            bestDistance = distance;
            hoveredIndex = i;
        }
    }
    return hoveredIndex;
}

void GameRenderer::drawMainMenu(Game& game)
{
    game.window.clear(sf::Color(20, 20, 30));
    float cx = static_cast<float>(game.window.getSize().x) / 2.0f;

    sf::Text title(FontManager::get(), "BLACKJACK", 36);
    title.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    title.setPosition(sf::Vector2f(cx, 150.0f));
    game.window.draw(title);

    int hoveredIndex = getHoveredMenuItemIndex(game);

    for (int i = 0; i < static_cast<int>(mainMenuItems().size()); ++i)
    {
        const MainMenuItem& item = mainMenuItems()[i];
        std::string label;
        switch (item.option)
        {
        case MenuOption::PlayGame:
            label = "Play Game";
            break;
        case MenuOption::PlayerCount:
            label = "Players: " + std::to_string(game.playerCount);
            break;
        case MenuOption::GameSpeed:
        {
            std::string speedLabel = "Speed: ";
            switch (game.gameSpeed)
            {
            case GameSpeed::Fast:
                speedLabel += "Fast";
                break;
            case GameSpeed::Normal:
                speedLabel += "Normal";
                break;
            case GameSpeed::Slow:
                speedLabel += "Slow";
                break;
            case GameSpeed::AutoPlay:
                speedLabel += "AutoPlay";
                break;
            case GameSpeed::Hyper:
                speedLabel += "Hyper";
                break;
            case GameSpeed::Gamer:
                speedLabel += "Gamer";
                break;
            }
            label = speedLabel;
            break;
        }
        case MenuOption::ReviewHistory:
            label = "Review History";
            break;
        case MenuOption::StrategyTrainer:
            label = "Practice Basic Strategy";
            break;
        case MenuOption::Tournament:
            label = "Tournament Mode";
            break;
        case MenuOption::Achievements:
            label = "Achievements";
            break;
        case MenuOption::GameRules:
            label = "Custom Game Rules";
            break;
        case MenuOption::Statistics:
            label = "Statistics Dashboard";
            break;
        }

        bool hovered = (i == hoveredIndex);
        drawMenuItem(game, label, game.menuSelection == item.option, item.y, hovered);
    }

    // Controls hint sits below every menu row: the last row (Statistics) is at y=540 and rows are 40px apart
    // previously sat at y=470 directly on top of the Achievements row. Kept centered horizontally like the title above.
    sf::Text instr(FontManager::get(), "Up/Down: navigate  Enter: select  Esc: quit", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    instr.setPosition(sf::Vector2f(cx, 585.0f));
    game.window.draw(instr);
}

// render the in-game rules-config panel. Each editable rule is one row: label on the left,
// current value on the right: with the selected row highlighted. Pure presentation: it reads the live gameRules_ plus the
// shared gameRuleRows()/gameRuleDescription() helpers through the Game friend link. Navigation and editing live in
// Game::update(); Escape persists there via rememberTableSettings().
void GameRenderer::drawGameRules(Game& game)
{
    const float w = static_cast<float>(game.window.getSize().x);
    const float h = static_cast<float>(game.window.getSize().y);
    const float cx = w / 2.0f;

    sf::Text title(FontManager::get(), "Custom Game Rules", 36);
    title.setFillColor(sf::Color(255, 215, 0));
    {
        sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    }
    title.setPosition(sf::Vector2f(cx, h * 0.12f));
    game.window.draw(title);

    const auto& rows = gameRuleRows();
    const float startY = h * 0.30f;
    const float spacing = 46.f;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        bool selected = (static_cast<int>(i) == game.gameRulesSelectedRow);
        std::string lineText = std::string(selected ? "> " : "  ") + rows[i].label + "   " + gameRuleDescription(game.gameRules_, rows[i].id);
        sf::Text line(FontManager::get(), lineText, 24);
        line.setFillColor(selected ? sf::Color(255, 255, 255) : sf::Color(200, 200, 200));
        line.setPosition(sf::Vector2f(cx, startY + i * spacing));
        game.window.draw(line);
    }

    sf::Text instr(FontManager::get(), "Up/Down: select   Left/Right: change   Esc: done", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    {
        sf::FloatRect ib = instr.getLocalBounds();
        instr.setOrigin(sf::Vector2f(ib.size.x / 2.0f, 0));
    }
    instr.setPosition(sf::Vector2f(cx, h * 0.78f));
    game.window.draw(instr);
}

void GameRenderer::drawPauseMenu(Game& game)
{
    float w = static_cast<float>(game.window.getSize().x);
    float h = static_cast<float>(game.window.getSize().y);
    float cx = w / 2.0f;

    sf::RectangleShape overlay(sf::Vector2f(w, h));
    overlay.setFillColor(sf::Color(0, 0, 0, 128));
    game.window.draw(overlay);

    sf::Text title(FontManager::get(), "PAUSED", 36);
    title.setFillColor(sf::Color(255, 215, 0));
    sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.size.x / 2.0f, 0));
    title.setPosition(sf::Vector2f(cx, h * 0.25f));
    game.window.draw(title);

    int hoveredIndex = getHoveredPauseMenuItemIndex(game);
    for (int i = 0; i < static_cast<int>(pauseMenuItems().size()); ++i)
    {
        const PauseMenuItem& item = pauseMenuItems()[i];
        bool selected = (game.pauseSelection == item.option);
        bool hovered = (i == hoveredIndex);

        drawMenuItem(game, item.label, selected, pauseRowY(game, i), hovered);
    }

    sf::Text instr(FontManager::get(), "Up/Down: navigate  Enter: select  Esc: resume", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    sf::FloatRect ib = instr.getLocalBounds();
    instr.setOrigin(sf::Vector2f(ib.size.x / 2.0f, 0));
    instr.setPosition(sf::Vector2f(cx, h * 0.68f));
    game.window.draw(instr);
}
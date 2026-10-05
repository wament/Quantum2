#include <cmath>
#include "Game.hpp"
#include "GameRenderer.hpp"
#include "Game.hpp"
#include "Constants.hpp"
#include "Graphics.hpp"
#include "FontManager.hpp"

void GameRenderer::draw(Game &game)
{
    game.window.clear();
    game.window.draw(*game.backgroundSprite);

    switch (game.appMode)
    {
    case AppMode::MainMenu:
        drawMainMenu(game);
        break;
    case AppMode::Playing:
        drawGameTable(game);
        if (game.roundState == RoundState::OfferInsurance)
            drawInsurance(game);

        game.updateHUDState();
        game.hud.update(game.hudState);
        game.hud.draw(game.window);
        drawSummaryOverlay(game);
        break;
    case AppMode::Paused:
        drawGameTable(game);
        game.updateHUDState();
        game.hud.update(game.hudState);
        game.hud.draw(game.window);
        drawPauseMenu(game);
        break;
    case AppMode::ReplayHistory:
    case AppMode::ReplayPlaying:
        game.replay->draw();
        break;
    }

    game.window.display();
}

// Phase R1a: shared table-draw for Playing and Paused states (dealer + players + chips).
void GameRenderer::drawGameTable(Game &game)
{
    drawDealer(game);
    drawPlayers(game);
    drawChips(game);
}

void GameRenderer::resizeView(sf::RenderWindow &window, sf::View &view)
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

void GameRenderer::drawDealer(Game &game)
{
    if (game.roundState == RoundState::PlayerTurn || game.roundState == RoundState::OfferInsurance)
    {
        auto back = Graphics::makeCardBackSprite(game.cardTexture);
        back.setPosition(sf::Vector2f(Constants::DEALER_X, Constants::DEALER_Y));
        game.window.draw(back);

        auto up = Graphics::makeCardSprite(game.dealer.getCard(0), game.cardTexture);
        up.setPosition(sf::Vector2f(Constants::DEALER_X - 10, Constants::DEALER_Y));
        game.window.draw(up);
        return;
    }

    for (std::size_t i = 0; i < game.dealer.size(); i++)
    {
        Graphics::drawCard(game.window, game.cardTexture, game.dealer.getCard(i),
                          Constants::DEALER_X + i * 60, Constants::DEALER_Y);
    }
}

sf::Vector2f GameRenderer::getPlayerPosition(Game &game, size_t playerIndex, size_t totalPlayers) const
{
    if (totalPlayers <= 2)
    {
        return {playerIndex * 200.f, 0.f};
    }

     // Manual per-player offsets for fine-tuning placement
    static const sf::Vector2f playerOffsets[6] = {
        {-21.f, -1.f},  // Player 0
        {-3.f, -9.f},    // Player 1
        {-1.f, 0.f},    // Player 2
        {8.f, 0.f},    // Player 3
        {7.f, -3.f},    // Player 4
        {6.f, 7.f}     // Player 5
    };
    // Arrange players in an arc around the table
    float centerX = Constants::WINDOW_WIDTH / 2.f - 12;
    float centerY = 700.f;
    float radius = 400.f;

    // Arc span increases with player count: 75° for 3 players, 120° for 6 players
    float arcSpan = 60.f + (totalPlayers - 2) * 15.f;
    float startAngle = -arcSpan / 2.f;
    float angleStep = arcSpan / (totalPlayers - 1);

    float angle = startAngle + playerIndex * angleStep;
    float angleRad = angle * 3.14159f / 180.f;

    float x = centerX + radius * sin(angleRad);
    float y = centerY - radius * cosh(0.78*angleRad);

    sf::Vector2f offset = (playerIndex < 6) ? playerOffsets[playerIndex] : sf::Vector2f(0.f, 0.f);
    return {x - Constants::PLAYER_X + offset.x, y - Constants::PLAYER_Y + offset.y};
}

void GameRenderer::drawPlayers(Game &game)
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
            sf::RectangleShape highlight(sf::Vector2f(120.f, 260.f));
            highlight.setFillColor(sf::Color(255, 255, 0, 30));
            highlight.setOutlineColor(sf::Color(255, 255, 0, 150));
            highlight.setOutlineThickness(2.f);
            highlight.setPosition(sf::Vector2f(Constants::PLAYER_X + xOffset - 30.f, Constants::PLAYER_Y + yOffset - 40.f));
            game.window.draw(highlight);

            sf::Text turnLabel(FontManager::get(), "YOUR TURN", 14.f);
            turnLabel.setFillColor(sf::Color(255, 255, 0));
            turnLabel.setPosition(sf::Vector2f(Constants::PLAYER_X + xOffset - 10, Constants::PLAYER_Y + yOffset + 180.f));
            game.window.draw(turnLabel);
        }

        for (std::size_t handIndex = 0; handIndex < p.hands.size(); handIndex++)
        {
            const Hand &hand = p.hands[handIndex];
            float handX = Constants::PLAYER_X + xOffset + handIndex * 70;
            float handY = Constants::PLAYER_Y + yOffset;

            // Draw cards for this hand
            for (std::size_t i = 0; i < hand.size(); i++)
            {
                float cardX = handX + i * 16;
                float cardY = handY - i * 16;

                // Only rotate the third card (double down card) 90 degrees
                if (hand.isDoubledDown() && i == 2)
                {
                    auto sprite = Graphics::makeCardSprite(hand.getCard(i), game.cardTexture);
                    sprite.setPosition(sf::Vector2f(cardX, cardY));
                    sprite.setOrigin(sf::Vector2f(Constants::CARD_WIDTH / 2.f, Constants::CARD_HEIGHT / 2.f));
                    sprite.move(sf::Vector2f(Constants::CARD_WIDTH / 2.f, Constants::CARD_HEIGHT / 2.f));
                    sprite.rotate(sf::degrees(90.f));
                    game.window.draw(sprite);
                }
                else
                {
                    Graphics::drawCard(game.window, game.cardTexture, hand.getCard(i), cardX, cardY);
                }
            }
            
            // Draw BUSTED label on top of busted hands
            if (hand.isBust())
            {
                sf::Text bustedText(FontManager::get(), "BUSTED", 18.f);
                bustedText.setFillColor(sf::Color(255, 0, 0));
                bustedText.setStyle(sf::Text::Bold);
                bustedText.setPosition(sf::Vector2f(handX, handY + 5.f));
                game.window.draw(bustedText);
            }

            // Draw white flag for surrendered hands
            if (hand.isSurrendered())
            {
                sf::RectangleShape pole(sf::Vector2f(3.f, 30.f));
                pole.setFillColor(sf::Color(128, 128, 128));
                pole.setPosition(sf::Vector2f(handX + 10, handY - 40));

                sf::RectangleShape flag(sf::Vector2f(20.f, 12.f));
                flag.setFillColor(sf::Color::White);
                flag.setOutlineColor(sf::Color::Black);
                flag.setOutlineThickness(1.f);
                flag.setPosition(sf::Vector2f(handX + 13, handY - 40));

                game.window.draw(pole);
                game.window.draw(flag);
            }
        }
    }
}

void GameRenderer::drawChips(Game &game)
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
        {100, sf::Color::White}
    };

    for (size_t playerIndex = 0; playerIndex < game.players.size(); playerIndex++)
    {
        auto& p = game.players[playerIndex];
        sf::Vector2f pos = getPlayerPosition(game, playerIndex, game.players.size());
        float xOffset = pos.x;
        float yOffset = pos.y;

        int handIndex = 0;
        for (Hand &hand : p.hands)
        {
            int totalBet = hand.getBet();
            int stackBet = hand.isDoubledDown() ? totalBet / 2 : totalBet;

            if (totalBet <= 0)
            {
                handIndex++;
                continue;
            }
 
            float chipX = Constants::PLAYER_X + xOffset + handIndex * 80 + 24;
            float chipY = Constants::PLAYER_Y + yOffset + Constants::CARD_HEIGHT + 45.f;

            int remaining = stackBet * 100;
            std::vector<Chip> stack;

            for (const auto &chip : denominations)
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

void GameRenderer::drawDeck(Game &game)
{
    for (std::size_t i = 0; i < game.deck.size(); i++)
    {
        Graphics::drawCard(game.window, game.cardTexture, game.deck.getCard(i),
                          Constants::DECK_X + (i % 52) * 20,
                          Constants::DECK_Y + (i / 52) * 80);
    }
}

void GameRenderer::drawInsurance(Game &game)
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

void GameRenderer::drawSummaryOverlay(Game &game)
{
    if (game.hudState.playerSummaries.empty() || game.hudState.summaryTimer <= 0.0f)
        return;

    // Calculate alpha based on timer (fade out over 6 seconds)
    float alpha = static_cast<float>(std::min(255.0f, game.hudState.summaryTimer * 42.5f));
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

void GameRenderer::drawMenuItem(Game &game, const std::string &label, bool selected, float y, bool hovered)
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

int GameRenderer::getHoveredMenuItemIndex(Game &game) const
{
    float cx = static_cast<float>(game.window.getSize().x) / 2.0f;
    sf::Vector2f mousePos = game.window.mapPixelToCoords(sf::Mouse::getPosition(game.window));

    int hoveredIndex = -1;
    const float rowHalfHeight = 30.f; // comfortably covers a size-24 menu line
    float bestDistance = rowHalfHeight;
    for (int i = 0; i < static_cast<int>(mainMenuItems().size()); ++i)
    {
        const MainMenuItem &item = mainMenuItems()[i];
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

// Vertical pixel-centre of pause-menu row `index` at the current window height. Uses the
// fractional positions from pauseMenuItems() so it matches drawPauseMenu and the click/hover
// hit-testing exactly -- one source of truth for where those rows sit.
float GameRenderer::pauseRowY(Game &game, int index) const
{
    if (index < 0 || index >= static_cast<int>(pauseMenuItems().size()))
        return -1.f;

    float height = game.window.getSize().y;
    return height * pauseMenuItems()[index].yFrac;
}

// Index of the pause-menu row currently under the mouse cursor, or -1 if none. Nearest-centre
// within each row's half-height and centred horizontally around the screen centre -- used by both
// hover highlighting (drawPauseMenu) and click selection (processPauseEvents) so they agree.
int GameRenderer::getHoveredPauseMenuItemIndex(Game &game) const
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

void GameRenderer::drawMainMenu(Game &game)
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
        const MainMenuItem &item = mainMenuItems()[i];
        std::string label;
        switch (item.option)
        {
        case MenuOption::PlayGame:
            label = "Play Game";
            break;
        case MenuOption::PlayerCount:
            label = "Players: " + std::to_string(game.playerCount) + " (Left/Right)";
            break;
        case MenuOption::GameSpeed:
        {
            std::string speedLabel = "Speed: ";
            switch (game.gameSpeed)
            {
            case GameSpeed::Fast:      speedLabel += "Fast"; break;
            case GameSpeed::Normal:    speedLabel += "Normal"; break;
            case GameSpeed::Slow:      speedLabel += "Slow"; break;
            case GameSpeed::AutoPlay:  speedLabel += "AutoPlay"; break;
            }
            speedLabel += " (Left/Right)";
            label = speedLabel;
            break;
        }
        case MenuOption::ReviewHistory:
            label = "Review History";
            break;
        }

        bool hovered = (i == hoveredIndex);
        drawMenuItem(game, label, game.menuSelection == item.option, item.y, hovered);
    }

    sf::Text instr(FontManager::get(), "Up/Down: navigate  Enter: select  Esc: quit", 14);
    instr.setFillColor(sf::Color(140, 140, 140));
    instr.setPosition(sf::Vector2f(cx, 400.0f));
    game.window.draw(instr);
}

void GameRenderer::drawPauseMenu(Game &game)
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
        const PauseMenuItem &item = pauseMenuItems()[i];
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
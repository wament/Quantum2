#include "StrategyTrainer.hpp"
#include "BasicStrategy.hpp"
#include "Constants.hpp"
#include "FontManager.hpp"
#include "Graphics.hpp"   // Graphics::makeCardSprite renders the real cards on screen (shared tilemap from game.cardTexture).

#include <algorithm>

// Draw a string centred horizontally at (cx, y) using the shared font. Centralises the origin/position boilerplate
// label in draw() lines up consistently and draw() stays readable.
void centeredText(sf::RenderWindow& window, const std::string& text, unsigned size, sf::Color color, float cx, float y)
{
    sf::Text t(FontManager::get(), text, size);
    t.setFillColor(color);
    const auto b = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
    t.setPosition(sf::Vector2f(cx, y));
    window.draw(t);
}

using namespace BasicStrategy;

namespace
{
    // Faithful port of Game::giveBasicStrategyAdvice's inputs to the shared advisor primitive. Returns one of
    // H/D/S/P/R (soft 'Y'->H and pair 'M' resolve inside getRecommendedAction)
    // is exactly equivalent to what the in-game Book box shows.
    char recommendAction(const Hand& playerHand, const Hand& dealerHand)
    {
        int playerValue = playerHand.getValue();
        int dealerUpValue = dealerHand.getDealerUpCard().getBlackjackValue();
        bool hasAce = playerHand.isSoft();
        bool isPair = playerHand.size() == 2 &&
                      playerHand.getCard(0).getBlackjackValue() == playerHand.getCard(1).getBlackjackValue();
        int pairValue = isPair ? playerHand.getCard(0).getBlackjackValue() : 0;
        return getRecommendedAction(playerValue, dealerUpValue, hasAce, isPair, pairValue);
    }

    // The game's surrender key is 'Y'; basic strategy reports surrender as 'R'. Both must grade identically.
    char normalizeInput(char chosen)
    {
        if (chosen == 'y' || chosen == 'Y')
            return 'R';
        return chosen;
    }

    const char* actionName(char action)
    {
        switch (action)
        {
        case 'H': return "Hit";
        case 'D': return "Double Down";
        case 'S': return "Stand";
        case 'P': return "Split";
        case 'R': return "Surrender";
        default:  return "?";
        }
    }
}

StrategyTrainer::StrategyTrainer() = default;

void StrategyTrainer::startSession(int deckCount_, int secondsPerSession_)
{
    deckCount = deckCount_;
    // Rebuild a fresh shoe and shuffle it. Deck's own RNG drives the deal order (reseeded from the OS each shuffle),
    // so auto-generated questions are unpredictable; the trainer only needs to grade whatever the shoe deals.
    deck.create(deckCount);
    deck.shuffle();

    stats = SessionStats{};
    elapsedSeconds = 0.0f;
    timed = secondsPerSession_ > 0;
    secondsPerSession = secondsPerSession_;
    active = true;
    complete = false;
    lastVerdict.clear();

    nextQuestion(); // present the first hand immediately so the screen is never blank on entry
}

char StrategyTrainer::loadQuestion(const Hand& playerHand, const Card& dealerUpCard)
{
    questionHand.clear();
    for (std::size_t i = 0; i < playerHand.size(); ++i)
        questionHand.addCard(playerHand.getCard(i));
    questionDealer.clear();
    questionDealer.addCard(dealerUpCard);

    computeBookAction();
    return currentCorrect;
}

bool StrategyTrainer::submitAction(char chosen)
{
    if (!active || complete)
        return false; // no live question to grade

    char answer = normalizeInput(chosen);
    bool matched = (answer == currentCorrect);

    lastVerdict = matched ? ("Correct! " + std::string(1, currentCorrect) + " - " + actionName(currentCorrect))
                          : ("Incorrect. The book says " + std::string(1, currentCorrect) + " - " + actionName(currentCorrect));

    stats.totalQuestions++;
    if (matched)
    {
        stats.correctAnswers++;
        stats.currentStreak++;
        stats.bestStreak = std::max(stats.bestStreak, stats.currentStreak);
    }
    else
    {
        stats.currentStreak = 0;
    }

    nextQuestion(); // advance to the next hand automatically after every graded answer
    return matched;
}

void StrategyTrainer::nextQuestion()
{
    if (!active)
        return;

    // A question needs two player cards plus the dealer's up-card. Fewer than three left means the shoe is spent.
    if (deck.size() < 3)
    {
        active = false;
        complete = !timed; // an untimed session ends on exhaustion; a timed one keeps running until its timer elapses
        if (lastVerdict.empty())
            lastVerdict = "Shoe finished - press Enter for a new session.";
        return;
    }

    // A natural blackjack (2 cards = 21) needs no decision
    // Rejected hands are simply removed from this training shoe (it isn't a saved record)
    int attempts = 0;
    do
    {
        questionHand.clear();
        questionHand.addCard(deck.draw());
        questionHand.addCard(deck.draw());

        questionDealer.clear();
        questionDealer.addCard(deck.draw());
        ++attempts;
    } while (questionHand.isBlackjack() && attempts < 500 && deck.size() >= 3);

    computeBookAction();
}

void StrategyTrainer::update(float deltaTime)
{
    if (!active || !timed)
        return;

    elapsedSeconds += deltaTime;
    if (elapsedSeconds >= static_cast<float>(secondsPerSession))
    {
        active = false;
        complete = true;
        lastVerdict = "Time's up! Session finished - press Enter for a new session.";
    }
}

void StrategyTrainer::computeBookAction()
{
    currentCorrect = recommendAction(questionHand, questionDealer);
}

void StrategyTrainer::draw(sf::RenderWindow& window, const sf::Texture& cardTexture) const
{
    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const float cx = w / 2.0f;

    centeredText(window, "STRATEGY TRAINER", 34, sf::Color(255, 215, 0), cx, h * 0.09f);
    centeredText(window, "Call the play basic strategy prescribes.", 15, sf::Color(200, 200, 200), cx, h * 0.09f + 40.f);

    if (!active)
    {
        std::string prompt = lastVerdict.empty() ? "Press Enter to start." : lastVerdict;
        centeredText(window, prompt, 20, sf::Color(235, 235, 235), cx, h * 0.46f);
        centeredText(window, "Enter: start / restart    Esc: back to menu", 14, sf::Color(150, 150, 150), cx, h * 0.46f + 38.f);
        return;
    }

    const float cardEdge = static_cast<float>(Constants::CARD_WIDTH) * 2.f;
    const float cardsMidY = h * 0.39f;

    auto drawCardAt = [&](const Card& card, float px, float py) {
        sf::Sprite sprite(Graphics::makeCardSprite(card, cardTexture));
        const auto r = sprite.getTextureRect();
        sprite.setOrigin(sf::Vector2f(r.size.x / 2.f, r.size.y / 2.f));
        sprite.setScale(sf::Vector2f(2.f, 2.f));
        sprite.setPosition(sf::Vector2f(px, py));
        window.draw(sprite);
    };

    const int playerCount = static_cast<int>(questionHand.size());
    const float gap = 28.f;
    const float playerTotalW = playerCount > 0 ?
        static_cast<float>(playerCount) * cardEdge + static_cast<float>(playerCount - 1) * gap : 0.f;
    const float playerCx = cx - cardEdge * 1.5f;
    for (int i = 0; i < playerCount; ++i)
    {
        const float px = playerCx - playerTotalW / 2.f + i * (cardEdge + gap) + cardEdge / 2.f;
        drawCardAt(questionHand.getCard(i), px, cardsMidY);
    }

    const float dealerCx = cx + cardEdge * 1.5f;
    if (questionDealer.size() > 0)
        drawCardAt(questionDealer.getCard(0), dealerCx, cardsMidY);

    centeredText(window, "Your hand - " + std::to_string(questionHand.getValue()),
                 18, sf::Color(230, 230, 230), playerCx, cardsMidY + cardEdge / 2.f + 30.f);
    if (questionDealer.size() > 0)
        centeredText(window, "Dealer shows - " + questionDealer.getCard(0).getBlackjackValueToString(),
                     18, sf::Color(255, 215, 0), dealerCx, cardsMidY + cardEdge / 2.f + 30.f);

    std::string statsText = "Accuracy " + std::to_string(static_cast<int>(stats.accuracyPercent() + 0.5f)) + "%" +
                            "   Correct " + std::to_string(stats.correctAnswers) + "/" +
                            std::to_string(stats.totalQuestions) + "   Streak " + std::to_string(stats.currentStreak);
    centeredText(window, statsText, 16, sf::Color(220, 220, 220), cx, h * 0.565f);

    if (complete)
    {
        std::string summary = "Session over - Accuracy " + std::to_string(static_cast<int>(stats.accuracyPercent() + 0.5f)) + "%" +
                              "   Best streak " + std::to_string(stats.bestStreak);
        centeredText(window, summary, 24, sf::Color(255, 215, 0), cx, h * 0.50f);
        centeredText(window, "Press Enter for a new session", 15, sf::Color(150, 150, 150), cx, h * 0.50f + 40.f);
        return;
    }

    const float buttonY = h * 0.68f;
    const auto rects = buttonRects(w, h);
    int hovered = -1;
    const sf::Vector2f mouse(sf::Mouse::getPosition(window));
    for (int i = 0; i < 5; ++i)
        if (mouse.x >= rects[i].position.x && mouse.x <= rects[i].position.x + rects[i].size.x &&
            mouse.y >= rects[i].position.y && mouse.y <= rects[i].position.y + rects[i].size.y)
        {
            hovered = i;
            break;
        }

    const std::vector<Action>& actions = buttonActions();
    for (int i = 0; i < 5; ++i)
    {
        const auto& rect = rects[i];
        sf::RectangleShape bg(sf::Vector2f(rect.size.x, rect.size.y));
        bg.setPosition(sf::Vector2f(rect.position.x, rect.position.y));
        bg.setFillColor(i == hovered ? sf::Color(70, 70, 100) : sf::Color(45, 45, 62));
        window.draw(bg);
        if (i == hovered)
        {
            sf::RectangleShape frame(sf::Vector2f(rect.size.x + 4.f, rect.size.y + 4.f));
            frame.setPosition(sf::Vector2f(rect.position.x - 2.f, rect.position.y - 2.f));
            frame.setFillColor(sf::Color(255, 215, 0));
            window.draw(frame);
        }
        centeredText(window, actionName(static_cast<char>(actions[i])),
                     20, sf::Color(255, 255, 255), rect.position.x + rect.size.x / 2.f, rect.position.y + rect.size.y / 2.f);
    }

    const float keyHintY = buttonY + 46.f;
    centeredText(window, "Keyboard:   H=Hit   D=Double Down   S=Stand   P=Split   Y or R=Surrender",
                 13, sf::Color(150, 150, 150), cx, keyHintY);

    if (!lastVerdict.empty())
        centeredText(window, lastVerdict, 18, sf::Color(255, 235, 130), cx, keyHintY + 28.f);

    if (!timed)
        centeredText(window, "Cards left: " + std::to_string(remainingCardsForTest()),
                     12, sf::Color(120, 120, 120), cx, h - 24.f);
}

std::vector<StrategyTrainer::Action> StrategyTrainer::buttonActions()
{
    return { Action::Hit, Action::DoubleDown, Action::Stand, Action::Split, Action::Surrender };
}

std::vector<sf::Rect<float>> StrategyTrainer::buttonRects(float w, float h) const
{
    std::vector<sf::Rect<float>> rects;
    const float cx = w / 2.0f;
    const float buttonH = 56.f;
    const float gap = 16.f;
    const float availW = w * 0.84f;
    const float btnW = (availW - 4.f * gap) / 5.f;
    const float totalW = btnW * 5.f + 4.f * gap;
    const float startX = cx - totalW / 2.f;
    const float buttonY = h * 0.68f;
    for (int i = 0; i < 5; ++i)
        rects.emplace_back(sf::Rect<float>(sf::Vector2f(startX + i * (btnW + gap), buttonY - buttonH / 2.f), sf::Vector2f(btnW, buttonH)));
    return rects;
}

int StrategyTrainer::buttonIndexAt(float x, float y, float w, float h) const
{
    const auto rects = buttonRects(w, h);
    for (int i = 0; i < static_cast<int>(rects.size()); ++i)
    {
        const float left = rects[i].position.x;
        const float top = rects[i].position.y;
        const float right = left + rects[i].size.x;
        const float bottom = top + rects[i].size.y;
        if (x >= left && x <= right && y >= top && y <= bottom)
            return i;
    }
    return -1;
}

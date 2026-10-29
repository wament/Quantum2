#include "StrategyTrainer.hpp"
#include "BasicStrategy.hpp"
#include "Constants.hpp"
#include "FontManager.hpp"

#include <algorithm>

using namespace BasicStrategy;

namespace
{
    // Faithful port of Game::giveBasicStrategyAdvice's inputs to the shared advisor primitive. Returns one of
    // H/D/S/P/R (soft 'Y'->H and pair 'M' resolve inside getRecommendedAction), so grading a player key against it
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

    // A natural blackjack (2 cards = 21) needs no decision, so keep dealing until the player's hand is a real choice.
    // Rejected hands are simply removed from this training shoe (it isn't a saved record), which keeps bounds simple.
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

void StrategyTrainer::draw(sf::RenderWindow& window) const
{
    const float cx = static_cast<float>(window.getSize().x) / 2.0f;
    const float w = static_cast<float>(window.getSize().x);

    // Title + mode hint.
    {
        sf::Text t(FontManager::get(), "STRATEGY TRAINER", 34);
        t.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 60.f));
        window.draw(t);
    }
    {
        sf::Text t(FontManager::get(), "Call the play basic strategy prescribes.", 14);
        t.setFillColor(sf::Color(180, 180, 180));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 92.f));
        window.draw(t);
    }

    if (!active)
    {
        // Session not running yet (fresh entry or finished): prompt to start.
        std::string prompt = lastVerdict.empty() ? "Press Enter to start." : lastVerdict;
        sf::Text t(FontManager::get(), prompt, 18);
        t.setFillColor(sf::Color(230, 230, 230));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 220.f));
        window.draw(t);
        sf::Text i(FontManager::get(), "Enter: start / restart    Esc: back to menu", 14);
        i.setFillColor(sf::Color(140, 140, 140));
        sf::FloatRect bi = i.getLocalBounds();
        i.setOrigin(sf::Vector2f(bi.size.x / 2.0f, bi.size.y / 2.0f));
        i.setPosition(sf::Vector2f(cx, 260.f));
        window.draw(i);
        return;
    }

    // The current question: player hand total + dealer up-card.
    {
        sf::Text t(FontManager::get(), "Your hand", 18);
        t.setFillColor(sf::Color(230, 230, 230));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 150.f));
        window.draw(t);
    }

    std::string cardLabel;
    for (std::size_t i = 0; i < questionHand.size(); ++i)
        cardLabel += questionHand.getCard(i).getBlackjackValueToString() + " ";
    if (cardLabel.empty())
        cardLabel = "?";
    {
        sf::Text t(FontManager::get(), cardLabel, 26);
        t.setFillColor(sf::Color(255, 255, 255));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx - 90.f, 185.f));
        window.draw(t);
    }

    std::string dealerLabel = questionDealer.size() ? questionDealer.getCard(0).getBlackjackValueToString() : "?";
    {
        sf::Text t(FontManager::get(), "Deal up: " + dealerLabel, 22);
        t.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx + 90.f, 185.f));
        window.draw(t);
    }

    // Action-key legend.
    {
        sf::Text t(FontManager::get(), "[H]it   [D]ouble   [S]tand   [P]plit   [Y/R]ender", 16);
        t.setFillColor(sf::Color(200, 200, 200));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 235.f));
        window.draw(t);
    }

    // Stats bar.
    {
        std::string statsText = "Accuracy " + std::to_string(static_cast<int>(stats.accuracyPercent() + 0.5f)) + "%" +
                                "   Correct " + std::to_string(stats.correctAnswers) + "/" +
                                std::to_string(stats.totalQuestions) + "   Streak " + std::to_string(stats.currentStreak);
        sf::Text t(FontManager::get(), statsText, 16);
        t.setFillColor(sf::Color(220, 220, 220));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 270.f));
        window.draw(t);
    }

    // The verdict from the last graded answer (empty until the first answer comes in).
    if (!lastVerdict.empty())
    {
        sf::Text t(FontManager::get(), lastVerdict, 18);
        t.setFillColor(sf::Color(255, 235, 130));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 305.f));
        window.draw(t);
    }

    // Session summary once it has finished.
    if (complete)
    {
        std::string summary = "Session over - Accuracy " + std::to_string(static_cast<int>(stats.accuracyPercent() + 0.5f)) + "%" +
                              "   Best streak " + std::to_string(stats.bestStreak);
        sf::Text t(FontManager::get(), summary, 20);
        t.setFillColor(sf::Color(255, 215, 0));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f(b.size.x / 2.0f, b.size.y / 2.0f));
        t.setPosition(sf::Vector2f(cx, 360.f));
        window.draw(t);
        sf::Text i(FontManager::get(), "Press Enter for a new session", 14);
        i.setFillColor(sf::Color(140, 140, 140));
        sf::FloatRect bi = i.getLocalBounds();
        i.setOrigin(sf::Vector2f(bi.size.x / 2.0f, bi.size.y / 2.0f));
        i.setPosition(sf::Vector2f(cx, 392.f));
        window.draw(i);
    }

    // A small corner note so the player knows how many questions remain in an untimed shoe.
    if (!timed)
    {
        sf::Text t(FontManager::get(), "Cards left: " + std::to_string(remainingCardsForTest()), 12);
        t.setFillColor(sf::Color(120, 120, 120));
        window.draw(t);
    }
}

#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <deque>

#include "GameRecord.hpp"
#include "Hand.hpp"
#include "Card.hpp"

// Forward declaration so the extracted view layer can be granted friendship without a circular include.
class ReplayView;

namespace ReplayInternals
{

    // A single navigable step in the replay timeline.
    // Each step corresponds to one discrete action or card dealt.
    struct ReplayStep
    {
        enum class Type
        {
            DealCard,          // Any card dealt to player or dealer
            PlayerDecision,    // Hit/Stand/Double/Split/Surrender
            InsuranceOffer,    // Insurance was offered (ace up)
            InsuranceDecision, // Insurance taken or declined
            DealerDraw,        // Dealer draws a card
            RoundEnd           // Round evaluation complete
        };

        Type type = Type::DealCard;
        int roundIndex = 0; // Which round this step belongs to
        int handIndex = 0;  // Which player hand (for DealCard/PlayerDecision)

        Card card; // The card dealt (for DealCard/DealerDraw)
        bool isDealerCard = false;

        std::string action; // "H"/"S"/"D"/"P"/"R" for PlayerDecision,
                            // "I"/"N" for insurance decision
        bool insuranceTaken = false;
        int insuranceAmount = 0;

        // Count and state at this step
        int runningCount = 0;
        float trueCount = 0.f;
        int cardsRemaining = 0;

        // Cumulative shoe profit up to this step
        int cumulativeProfit = 0;

        // Whether dealer's hole card is visible at this step
        bool dealerHoleCardVisible = false;
    };

    struct ReplayState
    {
        // Dealer's hand
        Hand dealerHand;
        bool dealerHoleCardVisible = false;

        // One entry per player hand being played
        struct HandSnapshot
        {
            Hand cards; // Use the real Hand class

            // Keep these as snapshot metadata (Hand doesn't own these)
            int bet = 0;
            bool isFinished = false;
            bool isSplit = false;
            bool isDoubledDown = false;
            bool isSurrendered = false;
            std::vector<std::string> actionsTaken;
            std::string playerName = "";  // seat identity (William or an NPC name)

            // Computed properties — derive them from Hand on demand, or cache lazily:
            bool isBlackjack() const { return cards.isBlackjack(); }
            bool isBust() const { return cards.isBust(); }
            int getValue() const { return cards.getValue(); }
        };
        std::vector<HandSnapshot> playerHands;
        int currentHandIndex = 0;

        // Round-level info
        int roundIndex = 0;
        int runningCount = 0;
        float trueCount = 0.f;
        int cardsRemaining = 0;

        // Profit tracking
        int roundProfit = 0; // Net for this round so far
        int shoeProfit = 0;  // Net for entire shoe so far

        // Insurance state (for the first hand in the round)
        int insuranceBet = 0;
        bool insuranceActive = false;

        // Step info for slider/overlays
        int currentStepIndex = 0;
        int totalSteps = 0;
        int totalRounds = 0;
        bool isLastRound = false;
        bool shoeCompleted = false;
    };

}

// One entry in the history list (selectable shoe).
struct ReplayHistoryEntry
{
    int index = 0;
    int roundCount = 0;
    int cardsDealt = 0;
    int netProfit = 0;
    bool completed = false;
    long long timestamp = 0;
    int startingCount = 0; // runningCount carried from previous shoe
};

class Replay
{
public:
    // Current mode of the replay system
    enum class Mode
    {
        HistoryList, // User is selecting a shoe from history
        Playing      // User is actively replaying a shoe
    };

    explicit Replay(sf::RenderWindow &window,
                    sf::Texture &cardTexture,
                    sf::Font &font);

    // Load all shoe records from the player's history
    void loadHistory(const std::vector<ShoeRecord> &shoes);

    // Start replaying a specific shoe from history
    void loadShoe(const ShoeRecord &shoe, int shoeIndex);

    // Exit replay and return to live game
    void exitReplay();

    // --- Navigation ---
    void stepForward();
    void stepBackward();
    void goToRound(int roundIndex);
    void skipHand();
    void skipRound();
    void setAutoPlay(bool enable, float stepsPerSecond = 2.0f);
    void setStartingBankroll(int bankroll);

    // --- History list navigation ---
    void selectPreviousShoe();
    void selectNextShoe();
    // Index of the history-list row under `mousePos` (window pixels), or -1 if none.
    // Shared by drawHistoryList (hover highlight) and processEvents (click selection) so
    // they agree on which shoe is under the cursor.
    int hoveredHistoryRow(sf::Vector2i mousePos) const;

    // --- Queries ---
    Mode getMode() const;
    const ReplayInternals::ReplayState &getCurrentState() const;
    const std::vector<ReplayHistoryEntry> &getHistoryEntries() const;
    int getSelectedHistoryIndex() const;
    bool isAtStart() const;
    bool isAtEnd() const;

    // --- Core loop ---
    void processEvents(sf::Event &event);
    void update(float deltaTime);
    void draw();

    // Allow the extracted view layer to read this class's private step/state data for rendering.
    friend class ReplayView;

private:
    // Parse ShoeRecord into an ordered sequence of ReplaySteps
    std::vector<ReplayInternals::ReplayStep> buildSteps(const ShoeRecord &shoe);

    // Reconstruct the visual ReplayState from steps[0..stepIndex]
    ReplayInternals::ReplayState buildStateAtStep(int stepIndex) const;

    // Navigation helpers
    int findRoundStartStep(int roundIndex) const;
    int findHandStartStep(int roundIndex, int handIndex) const;
    int findRoundEndStep(int roundIndex) const;
    int findNextDecisionStep(int fromStepIndex) const;

    // Rendering helpers
    void drawHistoryList();

    // --- History list scrolling ---
    // Number of rows currently fitting above the window bottom edge (rows start at y=120, 40px tall).
    int historyVisibleRowCount() const;
    // Largest scroll offset that still leaves at least one shoe row visible in the list.
    int maxHistoryScrollOffset() const;
    // After arrow-key navigation, clamp the scroll offset so the selected row stays within view.
    void adjustHistorySelectionVisibility();
    void drawReplayView();
    void drawTable(const ReplayInternals::ReplayState &state);
    void drawControls(const ReplayInternals::ReplayState &state);
    void drawAnalysisOverlay(const ReplayInternals::ReplayState &state);
    void drawRoundSlider(const ReplayInternals::ReplayState &state);
    void drawStepSummary(const ReplayInternals::ReplayState &state);

    // Basic strategy lookup (mirrors Game::giveBasicStrategyAdvice)
    char getBasicStrategyAction(const ReplayInternals::ReplayState &state) const;

    // --- Resources (references from Game) ---
    sf::RenderWindow &window;
    sf::Texture &cardTexture;
    sf::Font &font;

    // --- History list ---
    std::vector<ShoeRecord> allShoes; // full shoe records for loading
    std::vector<ReplayHistoryEntry> historyEntries;
    int selectedHistoryIndex = 0;

    // Number of rows scrolled past the top so shoe entries below the visible area can still be hovered and clicked.
    int historyScrollOffset = 0;

    // Current cursor (window pixels), updated each frame so the hover highlight and the
    // click-selection path agree on which list row is under the mouse.
    sf::Vector2i replayCursorPos{0, 0};

    // --- Replay state ---
    Mode mode = Mode::HistoryList;
    ShoeRecord currentShoe;
    int currentShoeIndex = -1;

    std::vector<ReplayInternals::ReplayStep> steps;
    int currentStepIndex = 0;
    ReplayInternals::ReplayState currentState;

    // --- Auto-play ---
    bool autoPlay = false;
    float autoPlayInterval = 0.5f; // seconds per step (1 / stepsPerSecond)
    float autoPlayTimer = 0.0f;

    // --- Slider interaction ---
    bool sliderDragging = false;
    sf::FloatRect sliderBounds;

    // --- Round info cache (for slider hover) ---
    struct RoundInfo
    {
        int netProfit = 0;
        float maxTrueCount = 0.f;
        int handCount = 0;
    };
    std::vector<RoundInfo> roundInfos;

    // --- Bankroll for Kelly overlay ---
    // Set when loadShoe is called from Game (Game knows the real bankroll)
    int startingBankroll = 1000;
    int currentBankroll = 1000;
};
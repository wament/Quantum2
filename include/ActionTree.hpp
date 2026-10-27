#pragma once

// ---------------------------------------------------------------------------
// Action Tree Tool — headless action-tree analysis model + serializers.
//
// A "chunk" is one round of a shoe: its root node is the initial-deal state
// (every seat + dealer hold their two cards, dealt up front exactly like
// Game::deal()). The tree enumerates EVERY legal action at every node; the
// basic-strategy edge is flagged via TreeNode.isBasicStrategy; outcomes are
// grounded in the actual shoe's card order through a per-path deck cursor.
//
// The whole shoe is a forest of these per-round chunks. The model doubles as
// the single source of truth for a future visualizer; three backends read it
// today: flat pre-order text (primary save), ASCII preview, JSON export.
//
// Rules are MIRRORED from src/Game.cpp + src/RoundEngine.cpp (not driven):
// reuse Card/Hand/Constants + BasicStrategy::getRecommendedAction only.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "Card.hpp"

// Per-path working state, defined at namespace scope so both free helpers and member functions can
// name them (see §4/§6). HandInfo mirrors an active Hand's flag-bearing split / split-ace state.
struct HandInfo
{
    std::vector<Card> cards;
    bool split = false;
    bool splitAce = false;
};
struct HandEntry
{
    int seatIndex;
    std::vector<Card> cards;
    bool surrendered = false;   // §4: mirrors a Game hand's surrender flag so settlement matches real Game
};

enum class Outcome
{
    DealRoot,         // chunk root: the initial-deal state holding every seat's fixed starting pair
    HitContinue,      // drew a card and is still active (non-terminal internal node)
    Stand,            // stood; hand finished without drawing
    Bust,             // bust (>21) after a hit — terminal leaf for that hand
    TwentyOne,        // reached exactly 21 via hits (not a natural) — terminal leaf
    Blackjack,        // natural 2-card unsplit blackjack — terminal leaf
    Surrender,        // surrendered first two cards; half bet lost — terminal leaf
    DoubleEnd,        // doubled down: one forced card then done — terminal leaf
    SplitSiblingStart, // a split hand beginning its own decision subtree
    DealerResult      // terminal: dealer finished + all seats settled on this path
};

struct TreeNode
{
    int nodeId = 0;
    int seatIndex = 0;          // -1 for dealer nodes
    bool isDealerNode = false;

    char decision = 'D';        // edge taken to reach this node: D(deal-root)/H/S/D(double)/P/R/X(dealer)
    bool isBasicStrategy = false;   // true => this edge is the prescribed basic-strategy line (the label)

    std::vector<Card> cards;    // cards at this node (dealer: up + hole as known; then draws)
    int handValue = 0;
    bool isSoft = false;

    Outcome outcome = Outcome::HitContinue;
    // For terminal DealerResult leaves, per-seat settlement of THIS round:
    std::vector<int> seatResults;   // index by seatIndex in the chunk's seat list

    int deckCursorAtNode = 0;     // absolute position in the shoe sequence at this node
    std::vector<TreeNode> children;
};

struct SeatInfo
{
    int index = 0;
    std::string name;
    std::vector<Card> startCards;
};

struct Chunk
{
    int roundIndex = 0;
    int deckStartCursor = 0;      // tail position this chunk's initial deal reads from
    std::vector<SeatInfo> seats;  // seat identity + fixed starting pair (unchanged across branches)
    TreeNode root;                // DealRoot node holding all initial hands
};

class ActionTree
{
  public:
    // Build the analysis tool over an immutable shoe sequence. `seatCount` is the fixed number of
    // seats dealt each round for this run (default 1, mirroring the human seat); multi-seat runs share
    // one monotonically-advancing deck cursor per path (§6/§1.2).
    explicit ActionTree(std::vector<Card> shoeSequence, int seatCount = 1);

    std::vector<Chunk> buildShoeForest();

    std::string toFlatText() const;      // PRIMARY save format: flat pre-order records
    std::string toAscii(int maxDepth = -1) const;   // human-readable preview (-1 = full)
    std::string toJson() const;          // universal sharing / future viz input
    static ActionTree fromFlatText(const std::string&);

    struct Spine
    {
        int roundIndex = 0;
        std::vector<int> nodeIds;
    };
    std::vector<Spine> extractBasicStrategySpines() const;

    void summarize(std::size_t& outNodes, std::size_t& outLeaves, int& outMaxDepth) const;

  private:

    // ---- generators / rule mirror (§4/§6) ---------------------------------
    void buildRound(Chunk& chunk);
    void walkActiveSeat(TreeNode& node, int seatIndex, const Card& dealerUp, HandInfo hand,
                        int cursor, int handCount, std::vector<HandEntry>& pathHands,
                        int splitRemaining, int familyPairValue, Suit familySuit);
    void attachContinuation(TreeNode& node, int seatIndex, int cursor,
                            std::vector<HandEntry>& pathHands, int splitRemaining,
                            int totalHands, int familyPairValue, Suit familySuit);
    void buildFirstSplitHand(TreeNode& parentNode, int seatIndex, int cursor, int pairValue,
                             Suit suit, int totalHands, std::vector<HandEntry>& pathHands);
    void buildDealerBranch(TreeNode& terminalNode, int cursor,
                           const std::vector<HandEntry>& pathHands);
    // Legal-action enumeration (§4): canonical-ordered subset of {H,S,D,P,R}.
    std::vector<char> legalActions(const HandInfo& hand, int handCount) const;
    // §6 buildRound: play one round along the basic-strategy spine to seed the next chunk's cursor.
    int simulateSpineCards(int start);
    int playSeat(std::vector<HandInfo>& hands, int cursor);

    // ---- backends (§7) ----------------------------------------------------
    void emitFlat(const TreeNode& node, std::string& out) const;
    std::string record(const TreeNode& node) const;
    void parseFlatNode(std::vector<std::string>::const_iterator& it,
                        std::vector<std::string>::const_iterator end, TreeNode& out);
    void emitAscii(const TreeNode& node, const std::string& indentStr, int depth, int maxDepth,
                   bool last, bool hasParent, std::string& out) const;
    void writeNodeJson(const TreeNode& node, int ind, std::string& out) const;
    void emitJson(const TreeNode& node, std::string& out, int indent) const;

    // Per-seat final-hand aggregation used by dealer settlement.
    int aggregateSeatResult(const std::vector<HandEntry>& hands, int dealerValue, bool dealerBust,
                            bool dealerHasBlackjack) const;
    std::vector<Card> initialSeatCards(int start, int i) const;

    std::vector<Card> shoe_;              // immutable input sequence for this run
    int seatCount_ = 1;
    std::vector<Chunk> chunks_;
    int nextNodeId_ = 0;                   // pre-order id assigned as nodes are created
    // Current-chunk build context (populated by buildRound):
    int curDeckStart_ = 0;
    Card curDealerUp_{};
    std::vector<std::vector<Card>> curSeatCards_;
};

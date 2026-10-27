// ---------------------------------------------------------------------------
// Action Tree Tool tests (§8 of docs/action_tree_spec.md).
// 1. Unit (no Game): legal-action sets + isBasicStrategy labeling, split fan-
//    out / MAX_SPLIT_HANDS cap, double-after-exactly-one-card, surrender only
//    on two-card hands, initial-deal deal-order mirror, flat byte-identical
//    round-trip, JSON structural fidelity, determinism.
// 2. Cross-check vs real headless Game (§8.2): the basic-strategy spine must
//    reproduce Game's prescribed actions, outcomes and card-consumption exactly.
// 3. Size measurement (§8.3) + determinism (§8.4).
//
// Rules are MIRRORED (not driven); only Card / Hand / Constants /
// BasicStrategy::getRecommendedAction are reused from the game sources.
// ---------------------------------------------------------------------------

#include <catch2/catch_all.hpp>
#include <ActionTree.hpp>
#include <Card.hpp>
#include <Constants.hpp>
#include <BasicStrategy.hpp>
#include <Game.hpp>
#include <RoundEngine.hpp>
#include <Player.hpp>
#include <Hand.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstring>

using namespace Constants;

namespace
{
Card makeCard(int value, Suit suit)
{
    Card c;
    c.value = value;
    c.suit = suit;
    return c;
}

// Full NUM_DECKS shoe in a fixed order (rank 1..13 x four suits x decks).
std::vector<Card> fullShoe()
{
    std::vector<Card> shoe;
    for (int d = 0; d < NUM_DECKS; ++d)
        for (int v = 1; v <= 13; ++v)
            for (Suit st : {Suit::Spades, Suit::Hearts, Suit::Diamonds, Suit::Clubs})
                shoe.push_back(makeCard(v, st));
    return shoe;
}

// Canonical seat-0 shoe: P_c1, dealer up, P_c2, then filler. Mirrors ActionTree's own initial-deal
// layout (seat pair at start/start+2, dealer up at start+seatCount) so the tree and a real Game deal
// identical physical cards from identical positions.
std::vector<Card> seatShoe(const Card& c1, const Card& c2, const Card& dealerUp, int fill = 64)
{
    std::vector<Card> shoe;
    shoe.push_back(c1);
    shoe.push_back(dealerUp);
    shoe.push_back(c2);
    for (int i = 0; i < fill; ++i)
        shoe.push_back(makeCard(2 + (i % 9), Suit::Spades)); // values 2..10, enough to never run dry
    return shoe;
}

enum class Coarse
{
    Win,
    Lose,
    Push,
    Surrender
};

struct GameRound
{
    std::vector<std::vector<Card>> seatHands;
    std::vector<Coarse> seatOutcomes;
    std::vector<Card> dealerCards;
};

Coarse mapResult(SettlementResult r)
{
    switch (r)
    {
    case SettlementResult::Surrender:
        return Coarse::Surrender;
    case SettlementResult::Lose:
        return Coarse::Lose;
    case SettlementResult::Push:
        return Coarse::Push;
    case SettlementResult::Win:
        return Coarse::Win;
    }
    return Coarse::Push;
}

// Drive a headless Game's round to completion under AutoPlay (which makes even seat 0 act on the
// prescribed basic strategy and advances betting / insurance / dealer turn). Returns each seat's final
// hand cards, that seat's production-settled outcome, and the finished dealer hand. Completion is detected
// from settled state (all hands finished + dealer resolved), independent of roundState, so a stray AutoPlay
// nextRound can never overwrite the captured data mid-read.
GameRound driveRealGame(const std::vector<Card>& shoe, int seats = 1)
{
    Game game(true); // headless: no window / assets / replay / HUD
    game.setDeck(shoe);
    game.startTestRound(seats);
            game.setGameSpeedForTest(GameSpeed::AutoPlay);

    RoundEngine engine;
    GameRound result;
    int guard = 0;
    while (guard++ < 5000)
    {
        // engine.tick() does not touch Game::deltaTime (set from a real clock in Game::update), so
        // propagate it here: otherwise dealerTurn's timer freezes and the dealer never draws.
        game.deltaTime = 1.0f;
        engine.tick(1.0f, game); // positive dt so the NPC action timer actually advances actions

        bool allDone = true;
        for (int p = 0; p < game.playerCountForTest(); ++p)
            for (const Hand& h : game.testPlayer(p).hands)
                if (!h.isFinished())
                {
                    allDone = false;
                    break;
                }

        const Hand& dl = game.testDealer();
        bool dealerResolved = dl.isBust() || dl.getValue() >= DEALER_STAND;
        if (guard <= 10)
            fprintf(stderr, "[DBG-DRG] g=%d dl.size=%zu val=%d allDone=%d resolved=%d\n",
                    guard, dl.size(), dl.getValue(), (int)allDone, (int)dealerResolved);
        if (allDone && dealerResolved)
            break;
    }

    // Final dealer reference for production settlement.
    const Hand& dl = game.testDealer();
    int dealerValue = dl.getValue();
    bool dealerBust = dl.isBust();
    bool dealerBlackjack = (dl.size() == 2 && dl.isBlackjack());

    for (int p = 0; p < game.playerCountForTest(); ++p)
    {
        Player& player = game.testPlayer(p);
        std::vector<Card> handCards;
        for (const Hand& h : player.hands)
        {
            for (std::size_t k = 0; k < h.size(); ++k)
                handCards.push_back(h.getCard(k));
            int payout = 0;
            SettlementResult res = player.settle(h, dealerValue, dealerBust, dealerBlackjack, payout);
            result.seatOutcomes.push_back(mapResult(res));
        }
        result.seatHands.push_back(std::move(handCards));
    }
    for (std::size_t k = 0; k < dl.size(); ++k)
        result.dealerCards.push_back(dl.getCard(k));
    return result;
}

// Total cards dealt from the shoe during one round: every seat hand card + the dealer's cards.
std::size_t gameCardsConsumed(const GameRound& r)
{
    std::size_t total = 0;
    for (const auto& hs : r.seatHands)
        total += hs.size();
    total += r.dealerCards.size();
    return total;
}

// Flat pre-order list of value-type nodes, so a test can fan out / inspect any node by identity.
void collectNodes(const TreeNode& n, std::vector<TreeNode>& out)
{
    out.push_back(n);
    for (const TreeNode& c : n.children)
        collectNodes(c, out);
}
std::vector<TreeNode> collectNodes(const TreeNode& root)
{
    std::vector<TreeNode> v;
    collectNodes(root, v);
    return v;
}

// A round's seat-0 initial hand sits at chunk.root.children[0] (root is the DealRoot). Return that node
// so legal-action / basic-strategy assertions can inspect its edges. The built forest is kept alive in a
// thread-local static so the returned pointer stays valid for the whole test case; the old code deep-copied
// each root into `cached` but still returned the local `forest`'s address (dangling -> use-after-free).
static thread_local std::vector<Chunk> g_forestForHelpers;

const TreeNode* seatZeroNode(const Card& c1, const Card& c2, const Card& dealerUp)
{
    ActionTree at(seatShoe(c1, c2, dealerUp), 1);
    g_forestForHelpers = at.buildShoeForest();
    REQUIRE(!g_forestForHelpers.empty());
    CHECK(g_forestForHelpers.front().root.outcome == Outcome::DealRoot);
    CHECK(!g_forestForHelpers.front().root.children.empty());
    // The chunk root is seat-0's initial-deal decision node: its direct children are the canonical
    // legal-action edges {H,S,D,P,R} (§1.1/§4). Return it so action/basic-strategy assertions inspect
    // those edges directly (the old collapsed-model assumption that children[0] *is* the seat-0 node was wrong).
    return &g_forestForHelpers.front().root;
}

// Map ActionTree's internal ResultCode (PUSH/WIN/LOSS/SURRENDER/BLACKJACK) to Coarse. Game reports a
// natural unsplit blackjack as a Win, so BLACKJACK(4) maps there too for the cross-check comparison.
Coarse coarseFromResult(int code)
{
    switch (code)
    {
    case 0:
        return Coarse::Push; // PUSH
    case 1:
        return Coarse::Win; // WIN
    case 2:
        return Coarse::Lose; // LOSS
    case 3:
        return Coarse::Surrender; // SURRENDER
    default:
        return Coarse::Win; // BLACKJACK
    }
}

// Aggregate a seat's per-hand coarse outcomes into one, mirroring ActionTree::aggregateSeatResult.
Coarse aggregateCoarse(const std::vector<Coarse>& cs)
{
    bool hasWin = false, hasLoss = false;
    bool allPush = true;
    for (const auto& c : cs)
    {
        if (c == Coarse::Win)
            hasWin = true;
        else if (c == Coarse::Lose)
            hasLoss = true;
        if (c != Coarse::Push)
            allPush = false;
    }
    if (allPush)
        return Coarse::Push;
    if (hasWin && !hasLoss)
        return Coarse::Win;
    if (hasLoss && !hasWin)
        return Coarse::Lose;
    return hasWin ? Coarse::Win : Coarse::Lose;
}

// Follow the isBasicStrategy edges from a chunk root down to its terminal DealerResult leaf (§5/§8).
const TreeNode* spineLeaf(const Chunk& c)
{
    const TreeNode* cur = &c.root;
    while (true)
    {
        const TreeNode* next = nullptr;
        for (const TreeNode& child : cur->children)
            if (child.isBasicStrategy)
            {
                next = &child;
                break;
            }
        if (!next)
            break;
        cur = next;
    }
    return cur;
}

std::vector<char> childDecisions(const TreeNode& n)
{
    std::vector<char> d;
    for (const TreeNode& c : n.children)
        d.push_back(c.decision);
    return d;
}
} // namespace

// §6: the chunk root must hold each seat's fixed starting pair and the dealer up-card read from the exact
// positions Game::deal() uses, on a per-path deck cursor that only advances during play.
TEST_CASE("Initial deal mirrors Game::deal() position-by-position", "[ActionTree][DealOrder]")
{
    std::vector<Card> shoe;
    shoe.push_back(makeCard(10, Suit::Spades));  // 0: seat0 card 1
    shoe.push_back(makeCard(5, Suit::Hearts));   // 1: dealer up
    shoe.push_back(makeCard(8, Suit::Diamonds)); // 2: seat0 card 2
    shoe.push_back(makeCard(2, Suit::Clubs));    // 3: dealer hole
    for (int i = 0; i < 40; ++i)
        shoe.push_back(makeCard(3 + (i % 8), Suit::Hearts));

    ActionTree at(std::move(shoe), 1);
    std::vector<Chunk> forest = at.buildShoeForest();
    REQUIRE(!forest.empty());   // whole shoe -> a forest of per-round chunks (§1.6); we assert the FIRST round's deal

    const Chunk& c = forest[0];
    CHECK(c.deckStartCursor == 0);
    REQUIRE(c.seats.size() == 1);
    CHECK(c.seats[0].index == 0);
    CHECK(c.seats[0].name == "William");
    CHECK(c.seats[0].startCards.size() == 2);
    CHECK(c.seats[0].startCards[0].value == 10);
    CHECK(c.seats[0].startCards[1].value == 8);
    CHECK(c.root.cards.size() == 2);
    CHECK(c.root.cards[0].value == 10);
    CHECK(c.root.cards[1].value == 8);
}

// §4: legal-action enumeration is a canonical-ordered subset of {H,S,D,P,R}.
TEST_CASE("Legal actions match the canonical H,S,D,P,R subset for representative hands", "[ActionTree][LegalActions]")
{
    // Hard 16 vs a ten: two non-pair cards -> can hit, stand, double and surrender, but not split.
    {
        const TreeNode* n = seatZeroNode(makeCard(10, Suit::Spades), makeCard(6, Suit::Hearts),
                                         makeCard(10, Suit::Diamonds));   // hard 16 vs a ten
        std::vector<char> cd = childDecisions(*n);
        CHECK(std::set<char>(cd.begin(), cd.end()) == std::set<char>{'H', 'S', 'D', 'R'});
    }

    // Pair of eights vs a five: splittable, so P is offered alongside H/S/D/R.
    {
        const TreeNode* n = seatZeroNode(makeCard(8, Suit::Spades), makeCard(8, Suit::Hearts),
                                         makeCard(5, Suit::Diamonds));
        std::vector<char> cd = childDecisions(*n);
        fprintf(stderr, "[DBG-B2] childDecisions size=%zu first='%c'\n", cd.size(), cd.empty() ? '?' : cd[0]);
        std::set<char> choices(cd.begin(), cd.end());
        CHECK(choices == std::set<char>{'H', 'S', 'D', 'P', 'R'});
    }

    // Post-hit hands (>=3 cards) collapse to H/S only. Start 12 vs a three -> basic strategy hits, so the
    // first decision branch is a hit producing a >=3 card node whose children are just H/S.
    {
        const TreeNode* n = seatZeroNode(makeCard(4, Suit::Spades), makeCard(8, Suit::Hearts),
                                         makeCard(3, Suit::Diamonds));
        bool foundHitNode = false;
        for (const auto& kv : collectNodes(*n))
            if (!kv.isDealerNode && kv.cards.size() >= 3)   // player post-hit hands only (not the dealer chain)
            {
                foundHitNode = true;
                for (const TreeNode& c : kv.children)
                    if (!c.isDealerNode) CHECK((c.decision == 'H' || c.decision == 'S'));   // only player action edges
            }
        CHECK(foundHitNode);
    }
}

// §4/§5: exactly one edge per node is the prescribed basic-strategy line, and it matches getRecommendedAction.
TEST_CASE("isBasicStrategy marks exactly the prescribed edge", "[ActionTree][BasicStrategy]")
{
    // Soft 17 (A+6) vs a nine: play it -> basic strategy stands.
    {
        const TreeNode* n = seatZeroNode(makeCard(1, Suit::Spades), makeCard(6, Suit::Hearts),
                                         makeCard(9, Suit::Diamonds));
        int bsCount = 0;
        char bsDecision = ' ';
        for (const TreeNode& c : n->children)
            if (c.isBasicStrategy)
            {
                ++bsCount;
                bsDecision = c.decision;
            }
        CHECK(bsCount == 1);
        CHECK(bsDecision == BasicStrategy::getRecommendedAction(
                                17, makeCard(9, Suit::Diamonds).getBlackjackValue(), true, false, 0));
    }

    // Pair of eights vs a five: basic strategy splits -> the P edge is the basic-strategy one.
    {
        const TreeNode* n = seatZeroNode(makeCard(8, Suit::Spades), makeCard(8, Suit::Hearts),
                                         makeCard(5, Suit::Diamonds));
        int bsCount = 0;
        for (const TreeNode& c : n->children)
            if (c.isBasicStrategy)
                ++bsCount;
        CHECK(bsCount == 1);
    }

    // Hard 11 vs a five: basic strategy doubles -> the D edge is the basic-strategy one.
    {
        const TreeNode* n = seatZeroNode(makeCard(9, Suit::Spades), makeCard(2, Suit::Hearts),
                                         makeCard(5, Suit::Diamonds));
        bool found = false;
        for (const TreeNode& c : n->children)
            if (c.isBasicStrategy)
                found = (c.decision == 'D');
        CHECK(found);
    }
}

// §4: a double-down ends the hand after exactly one forced card — no further branching.
TEST_CASE("A doubled hand terminates after exactly one forced card", "[ActionTree][Double]")
{
    const TreeNode* n = seatZeroNode(makeCard(9, Suit::Spades), makeCard(2, Suit::Hearts),
                                     makeCard(5, Suit::Diamonds)); // hard 11 vs 5 -> double
    int terminals = 0;
    for (const auto& kv : collectNodes(*n))
        if (kv.cards.size() == 3 && kv.outcome == Outcome::DoubleEnd)
            ++terminals;
    CHECK(terminals == 1);

    const TreeNode* dNode = nullptr;
    for (const TreeNode& c : n->children)
        if (c.decision == 'D')
        {
            dNode = &c;
            break;
        }
    REQUIRE(dNode != nullptr);
    CHECK(dNode->outcome == Outcome::DoubleEnd);
    CHECK(dNode->cards.size() == 3);   // two initial cards + exactly one forced double-down card (§7)
    // Per spec §1.4 the line still completes through the dealer; the doubled hand's result lands on the
    // DealerResult leaf below it, so assert settlement is reachable (the old `children.empty()` assumed a
    // collapsed-model terminal node that no longer reflects the full-tree model).
    bool settled = false;
    for (const auto& kv : collectNodes(*dNode))
        if (kv.outcome == Outcome::DealerResult && !kv.seatResults.empty())
            settled = true;
    CHECK(settled);
}

// §4: surrender is offered only on two-card hands; once a hand has been hit (>=3 cards) it collapses to H/S.
TEST_CASE("Surrender is available only on two-card hands", "[ActionTree][Surrender]")
{
    // 16 vs a nine -> basic strategy surrenders: R must be among the initial (2-card) choices.
    {
        const TreeNode* n = seatZeroNode(makeCard(10, Suit::Spades), makeCard(6, Suit::Hearts),
                                         makeCard(9, Suit::Diamonds));
        bool hasR = false;
        for (const TreeNode& c : n->children)
            if (c.decision == 'R')
                hasR = true;
        CHECK(hasR);
    }

    // A 3-card hand never offers P/D/R — only H/S remain.
    {
        const TreeNode* n = seatZeroNode(makeCard(4, Suit::Spades), makeCard(8, Suit::Hearts),
                                         makeCard(3, Suit::Diamonds)); // 12 vs 3 -> hit first
        bool foundHitNode = false;
        for (const auto& kv : collectNodes(*n))
            if (!kv.isDealerNode && kv.cards.size() >= 3)   // player post-hit hands only (not the dealer chain)
            {
                foundHitNode = true;
                for (const TreeNode& c : kv.children)
                    if (!c.isDealerNode) CHECK((c.decision == 'H' || c.decision == 'S'));   // only player action edges
            }
        CHECK(foundHitNode);
    }
}

// §4/§6: a split adds exactly the sibling hands; re-splits grow past the initial two but never exceed
// MAX_SPLIT_HANDS.
TEST_CASE("A split adds exactly the sibling hands; re-split is capped at MAX_SPLIT_HANDS", "[ActionTree][Split]")
{
    // Initial pair of eights -> two siblings, no further re-split (filler cards don't make pairs).
    {
        ActionTree at(seatShoe(makeCard(8, Suit::Spades), makeCard(8, Suit::Hearts),
                               makeCard(5, Suit::Diamonds)),
                      1);
        std::vector<Chunk> forest = at.buildShoeForest();

        const TreeNode& marker = forest[0].root.children[0];
        int pChildren = 0;
        for (const auto& kv : collectNodes(marker))
            if (kv.outcome == Outcome::SplitSiblingStart)
                ++pChildren;
        CHECK(pChildren == 1); // one split marker under seat 0
        std::size_t maxFanOut = 0;
        for (const auto& kv : collectNodes(forest[0].root))
            if (kv.outcome == Outcome::SplitSiblingStart)
                maxFanOut = std::max(maxFanOut, kv.children.size());
        CHECK(maxFanOut == 2);
    }

    // Resplitting aces: two ace hands each draw an ace -> each re-splits into three (grows past the initial
    // two), and no node ever exceeds MAX_SPLIT_HANDS.
    {
        std::vector<Card> shoe;
        shoe.push_back(makeCard(1, Suit::Spades));   // 0: ace pair c1
        shoe.push_back(makeCard(5, Suit::Hearts));   // 1: dealer up (non-ace -> PlayerTurn)
        shoe.push_back(makeCard(1, Suit::Diamonds)); // 2: ace pair c2
        shoe.push_back(makeCard(2, Suit::Clubs));    // 3: dealer hole
        shoe.push_back(makeCard(1, Suit::Spades));   // forced card ace hand 0 -> {A,A} re-splits
        shoe.push_back(makeCard(1, Suit::Hearts));   // forced card ace hand 1 -> {A,A} re-splits
        for (int i = 0; i < 40; ++i)
            shoe.push_back(makeCard(2 + (i % 9), Suit::Diamonds));

        ActionTree at(std::move(shoe), 1);
        std::vector<Chunk> forest = at.buildShoeForest();

        std::size_t maxFanOut = 0, splitsSeen = 0;
        for (const auto& kv : collectNodes(forest[0].root))
            if (kv.outcome == Outcome::SplitSiblingStart)
            {
                ++splitsSeen;
                maxFanOut = std::max(maxFanOut, kv.children.size());
            }
        CHECK(splitsSeen >= 1);
        CHECK(maxFanOut >= 3);                          // re-split grew beyond the initial two
        CHECK(maxFanOut <= Constants::MAX_SPLIT_HANDS); // ...but never past the hard cap
    }
}

// §7a: the flat pre-order save format round-trips byte-identically and rebuilds an equivalent tree.
TEST_CASE("Flat pre-order text round-trips byte-identically", "[ActionTree][Flat]")
{
    ActionTree at(fullShoe(), 1);
    std::vector<Chunk> forest = at.buildShoeForest();
    REQUIRE(!forest.empty());

    std::string flat = at.toFlatText();
    CHECK(flat.rfind("FOREST ", 0) == 0); // primary save format header

    ActionTree rebuilt = ActionTree::fromFlatText(flat);
    CHECK(rebuilt.toFlatText() == flat); // byte-identical re-save

    int chunksA = 0, chunksB = 0;
    for (char ch : flat)
        if (ch == 'C')
            ++chunksA; // "CHUNK" lines contain a C
    std::string flat2 = rebuilt.toFlatText();
    for (char ch : flat2)
        if (ch == 'C')
            ++chunksB;
    CHECK(chunksA == chunksB);
}

// §7c: the JSON export carries every field a future visualizer needs, verbatim from the model.
TEST_CASE("JSON export preserves chunk/seat/node structure", "[ActionTree][Json]")
{
    ActionTree at(seatShoe(makeCard(8, Suit::Spades), makeCard(8, Suit::Hearts),
                           makeCard(5, Suit::Diamonds)),
                  1);
    std::vector<Chunk> forest = at.buildShoeForest();
    REQUIRE(!forest.empty());

    std::string j = at.toJson();

    CHECK(j.find("\"roundIndex\"") != std::string::npos);
    CHECK(j.find("\"deckStartCursor\"") != std::string::npos);
    CHECK(j.find("\"name\": \"William\"") != std::string::npos);

    // Every node object exposes the full field set the viz consumes directly.
    const char* keys[] = {"nodeId", "seatIndex", "isDealerNode", "decision",
                          "isBasicStrategy", "cards", "handValue", "isSoft",
                          "outcome", "deckCursorAtNode", "children"};
    for (const char* key : keys)
        CHECK(j.find(key) != std::string::npos);

    // Terminal settlement survives per-seat on the dealer-result leaf.
    CHECK(j.find("\"seatResults\"") != std::string::npos);

    // Deterministic re-serialization (no randomness in the export).
    CHECK(at.toJson() == j);
}

// §8.2: FIDELITY GATE — the generator's basic-strategy spine must reproduce a real headless Game's
// prescribed actions, outcomes and card-consumption exactly (same shoe, same rule mirror). This is the
// check that locks the rule mirror to src/Game.cpp + src/RoundEngine.cpp. driveRealGame() above is the
// independent oracle; we compare its per-seat settlement + total cards consumed against the tree's spine.

// Follow the prescribed line: take the basic-strategy edge when present, else the single linear continuation
// (dealer chain / split-hand tail are unbranched), stopping at the terminal DealerResult leaf.
const TreeNode* prescribedLeaf(const Chunk& c)
{
    const TreeNode* cur = &c.root;
    while (!cur->children.empty() && cur->outcome != Outcome::DealerResult)
    {
        const TreeNode* next = nullptr;
        for (const TreeNode& ch : cur->children)
            if (ch.isBasicStrategy) { next = &ch; break; }
        if (!next) next = &cur->children.front();   // linear continuation (dealer chain / split tail)
        cur = next;
    }
    return cur;
}

TEST_CASE("Basic-strategy spine reproduces real Game (§8.2)", "[ActionTree][CrossCheck]")
{
    struct Case { Card c1, up, c2; const char* name; };
    const Case cases[] = {
        {makeCard(10, Suit::Spades), makeCard(7, Suit::Hearts), makeCard(6, Suit::Diamonds), "16v7-hit"},   // 16 vs 7 -> hit/stand
        {makeCard(5, Suit::Spades),  makeCard(3, Suit::Hearts), makeCard(4, Suit::Diamonds), "9v3-double"},  // 9 vs 3 -> double
        {makeCard(10, Suit::Spades), makeCard(9, Suit::Hearts), makeCard(6, Suit::Diamonds), "16v9-surr"},   // 16 vs 9 -> surrender
        {makeCard(8, Suit::Spades),  makeCard(5, Suit::Hearts), makeCard(8, Suit::Diamonds), "88v5-split"},   // 8,8 vs 5 -> split
        {makeCard(2, Suit::Spades),  makeCard(5, Suit::Hearts), makeCard(2, Suit::Diamonds), "22v5-split"},   // 2,2 vs 5 -> split
        {makeCard(1, Suit::Spades),  makeCard(1, Suit::Hearts), makeCard(1, Suit::Clubs),    "AAvA-split"},   // A,A vs A -> split aces
        {makeCard(10, Suit::Spades), makeCard(10, Suit::Hearts), makeCard(10, Suit::Diamonds), "tensvsTens"}  // tens vs ten -> stand (P node exists)
    };

    for (const Case& cs : cases)
    {
        // Minimal filler keeps the multi-round forest cheap; values 2..9 avoid a second explosion downstream.
        std::vector<Card> shoe;
        shoe.push_back(cs.c1);
        shoe.push_back(cs.up);
        shoe.push_back(cs.c2);
        for (int i = 0; i < 8; ++i)
            shoe.push_back(makeCard(2 + (i % 8), Suit::Spades));

        ActionTree at(shoe, 1);
        std::vector<Chunk> forest = at.buildShoeForest();
        REQUIRE(!forest.empty());
        const Chunk& c = forest[0];

        const TreeNode* leaf = prescribedLeaf(c);
        REQUIRE(leaf != nullptr);
        CHECK(leaf->outcome == Outcome::DealerResult);   // spine reaches a settled leaf

        GameRound g = driveRealGame(shoe, 1);

        Coarse expected = leaf->seatResults.empty() ? Coarse::Push : coarseFromResult(leaf->seatResults[0]);
        Coarse actual = aggregateCoarse(g.seatOutcomes);

        // TEMP detailed diagnostics for every case.
        {
            std::string ev = (expected == Coarse::Win ? "WIN" : expected == Coarse::Lose ? "LOSE" :
                              expected == Coarse::Surrender ? "SURR" : "PUSH");
            std::string av = (actual == Coarse::Win ? "WIN" : actual == Coarse::Lose ? "LOSE" :
                              actual == Coarse::Surrender ? "SURR" : "PUSH");
            fprintf(stderr, "[DBG-DETAIL] %-22s GAME seatVals=[", cs.name);
            for (auto& hs : g.seatHands) { for (auto& cc : hs) fprintf(stderr, "%d,%d", cc.value, cc.getBlackjackValue()); }
            fprintf(stderr, "] dealer=");
            for (auto& cc : g.dealerCards) fprintf(stderr, "%d,", cc.value);
            fprintf(stderr, " tree seatResults=[");
            for (int r : leaf->seatResults) fprintf(stderr, "%d,", r);
            fprintf(stderr, "] expected=%s actual=%s\n", ev.c_str(), av.c_str());
        }

        long long treeConsumed = static_cast<long long>(leaf->deckCursorAtNode - c.deckStartCursor);
        long long gameConsumed = static_cast<long long>(gameCardsConsumed(g));

        fprintf(stderr, "[DBG-CC] %-24s treeCons=%3lld gameCons=%3lld outcome=%-8s match=%d cards=%d\n",
                cs.name, treeConsumed, gameConsumed,
                expected == Coarse::Win ? "WIN" : expected == Coarse::Lose ? "LOSE" :
                expected == Coarse::Surrender ? "SURR" : "PUSH",
                (expected == actual && treeConsumed == gameConsumed) ? 1 : 0,
                (int)g.seatOutcomes.size());

        CHECK(expected == actual);   // seat-0 outcome matches real Game (see [DBG-CC] match column)
        CHECK(treeConsumed == gameConsumed);   // card consumption matches real Game
    }
}

// §8.3: size measurement — build a whole shoe, summarize() counts nodes/leaves/max-depth and prints scale so
// limits can be chosen from real numbers rather than guessed.
TEST_CASE("summarize reports node/leaf/max-depth for a full-shoe forest", "[ActionTree][Size]")
{
    ActionTree at(fullShoe(), 1);
    std::vector<Chunk> forest = at.buildShoeForest();
    REQUIRE(!forest.empty());

    std::size_t nodes = 0, leaves = 0;
    int maxDepth = 0;
    at.summarize(nodes, leaves, maxDepth);

    CHECK(nodes > 0);
    CHECK(leaves > 0);
    CHECK(maxDepth >= 1);
    // A full shoe of NUM_DECKS rounds each branches well beyond a single path.
    CHECK(nodes > forest.size());
    printf("[ActionTree][Size] chunks=%zu nodes=%zu leaves=%zu maxDepth=%d\n",
           forest.size(), nodes, leaves, maxDepth);

    // Multi-seat runs fan out more (each seat is an independent line under the round).
    ActionTree multi(fullShoe(), 2);
    std::vector<Chunk> mforest = multi.buildShoeForest();
    std::size_t mn = 0, ml = 0;
    int md = 0;
    multi.summarize(mn, ml, md);
    printf("[ActionTree][Size] multi-seat nodes=%zu leaves=%zu maxDepth=%d\n", mn, ml, md);
    CHECK(mn >= nodes);
}

// §8.4: determinism — the same input sequence produces identical flat output across two independent runs /
// instances. The generator is a pure function of the immutable shoe.
TEST_CASE("Output is deterministic across runs", "[ActionTree][Determinism]")
{
    const std::vector<Card> shoe = seatShoe(makeCard(8, Suit::Spades), makeCard(8, Suit::Hearts),
                                            makeCard(5, Suit::Diamonds));

    ActionTree at1(shoe, 1);
    ActionTree at2(shoe, 1);
    at1.buildShoeForest();
    at2.buildShoeForest();

    CHECK(at1.toFlatText() == at2.toFlatText());
    CHECK(at1.toJson() == at2.toJson());

    std::vector<ActionTree::Spine> s1 = at1.extractBasicStrategySpines();
    std::vector<ActionTree::Spine> s2 = at2.extractBasicStrategySpines();
    CHECK(s1.size() == s2.size());
    for (std::size_t i = 0; i < s1.size(); ++i)
        CHECK(s1[i].nodeIds == s2[i].nodeIds);
}

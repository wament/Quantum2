#include "ActionTree.hpp"

#include "BasicStrategy.hpp"
#include "Constants.hpp"
#include "Hand.hpp"

#include <string>
#include <sstream>
#include <cstdio>

namespace
{
    enum ResultCode
    {
        PUSH = 0,
        WIN = 1,
        LOSS = 2,
        SURRENDER = 3,
        BLACKJACK = 4
    };

    const char* kSeatNames[] = {"William", "Alice", "Bob", "Carol", "Dave", "Eve"};

    std::string seatName(int i)
    {
        if (i >= 0 && i < static_cast<int>(sizeof(kSeatNames) / sizeof(kSeatNames[0])))
            return kSeatNames[i];
        return "Seat" + std::to_string(i);
    }

    Card cardWithBJ(int bjValue, Suit suit)
    {
        Card c;
        c.value = (bjValue == 11) ? 1 : (bjValue > 10 ? 10 : bjValue);
        c.suit = suit;
        return c;
    }

    bool isPairOf(const std::vector<Card>& cards)
    {
        return cards.size() == 2 &&
               cards[0].getBlackjackValue() == cards[1].getBlackjackValue();
    }

    int settleResult(const std::vector<Card>& handCards, bool split, bool surrendered,
                     int dealerValue, bool dealerBust, bool dealerHasBlackjack)
    {
        Hand hand(handCards);
        if (surrendered)
            return SURRENDER;
        const bool naturalBJ = hand.isBlackjack() && !split;   // only an unsplit natural pays 3:2
        if (naturalBJ)
            return dealerHasBlackjack ? PUSH : BLACKJACK;
        if (hand.isBust() || (hand.getValue() < dealerValue && !dealerBust))
            return LOSS;
        if (hand.getValue() > dealerValue || dealerBust)
            return WIN;
        return PUSH;
    }

    std::string resultName(int code)
    {
        switch (code)
        {
        case PUSH:      return "PUSH";
        case WIN:       return "WIN";
        case LOSS:      return "LOSS";
        case SURRENDER: return "SURRENDER";
        case BLACKJACK: return "BLACKJACK";
        }
        return "UNKNOWN";
    }

    std::string decisionLabel(char d, Outcome outcome)
    {
        return (outcome == Outcome::DealRoot) ? "DEAL" : std::string(1, d);
    }
}

ActionTree::ActionTree(std::vector<Card> shoeSequence, int seatCount)
    : shoe_(std::move(shoeSequence)), seatCount_(seatCount > 0 ? seatCount : 1)
{
}

std::vector<Card> ActionTree::initialSeatCards(int start, int i) const
{
    // Mirror Game::deal() interleaving: pass1 = s0c1..sN-1 c1; dealerUp; pass2 = s0c2..; dealerHole.
    std::vector<Card> pair;
    pair.push_back(shoe_[start + i]);
    pair.push_back(shoe_[start + seatCount_ + 1 + i]);
    return pair;
}

// Legal-action enumeration (§4): canonical-ordered subset of {H,S,D,P,R}, mirroring Game.cpp.
std::vector<char> ActionTree::legalActions(const HandInfo& hand, int handCount) const
{
    const int n = static_cast<int>(hand.cards.size());
    if (n == 0)
        return {};

    Hand handObj(hand.cards);
    const int value = handObj.getValue();

    // A split ace that received its one forced card stands unless it can be re-split.
    const bool resplitEligibleAces = hand.splitAce && n >= 2 && isPairOf(hand.cards) &&
                                     hand.cards[0].getBlackjackValue() == 11 &&
                                     handCount < Constants::MAX_SPLIT_HANDS &&
                                     Constants::RESPLIT_ACES_ALLOWED;

    if (hand.splitAce && n >= 2)
        return resplitEligibleAces ? std::vector<char>{'P'} : std::vector<char>{};

    std::vector<char> actions;
    actions.push_back('H');                                  // act while not a finished split ace
    if (value < 21)
        actions.push_back('S');

    const bool canDouble = (n == 2);                         // Hand::canDouble: two cards, not doubled
    if (canDouble && !(hand.split && !Constants::DOUBLE_AFTER_SPLIT_ALLOWED))
        actions.push_back('D');

    const int pairValue = isPairOf(hand.cards) ? hand.cards[0].getBlackjackValue() : 0;
    const bool canSplit = (n == 2 && isPairOf(hand.cards) && handCount < Constants::MAX_SPLIT_HANDS &&
                           !(hand.split && pairValue == 10 && !Constants::RESPLIT_TENS_ALLOWED));
    if (canSplit)
        actions.push_back('P');

    if (n == 2)                                              // surrender: any eligible first two cards (§4 knob)
        actions.push_back('R');

    return actions;   // canonical order H, S, D, P, R
}

struct AppliedAction { std::vector<Card> cards; int nextCursor = 0; Outcome outcome = Outcome::HitContinue; };

// Apply one non-split action (§4): returns updated hand, next cursor and terminal kind.
AppliedAction applySimpleAction(const Card& /*dealerUp*/, const HandInfo& hand, int cursor, char action,
                                const std::vector<Card>& shoe)
{
    AppliedAction ap;
    ap.cards = hand.cards;
    ap.nextCursor = cursor;
    switch (action)
    {
    case 'H':
        if (cursor < static_cast<int>(shoe.size()))
        {
            ap.cards.push_back(shoe[cursor]);
            ap.nextCursor = cursor + 1;
            {
                Hand h(ap.cards);
                if (h.isBust()) ap.outcome = Outcome::Bust;
                else if (h.getValue() == 21) ap.outcome = Outcome::TwentyOne;
                else ap.outcome = Outcome::HitContinue;
            }
        }
        else   // shoe exhausted: cannot draw, hand stands at its current value
        {
            ap.nextCursor = cursor;
            ap.outcome = Outcome::Stand;
        }
        break;
    case 'S': ap.outcome = Outcome::Stand; break;
    case 'D':
        if (cursor < static_cast<int>(shoe.size()))
        {
            ap.cards.push_back(shoe[cursor]);
            ap.nextCursor = cursor + 1;
            ap.outcome = Outcome::DoubleEnd;
        }
        else   // cannot force the double-down card: finish at current value (stand)
        {
            ap.nextCursor = cursor;
            ap.outcome = Outcome::Stand;
        }
        break;
    case 'R': ap.outcome = Outcome::Surrender; break;
    default: ap.outcome = Outcome::Stand; break;
    }
    return ap;
}

// Core DFS over decision points (§6). `node` is the active seat's current hand. Children enumerate
// legal actions in canonical order; each edge either keeps playing the same seat (hit), terminates
// this hand and continues to another seat / split hand / dealer, or fans out a sequential split chain.
void ActionTree::walkActiveSeat(TreeNode& node, int seatIndex, const Card& dealerUp, HandInfo hand,
                                int cursor, int handCount, std::vector<HandEntry>& pathHands,
                                int splitRemaining, int familyPairValue, Suit familySuit)
{
    Hand h(hand.cards);
    const int value = h.getValue();
    const bool isPair = isPairOf(hand.cards);
    const int pairValue = isPair ? hand.cards[0].getBlackjackValue() : 0;

    node.seatIndex = seatIndex;
    node.isDealerNode = false;
    node.cards = hand.cards;
    node.handValue = value;
    node.isSoft = h.isSoft();
    node.deckCursorAtNode = cursor;

    const bool naturalBJ = (hand.cards.size() == 2 && !hand.split && h.isBlackjack());
    // A split ace that received its one forced card stands only if it cannot be re-split; a resplit-eligible
    // ace proceeds to legalActions, which offers the 'P' re-split edge.
    const bool resplitEligibleAces = hand.splitAce && hand.cards.size() >= 2 && isPairOf(hand.cards) &&
                                     hand.cards[0].getBlackjackValue() == 11 &&
                                     handCount < Constants::MAX_SPLIT_HANDS && Constants::RESPLIT_ACES_ALLOWED;
    const bool forcedStand = hand.splitAce && hand.cards.size() >= 2 && !naturalBJ && !resplitEligibleAces;

    if (naturalBJ || forcedStand)
    {
        node.decision = 'D';
        node.outcome = naturalBJ ? Outcome::Blackjack : Outcome::Stand;
        pathHands.push_back({seatIndex, hand.cards});
        attachContinuation(node, seatIndex, cursor, pathHands, splitRemaining, handCount, familyPairValue, familySuit);
        pathHands.pop_back();
        return;
    }

    const char rec = BasicStrategy::getRecommendedAction(value, dealerUp.getBlackjackValue(), h.isSoft(),
                                                         isPair, pairValue);

    // Split: emit ONE sibling-fanning marker FIRST (children[0]) whose children are exactly the
    // `totalHands` sibling decision subtrees (§4/§6). This keeps a single SplitSiblingStart node per
    // split with fan-out == number of hands, matching spec §1.3 / ASCII "SPLIT (n hands)" marker.
    const bool canSplit = (hand.cards.size() == 2 && isPair && handCount < Constants::MAX_SPLIT_HANDS &&
                           !(hand.split && pairValue == 10 && !Constants::RESPLIT_TENS_ALLOWED));
    if (canSplit)
    {
        const int totalHands = handCount + 1;   // sibling family size for this split
        TreeNode m;
        m.nodeId = nextNodeId_++;
        m.seatIndex = seatIndex;
        m.isDealerNode = false;
        m.decision = 'P';
        m.outcome = Outcome::SplitSiblingStart;
        m.cards = hand.cards;
        m.handValue = value;
        m.isSoft = h.isSoft();
        m.isBasicStrategy = (rec == 'P');
        m.deckCursorAtNode = cursor;
        node.children.push_back(m);   // children[0]

        TreeNode& mark = node.children.front();   // the split marker

        // First sibling plays out fully and chains to the remaining hands via attachContinuation, so the
        // basic-strategy spine traverses every split hand in sequence (matches Game's sequential play and
        // keeps card-consumption identical).
        HandInfo sib0;
        int sc = cursor;   // local split-cursor: forced cards to split hands are drawn from here so the
                           // parameter `cursor` (the decision-point position for H/S/D/R edges) is not
                           // corrupted by split-card consumption (§fix-1). See ActionTree test [CrossCheck].
        sib0.cards.push_back(cardWithBJ(pairValue, hand.cards[0].suit));   // remaining pair card
        if (sc < static_cast<int>(shoe_.size()))
            sib0.cards.push_back(shoe_[sc++]);                          // one forced card
        sib0.split = true;
        sib0.splitAce = (pairValue == 11);

        TreeNode sibNode0;
        sibNode0.nodeId = nextNodeId_++;
        sibNode0.seatIndex = seatIndex;
        sibNode0.isDealerNode = false;
        sibNode0.decision = 'P';
        sibNode0.cards = sib0.cards;
        Hand sh0(sib0.cards);
        sibNode0.handValue = sh0.getValue();
        sibNode0.isSoft = sh0.isSoft();
        sibNode0.deckCursorAtNode = sc;
        sibNode0.outcome = Outcome::HitContinue;
        mark.children.push_back(sibNode0);

        walkActiveSeat(mark.children.back(), seatIndex, curDealerUp_, sib0, sc, totalHands, pathHands,
                       totalHands - 1, pairValue, hand.cards[0].suit);

        // Remaining siblings are direct children of the marker (so its fan-out == number of hands). Their play
        // lines are represented by the chain above; kept as minimal leaves to bound tree size.
        for (int k = 1; k < totalHands; ++k)
        {
            HandInfo sibk;
            sibk.cards.push_back(cardWithBJ(pairValue, hand.cards[0].suit));
            if (sc < static_cast<int>(shoe_.size()))
                sibk.cards.push_back(shoe_[sc++]);
            sibk.split = true;
            sibk.splitAce = (pairValue == 11);

            TreeNode sibNodeK;
            sibNodeK.nodeId = nextNodeId_++;
            sibNodeK.seatIndex = seatIndex;
            sibNodeK.isDealerNode = false;
            sibNodeK.decision = 'P';
            sibNodeK.cards = sibk.cards;
            Hand shk(sibk.cards);
            sibNodeK.handValue = shk.getValue();
            sibNodeK.isSoft = shk.isSoft();
            sibNodeK.deckCursorAtNode = sc;
            sibNodeK.outcome = Outcome::HitContinue;
            mark.children.push_back(sibNodeK);
        }
    }

    for (const char a : legalActions(hand, handCount))
    {
        if (a == 'P')
            continue;   // split already emitted as the sibling-fanning marker above

        TreeNode c;
        c.nodeId = nextNodeId_++;
        c.seatIndex = seatIndex;
        c.isDealerNode = false;

        AppliedAction ap = applySimpleAction(dealerUp, hand, cursor, a, shoe_);
        c.decision = a;
        c.cards = ap.cards;
        Hand hc(ap.cards);
        c.handValue = hc.getValue();
        c.isSoft = hc.isSoft();
        c.outcome = ap.outcome;
        c.deckCursorAtNode = ap.nextCursor;
        c.isBasicStrategy = (a == rec);
        node.children.push_back(c);

        if (c.outcome == Outcome::HitContinue)
        {
            // Continuing hit (§1.3/§4): the hand is still alive, so keep playing THIS seat's updated hand and
            // enumerate its new H/S edges. The hand only enters pathHands once it terminates downstream.
            HandInfo nh{ap.cards, hand.split, hand.splitAce};
            walkActiveSeat(node.children.back(), seatIndex, dealerUp, nh, ap.nextCursor,
                           handCount, pathHands, splitRemaining, familyPairValue, familySuit);
        }
        else
        {
            // Terminal action (§1.4): this seat's hand is finished -> advance to the next seat / a still-open
            // split sibling / and then the dealer chain on this line. Record surrender so settlement matches Game.
            pathHands.push_back({seatIndex, ap.cards, ap.outcome == Outcome::Surrender});
            attachContinuation(node.children.back(), seatIndex, ap.nextCursor, pathHands, splitRemaining,
                               handCount, familyPairValue, familySuit);
            pathHands.pop_back();
        }
    }
}

// Continue a path after a hand terminates: next split hand of this seat (splitRemaining), else the
// next seat, then — once every seat is done — the dealer chain (§6/§1.4). `totalHands` is the fixed
// sibling-hand count for the current split family.
void ActionTree::attachContinuation(TreeNode& node, int seatIndex, int cursor,
                                    std::vector<HandEntry>& pathHands, int splitRemaining,
                                    int totalHands, int familyPairValue, Suit familySuit)
{
    if (splitRemaining > 0)
    {
        HandInfo nh;
        std::vector<Card> hc;
        hc.push_back(cardWithBJ(familyPairValue, familySuit));   // remaining pair card
        Card forced{};                                           // §4 split dealing; guard shoe bounds
        if (cursor < static_cast<int>(shoe_.size()))
        {
            forced = shoe_[cursor];
            ++cursor;
        }
        hc.push_back(forced);
        nh.cards = hc;
        nh.split = true;
        nh.splitAce = (familyPairValue == 11);

        TreeNode child;
        child.nodeId = nextNodeId_++;
        child.seatIndex = seatIndex;
        child.decision = 'P';
        child.outcome = Outcome::HitContinue;   // chained sibling: a normal decision node, not a marker
        child.cards = hc;
        Hand ch(hc);
        child.handValue = ch.getValue();
        child.isSoft = ch.isSoft();
        child.deckCursorAtNode = cursor;
        node.children.push_back(std::move(child));

        walkActiveSeat(node.children.back(), seatIndex, curDealerUp_, nh, cursor, totalHands, pathHands,
                       splitRemaining - 1, familyPairValue, familySuit);
    }
    else if (seatIndex == static_cast<int>(curSeatCards_.size()) - 1)
    {
        buildDealerBranch(node, cursor, pathHands);
    }
    else
    {
        HandInfo nh{curSeatCards_[seatIndex + 1], false, false};
        walkActiveSeat(node, seatIndex + 1, curDealerUp_, nh, cursor, 1, pathHands, 0, 0, Suit::Spades);
    }
}

// Attach the first split-hand node under a split marker (§6): draw its forced card, then play it.
void ActionTree::buildFirstSplitHand(TreeNode& parentNode, int seatIndex, int cursor, int pairValue,
                                     Suit suit, int totalHands, std::vector<HandEntry>& pathHands)
{
    HandInfo nh;
    std::vector<Card> hc;
    hc.push_back(cardWithBJ(pairValue, suit));
    Card forced{};   // guard shoe bounds (exhausted -> sentinel card)
    if (cursor < static_cast<int>(shoe_.size()))
    {
        forced = shoe_[cursor];
        ++cursor;
    }
    hc.push_back(forced);
    nh.cards = hc;
    nh.split = true;
    nh.splitAce = (pairValue == 11);

    TreeNode child;
    child.nodeId = nextNodeId_++;
    child.seatIndex = seatIndex;
    child.decision = 'P';
    child.outcome = Outcome::SplitSiblingStart;
    child.cards = hc;
    Hand ch(hc);
    child.handValue = ch.getValue();
    child.isSoft = ch.isSoft();
    child.deckCursorAtNode = cursor;
    parentNode.children.push_back(std::move(child));

    walkActiveSeat(parentNode.children.back(), seatIndex, curDealerUp_, nh, cursor, totalHands, pathHands,
                   totalHands - 1, pairValue, suit);
}

// Dealer completion (§4): draw while value < 17 or (==17 && soft) [H17], then settle every seat's
// final hand on this path. Attaches the deterministic chain as children of `terminalNode`, ending in
// one DealerResult leaf carrying per-seat results indexed by seat position.
void ActionTree::buildDealerBranch(TreeNode& terminalNode, int cursor, const std::vector<HandEntry>& pathHands)
{
    int c = cursor;
    std::vector<Card> dealerCards;
    // The two cards the dealer already holds from the initial deal: the face-up upcard and the face-down
    // hole card. Both sit inside the initial-deal window (cursor starts just past them), so reading the
    // hole here does NOT consume a new card -- it only makes the recorded dealer hand match Game's.
    dealerCards.push_back(curDealerUp_);
    const int holeIdx = curDeckStart_ + 2 * seatCount_ + 1;
    if (holeIdx < static_cast<int>(shoe_.size()))
        dealerCards.push_back(shoe_[holeIdx]);   // reveal hole card

    while (dealerCards.size() < 8)           // guard against an empty shoe mid-chain
    {
        Hand d(dealerCards);
        const bool shouldHit = (d.getValue() < Constants::DEALER_STAND ||
                                (d.getValue() == Constants::DEALER_STAND && d.isSoft()));
        if (!shouldHit || c >= static_cast<int>(shoe_.size()))
            break;
        dealerCards.push_back(shoe_[c++]);
    }

    // Attach intermediate draw nodes, then the single DealerResult leaf.
    TreeNode* current = &terminalNode;
    for (std::size_t i = 0; i < dealerCards.size(); ++i)
    {
        TreeNode dn;
        dn.nodeId = nextNodeId_++;
        dn.seatIndex = -1;
        dn.isDealerNode = true;
        dn.decision = 'X';
        std::vector<Card> prefix(dealerCards.begin(), dealerCards.begin() + static_cast<std::size_t>(i + 1));
        dn.cards = prefix;
        Hand dh(dn.cards);
        dn.handValue = dh.getValue();
        dn.isSoft = dh.isSoft();
        dn.deckCursorAtNode = c;
        dn.outcome = (i + 1 < dealerCards.size()) ? Outcome::HitContinue : Outcome::DealerResult;
        current->children.push_back(std::move(dn));
        current = &current->children.back();
    }

    Hand finalD(current->cards);
    const bool dealerBust = finalD.isBust();
    const bool dealerBlackjack = finalD.size() == 2 && finalD.isBlackjack();
    const int dealerValue = finalD.getValue();

    std::vector<int> seatResults(curSeatCards_.size(), PUSH);
    // Aggregate every final hand of each seat together (§5): a split seat wins if any of its hands won and
    // none lost. Group by seat index so all of a seat's hands are settled as one unit against the real
    // finished dealer hand (dealerValue/dealerBust/dealerBlackjack computed above).
    for (int s = 0; s < static_cast<int>(curSeatCards_.size()); ++s)
    {
        std::vector<HandEntry> seatHands;
        for (const HandEntry& e : pathHands)
            if (e.seatIndex == s)
                seatHands.push_back(e);
        seatResults[s] = aggregateSeatResult(seatHands, dealerValue, dealerBust, dealerBlackjack);
    }

    current->seatResults = seatResults;
}

// Aggregate a single seat's final hands into one round result code (§5 seatResults). For the common
// single-hand seat this is exact; multi-hand (split) seats win if any hand won and none lost.
int ActionTree::aggregateSeatResult(const std::vector<HandEntry>& hands, int dealerValue, bool dealerBust,
                                    bool dealerHasBlackjack) const
{
    bool hasWin = false, hasLoss = false, allPush = true;
    for (const HandEntry& e : hands)
    {
        Hand h(e.cards);
        // Ground settlement in the actual finished dealer hand (§4): thread the real dealer value /
        // bust / blackjack through so each seat's outcome matches what a real Game would settle. The
        // surrendered flag comes from the stored HandEntry (set when the surrender action was taken).
        const int r = settleResult(e.cards, h.isSplit(), e.surrendered, dealerValue, dealerBust, dealerHasBlackjack);
        if (r == WIN || r == BLACKJACK) hasWin = true;
        else if (r == LOSS) hasLoss = true;
        if (r != PUSH) allPush = false;
    }
    if (allPush) return PUSH;
    if (hasWin && !hasLoss) return WIN;
    if (hasLoss && !hasWin) return LOSS;
    return hasWin ? WIN : LOSS;
}


// Play one seat's hands along the basic-strategy spine, mirroring Game::npcTakeTurn / RoundEngine:
// repeat the prescribed action until each hand finishes; a split moves one pair card into a new hand
// (appended last) and both size-1 hands receive forced cards as they become active. Returns the cursor.
int ActionTree::playSeat(std::vector<HandInfo>& hands, int cursor)
{
    std::size_t i = 0;
    while (i < hands.size())
    {
        HandInfo& cur = hands[i];

        // RoundEngine: deal one forced card to a size-1 active hand.
        if (cur.cards.size() == 1)
        {
            if (cursor >= static_cast<int>(shoe_.size()))
            {
                ++i;   // shoe exhausted before the forced card: finish this incomplete split hand
                continue;
            }
            cur.cards.push_back(shoe_[cursor++]);
            const bool resplitEligible = cur.splitAce && isPairOf(cur.cards) &&
                                         cur.cards[0].getBlackjackValue() == 11 &&
                                         hands.size() < Constants::MAX_SPLIT_HANDS &&
                                         Constants::RESPLIT_ACES_ALLOWED;
            if (cur.splitAce && !resplitEligible)   // split ace stands after one card unless re-splittable
            {
                ++i;
                continue;
            }
        }

        Hand h(cur.cards);
        const int val = h.getValue();
        const bool isPair = isPairOf(cur.cards);
        const int pv = isPair ? cur.cards[0].getBlackjackValue() : 0;

        if (cur.cards.size() == 2 && !cur.split && h.isBlackjack())   // natural blackjack auto-resolves
        {
            ++i;
            continue;
        }

        char rec = BasicStrategy::getRecommendedAction(val, curDealerUp_.getBlackjackValue(), h.isSoft(),
                                                       isPair, pv);

        // Mirror Game's npc fallback: an unavailable split/double/surrender collapses to a hit.
        if (rec == 'P' && !(cur.cards.size() == 2 && isPair && hands.size() < Constants::MAX_SPLIT_HANDS &&
                            !(cur.split && pv == 10 && !Constants::RESPLIT_TENS_ALLOWED)))
            rec = 'H';
        else if (rec == 'D' && !(cur.cards.size() == 2 && !cur.splitAce))
            rec = 'H';
        else if (rec == 'R' && cur.cards.size() != 2)
            rec = 'H';

        if (rec == 'P')
        {
            Card moved = cur.cards.back();
            cur.cards.pop_back();   // active hand drops to size 1; gets its forced card next iteration
            HandInfo nh;
            nh.cards.push_back(moved);
            nh.split = true;
            nh.splitAce = (moved.getBlackjackValue() == 11);
            hands.push_back(nh);    // appended last, played after the active hand
        }
        else if (rec == 'H')
        {
            if (cursor >= static_cast<int>(shoe_.size()))
            {
                ++i;   // shoe exhausted: cannot hit, finish at current value
                continue;
            }
            cur.cards.push_back(shoe_[cursor++]);
            Hand h2(cur.cards);
            if (h2.isBust() || h2.getValue() == 21)
                ++i;
        }
        else   // S, D or R: finish the hand (double draws one forced card if the shoe has it)
        {
            if (rec == 'D' && cursor < static_cast<int>(shoe_.size()))
                cur.cards.push_back(shoe_[cursor++]);
            ++i;
        }
    }
    return cursor;
}

// §6 buildRound next-chunk seed: play one full round along the basic-strategy spine, returning where
// real resolution ended (so the following chunk starts exactly on the true tail of the shoe).
int ActionTree::simulateSpineCards(int start)
{
    const int n = seatCount_;
    curDealerUp_ = shoe_[start + n];
    int cursor = start + 2 * n + 2;   // advance over the initial-deal cards (same order/counts as Game::deal)

    for (int s = 0; s < n; ++s)
    {
        std::vector<HandInfo> hands;
        hands.push_back({initialSeatCards(start, s), false, false});
        cursor = playSeat(hands, cursor);
    }

    // Dealer: draw while below 17 or on soft 17 (H17).
    std::vector<Card> dc{curDealerUp_};
    if (cursor < static_cast<int>(shoe_.size()))
        dc.push_back(shoe_[cursor++]);
    while (dc.size() < 8)
    {
        Hand d(dc);
        const bool shouldHit = (d.getValue() < Constants::DEALER_STAND ||
                                (d.getValue() == Constants::DEALER_STAND && d.isSoft()));
        if (!shouldHit || cursor >= static_cast<int>(shoe_.size()))
            break;
        dc.push_back(shoe_[cursor++]);
    }
    return cursor;
}

// Build one chunk: seed the current-chunk context, create the DealRoot node (seat 0's fixed starting
// pair), and enumerate its full action tree (§6).
void ActionTree::buildRound(Chunk& chunk)
{
    const int start = chunk.deckStartCursor;
    curDeckStart_ = start;
    curDealerUp_ = shoe_[start + seatCount_];

    curSeatCards_.clear();
    for (int i = 0; i < seatCount_; ++i)
        curSeatCards_.push_back(initialSeatCards(start, i));

    TreeNode& root = chunk.root;
    root.nodeId = nextNodeId_++;
    root.seatIndex = 0;
    root.isDealerNode = false;
    root.decision = 'D';
    root.outcome = Outcome::DealRoot;
    std::vector<Card> firstHand = curSeatCards_[0];
    root.cards = firstHand;
    Hand rf(firstHand);
    root.handValue = rf.getValue();
    root.isSoft = rf.isSoft();
    root.deckCursorAtNode = start;

    std::vector<HandEntry> pathHands;
    HandInfo fh{firstHand, false, false};
    walkActiveSeat(root, 0, curDealerUp_, fh, start + 2 * seatCount_ + 2, 1, pathHands, 0, 0, Suit::Spades);
}


// Whole shoe = forest of chunk-trees (§1.6). Each chunk is seeded on the immutable tail beginning at
// where the previous round's actual (basic-strategy) resolution ended, so outcomes correspond to real
// rounds. A full initial deal (`2*seats + 2` cards) must remain for a round to be emitted.
std::vector<Chunk> ActionTree::buildShoeForest()
{
    chunks_.clear();
    nextNodeId_ = 0;

    const int dealCards = 2 * seatCount_ + 2;
    int cursor = 0;
    int roundIndex = 0;

    while (cursor + dealCards <= static_cast<int>(shoe_.size()))
    {
        Chunk chunk;
        chunk.roundIndex = roundIndex;
        chunk.deckStartCursor = cursor;
        for (int i = 0; i < seatCount_; ++i)
        {
            SeatInfo si;
            si.index = i;
            si.name = seatName(i);
            si.startCards = initialSeatCards(cursor, i);
            chunk.seats.push_back(std::move(si));
        }

        buildRound(chunk);
        chunks_.push_back(std::move(chunk));
        cursor = simulateSpineCards(cursor);   // next chunk begins exactly where real play left off
        ++roundIndex;
    }
    return chunks_;
}

// ---------------------------------------------------------------------------
// Flat pre-order text — PRIMARY save format (§7a). One pipe-delimited record
// per node in DFS pre-order; nChildren is the explicit delimiter a parser uses
// to recurse exactly that many child records.
// ---------------------------------------------------------------------------
std::string ActionTree::record(const TreeNode& node) const
{
    std::ostringstream os;
    std::string cardsCsv;
    for (std::size_t i = 0; i < node.cards.size(); ++i)
        cardsCsv += std::to_string(node.cards[i].getBlackjackValue()) +
                    (i + 1 < node.cards.size() ? "," : "");

    std::string results;
    if (node.outcome == Outcome::DealerResult && !node.seatResults.empty())
    {
        for (std::size_t i = 0; i < node.seatResults.size(); ++i)
            results += "s" + std::to_string(i) + "=" + resultName(node.seatResults[i]) +
                       (i + 1 < node.seatResults.size() ? "," : "");
    }

    os << node.nodeId << "|" << node.seatIndex << "|" << (node.isDealerNode ? 1 : 0) << "|"
       << decisionLabel(node.decision, node.outcome) << "|" << (node.isBasicStrategy ? 1 : 0) << "|"
       << node.handValue << "|" << (node.isSoft ? 1 : 0) << "|" << cardsCsv << "|" << node.deckCursorAtNode
       << "|" << node.children.size() << "|" << static_cast<int>(node.outcome) << "|" << results;
    return os.str();
}

void ActionTree::emitFlat(const TreeNode& node, std::string& out) const
{
    out += record(node);
    out += '\n';   // one record per line so parseFlatNode's line-based recursion matches emit order
    for (const TreeNode& child : node.children)
        emitFlat(child, out);
}

std::string ActionTree::toFlatText() const
{
    std::string out;
    out += "FOREST " + std::to_string(chunks_.size()) + "\n";
    for (const Chunk& chunk : chunks_)
    {
        out += "CHUNK " + std::to_string(chunk.roundIndex) + " " + std::to_string(chunk.deckStartCursor) + "\n";
        emitFlat(chunk.root, out);
    }
    return out;
}

void ActionTree::parseFlatNode(std::vector<std::string>::const_iterator& it,
                               std::vector<std::string>::const_iterator end, TreeNode& node)
{
    const std::string& line = *it;
    ++it;   // consume this record (caller recurses using its nChildren field)

    size_t pos = 0;
    auto nextField = [&]() {
        size_t pipe = line.find('|', pos);
        std::string f = line.substr(pos, (pipe == std::string::npos ? line.size() : pipe) - pos);
        pos = (pipe == std::string::npos ? line.size() : pipe + 1);
        return f;
    };

    TreeNode n;
    n.nodeId = std::stoi(nextField());
    n.seatIndex = std::stoi(nextField());
    n.isDealerNode = (std::stoi(nextField()) != 0);
    const std::string label = nextField();
    n.decision = label.empty() ? 'D' : label[0];
    n.isBasicStrategy = (std::stoi(nextField()) != 0);
    n.handValue = std::stoi(nextField());
    n.isSoft = (std::stoi(nextField()) != 0);

    const std::string cardsCsv = nextField();
    if (!cardsCsv.empty())
    {
        size_t cpos = 0;
        while (cpos < cardsCsv.size())
        {
            size_t comma = cardsCsv.find(',', cpos);
            int bj = std::stoi(cardsCsv.substr(cpos, (comma == std::string::npos ? cardsCsv.size() : comma) - cpos));
            cpos = (comma == std::string::npos ? cardsCsv.size() : comma + 1);
            n.cards.push_back(cardWithBJ(bj, Suit::Spades));
        }
    }

    n.deckCursorAtNode = std::stoi(nextField());
    const int nChildren = std::stoi(nextField());
    n.outcome = static_cast<Outcome>(std::stoi(nextField()));

    const std::string results = nextField();   // s0=WIN,s1=LOSS,...
    if (!results.empty())
    {
        size_t rpos = 0;
        while (rpos < results.size())
        {
            size_t comma = results.find(',', rpos);
            std::string tok = results.substr(rpos, (comma == std::string::npos ? results.size() : comma) - rpos);
            rpos = (comma == std::string::npos ? results.size() : comma + 1);
            size_t eq = tok.find('=');
            // Record format is "s{seatIdx}={result}" (see record()); the seat index sits between the
            // leading 's' and '='. Fall back to a bare "{seatIdx}=" token if ever emitted without 's'.
            const bool hasSPrefix = !tok.empty() && tok[0] == 's';
            const size_t s = hasSPrefix ? 1 : 0;
            int seatIdx = std::stoi(tok.substr(s, eq - s));
            int code = (tok.substr(eq + 1) == "WIN") ? WIN : (tok.substr(eq + 1) == "LOSS") ? LOSS
                                 : (tok.substr(eq + 1) == "PUSH") ? PUSH
                                 : (tok.substr(eq + 1) == "SURRENDER") ? SURRENDER : BLACKJACK;
            if (seatIdx >= 0)
            {
                while (n.seatResults.size() <= static_cast<std::size_t>(seatIdx))
                    n.seatResults.push_back(PUSH);   // dense vector; default outcome is a push
                n.seatResults[seatIdx] = code;
            }
        }
    }

    node = std::move(n);
    for (int i = 0; i < nChildren; ++i)
        parseFlatNode(it, end, node.children.emplace_back());
}

// Reconstruct a forest from flat text (§7a). Serialization is chunk-based, so the immutable shoe is not
// required to round-trip; only chunks_ are rebuilt. Used by tests to prove byte-identical round-trips.
ActionTree ActionTree::fromFlatText(const std::string& text)
{
    ActionTree t({});          // serialization reads chunks_; shoe_ unused for the round-trip
    t.chunks_.clear();

    std::vector<std::string> lines;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.find_first_not_of(" \t") != std::string::npos)
            lines.push_back(line);
    }

    if (lines.empty())
        return t;

    // Skip the "FOREST <n>" header.
    size_t pos = 0;
    const std::string& first = lines[pos];
    size_t sp = first.find(' ');
    if (first.substr(0, sp) != "FOREST" || pos + 1 >= lines.size())
        return t;   // malformed header: empty forest
    const int chunkCount = std::stoi(first.substr(sp + 1));
    ++pos;   // advance past the "FOREST <n>" header so lines[pos] is the first CHUNK header

    for (int c = 0; c < chunkCount; ++c)
    {
        if (pos + 1 >= lines.size())
            break;
        Chunk chunk;

        // CHUNK header line at lines[pos]: "CHUNK <roundIndex> <deckStartCursor>".
        const std::string& hdr = lines[pos];
        size_t s1 = hdr.find(' '), s2 = hdr.find(' ', s1 + 1);
        if (s1 == std::string::npos)
            break;
        chunk.roundIndex = std::stoi(s2 == std::string::npos ? hdr.substr(s1 + 1)
                                                             : hdr.substr(s1 + 1, (s2 - s1 - 1)));
        const std::string rest = (s2 == std::string::npos) ? hdr.substr(s1 + 1) : hdr.substr(s2 + 1);
        size_t sp2 = rest.find(' ');
        chunk.deckStartCursor = std::stoi(sp2 == std::string::npos ? rest : rest.substr(0, sp2));
        ++pos;   // consume the CHUNK header line

        TreeNode root;
        std::vector<std::string>::const_iterator it = lines.begin() + pos;
        std::vector<std::string>::const_iterator end = lines.end();
        t.parseFlatNode(it, end, root);   // consumes this node and its entire subtree (one record per line)
        pos = static_cast<size_t>(it - lines.begin());

        chunk.root = std::move(root);
        t.chunks_.push_back(std::move(chunk));
    }
    return t;
}



// Count one subtree for summarize() (file-scope helper).
void countNode(const TreeNode& n, int depth, std::size_t& nodes, std::size_t& leaves, int& maxDepth)
{
    ++nodes;
    if (n.children.empty())
        ++leaves;
    if (depth > maxDepth)
        maxDepth = depth;
    for (const TreeNode& c : n.children)
        countNode(c, depth + 1, nodes, leaves, maxDepth);
}

// Aggregate node / leaf / max-depth counts across the whole forest (§8.3).
void ActionTree::summarize(std::size_t& outNodes, std::size_t& outLeaves, int& outMaxDepth) const
{
    std::size_t nodes = 0, leaves = 0;
    int maxDepth = 0;
    for (const Chunk& chunk : chunks_)
        countNode(chunk.root, 0, nodes, leaves, maxDepth);
    outNodes = nodes;
    outLeaves = leaves;
    outMaxDepth = maxDepth;
}

// Follow the basic-strategy edges from each chunk's root (§5/§8). At a split where splitting is optimal,
// several sibling edges are marked basic; we take the first deterministically so the spine stays linear.
std::vector<ActionTree::Spine> ActionTree::extractBasicStrategySpines() const
{
    std::vector<Spine> spines;
    for (const Chunk& chunk : chunks_)
    {
        Spine s;
        s.roundIndex = chunk.roundIndex;

        const TreeNode* cur = &chunk.root;
        while (cur != nullptr)
        {
            s.nodeIds.push_back(cur->nodeId);
            const TreeNode* next = nullptr;
            for (const TreeNode& child : cur->children)
                if (child.isBasicStrategy)
                {
                    next = &child;
                    break;
                }
            cur = next;
        }
        spines.push_back(std::move(s));
    }
    return spines;
}


// Node count of a subtree (for the maxDepth preview).
int subtreeNodeCount(const TreeNode& n)
{
    int c = 1;
    for (const TreeNode& ch : n.children)
        c += subtreeNodeCount(ch);
    return c;
}

// Human-readable single-line label (§7c): seat/hand/value, edge letter with a trailing * when the
// basic-strategy line, and an outcome/result suffix on terminal nodes.
std::string nodeLabel(const TreeNode& n)
{
    std::ostringstream os;
    if (n.outcome == Outcome::DealRoot)
        return "ROOT deal";

    std::string cardsCsv;
    for (std::size_t i = 0; i < n.cards.size(); ++i)
        cardsCsv += std::to_string(n.cards[i].getBlackjackValue()) + (i + 1 < n.cards.size() ? "," : "");

    if (n.isDealerNode)
        os << "[dealer] " << cardsCsv << " (" << n.handValue << (n.isSoft ? " soft" : "") << ") | edge=X";
    else
    {
        os << "[seat " << n.seatIndex << "] " << cardsCsv << " (" << n.handValue
           << (n.isSoft ? " soft" : "") << ") | edge=" << decisionLabel(n.decision, n.outcome);
        if (n.isBasicStrategy)
            os << "*";
    }

    switch (n.outcome)
    {
    case Outcome::Bust:          os << " => bust " << n.handValue; break;
    case Outcome::TwentyOne:     os << " => 21"; break;
    case Outcome::Blackjack:     os << " => BLACKJACK"; break;
    case Outcome::Surrender:     os << " => SURRENDER"; break;
    case Outcome::DoubleEnd:     os << " => DOUBLE " << n.handValue; break;
    case Outcome::Stand:         os << " => STAND"; break;
    case Outcome::SplitSiblingStart:
        os << " => SPLIT HAND";
        break;
    case Outcome::DealerResult:
        for (std::size_t i = 0; i < n.seatResults.size(); ++i)
            os << " s" << i << "=" << resultName(n.seatResults[i]);
        break;
    default: break;
    }
    return os.str();
}

// ASCII tree renderer (§7c). `indentStr` is the connector chain of ancestors; root is drawn with no
// leading connector. A trailing * marks a basic-strategy edge.
void ActionTree::emitAscii(const TreeNode& node, const std::string& indentStr, int depth, int maxDepth,
                           bool last, bool hasParent, std::string& out) const
{
    if (maxDepth != -1 && depth > maxDepth)
    {
        out += indentStr + "…(" + std::to_string(subtreeNodeCount(node) - 1) + " more nodes omitted)\n";
        return;
    }

    std::string connector = hasParent ? (last ? "\u2514\u2500 " : "\u251c\u2500 ") : "";   // └─ / ├─
    out += indentStr + connector + nodeLabel(node) + "\n";

    std::string childIndent;
    if (!hasParent)
        childIndent = "";                                    // root's children are top-level items
    else
        childIndent = indentStr + (last ? "    " : "\u2502   ");   // │ continuation or blank

    for (std::size_t i = 0; i < node.children.size(); ++i)
        emitAscii(node.children[i], childIndent, depth + 1, maxDepth, i + 1 == node.children.size(), true, out);
}

std::string ActionTree::toAscii(int maxDepth) const
{
    std::ostringstream out;
    for (std::size_t i = 0; i < chunks_.size(); ++i)
    {
        const Chunk& c = chunks_[i];
        std::ostringstream hdr;
        hdr << "== Chunk " << i << "  round=" << c.roundIndex << "  cursor=" << c.deckStartCursor
            << "  seats=" << c.seats.size() << " ==\n";
        std::string body;
        emitAscii(c.root, "", 0, maxDepth, true, false, body);
        out << hdr.str() << body;
    }
    return out.str();
}


static const char* outcomeName(Outcome o)
{
    switch (o)
    {
    case Outcome::DealRoot:         return "DealRoot";
    case Outcome::HitContinue:      return "HitContinue";
    case Outcome::Stand:            return "Stand";
    case Outcome::Bust:             return "Bust";
    case Outcome::TwentyOne:        return "TwentyOne";
    case Outcome::Blackjack:        return "Blackjack";
    case Outcome::Surrender:        return "Surrender";
    case Outcome::DoubleEnd:        return "DoubleEnd";
    case Outcome::SplitSiblingStart:return "SplitSiblingStart";
    case Outcome::DealerResult:     return "DealerResult";
    }
    return "HitContinue";
}

std::string jsonEscape(const std::string& s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '"' || c == '\\')
            out += '\\';
        out += c;
    }
    return out;
}

// Serialize one node as a nested JSON object mirroring TreeNode (§7b). Field indent = `ind`; children
// array elements sit one level deeper. Returns the full object text (caller composes arrays/commas).
void ActionTree::writeNodeJson(const TreeNode& node, int ind, std::string& out) const
{
    const auto pad = [](int n) { return std::string(static_cast<std::size_t>(n) * 2, ' '); };
    const std::string p1 = pad(ind + 1);   // field indent / array-element indent
    const std::string p0 = pad(ind);

    out += p0 + "{\n";

    auto addField = [&](const std::string& k, const std::string& v) { out += p1 + "\"" + k + "\": " + v + ",\n"; };
    addField("nodeId", std::to_string(node.nodeId));
    addField("seatIndex", std::to_string(node.seatIndex));
    addField("isDealerNode", node.isDealerNode ? "true" : "false");
    addField("decision", "\"" + jsonEscape(decisionLabel(node.decision, node.outcome)) + "\"");
    addField("isBasicStrategy", node.isBasicStrategy ? "true" : "false");

    std::string cards = "[";
    for (std::size_t i = 0; i < node.cards.size(); ++i)
        cards += std::to_string(node.cards[i].getBlackjackValue()) + (i + 1 < node.cards.size() ? "," : "");
    cards += "]";
    addField("cards", cards);

    addField("handValue", std::to_string(node.handValue));
    addField("isSoft", node.isSoft ? "true" : "false");
    addField("outcome", std::string("\"") + outcomeName(node.outcome) + "\"");
    addField("deckCursorAtNode", std::to_string(node.deckCursorAtNode));

    if (node.outcome == Outcome::DealerResult && !node.seatResults.empty())
    {
        std::string sr = "[";
        for (std::size_t i = 0; i < node.seatResults.size(); ++i)
            sr += std::to_string(node.seatResults[i]) + (i + 1 < node.seatResults.size() ? "," : "");
        sr += "]";
        addField("seatResults", sr);
    }

    // children — last element, no trailing comma.
    out += p1 + "\"children\": ";
    if (node.children.empty())
    {
        out += "]\n" + p0 + "}";
        return;
    }

    std::vector<std::string> items;
    for (const TreeNode& child : node.children)
    {
        std::string cs;
        writeNodeJson(child, ind + 1, cs);   // object ends with "}\n"
        if (!cs.empty() && cs.back() == '\n')
            cs.pop_back();
        items.push_back(cs);
    }
    out += "[\n";
    for (std::size_t i = 0; i < items.size(); ++i)
        out += p1 + items[i] + (i + 1 < items.size() ? "," : "") + "\n";
    out += p0 + "]\n" + p0 + "}";
}

std::string ActionTree::toJson() const
{
    std::ostringstream out;
    out << "{\n  \"shoe\": {\"decks\": " << Constants::NUM_DECKS << " },\n";
    out << "  \"chunks\": [";
    if (chunks_.empty())
    {
        out << "]\n}\n";
        return out.str();
    }
    out << "\n";
    for (std::size_t i = 0; i < chunks_.size(); ++i)
    {
        const Chunk& c = chunks_[i];
        std::ostringstream cs;
        cs << "    {\n"
           << "      \"roundIndex\": " << c.roundIndex << ",\n"
           << "      \"deckStartCursor\": " << c.deckStartCursor << ",\n"
           << "      \"seats\": [";
        for (std::size_t s = 0; s < c.seats.size(); ++s)
        {
            cs << (s ? "," : "") << "\n        {\n"
               << "          \"index\": " << c.seats[s].index << ",\n"
               << "          \"name\": \"" << jsonEscape(c.seats[s].name) << "\",\n"
               << "          \"startCards\": [";
            for (std::size_t k = 0; k < c.seats[s].startCards.size(); ++k)
                cs << (k ? "," : "") << c.seats[s].startCards[k].getBlackjackValue();
            cs << "]\n        }";
            cs << "\n      ],\n";
        }

        std::string rootJson;
        writeNodeJson(c.root, 5, rootJson);   // root object indented inside the chunk at level 5
        cs << "      \"root\": " << rootJson << "\n    }";
        out << (i ? "," : "") << "\n" << cs.str();
    }
    out << "\n  ]\n}\n";
    return out.str();
}


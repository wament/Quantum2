#include <catch2/catch_all.hpp>
#include <Player.hpp>
#include <Constants.hpp>
#include <Hand.hpp>
#include <BasicStrategy.hpp>

#include <filesystem>   // U8 test: hermetic HOME redirect so load() returns a fresh profile
#include <cstdlib>      // setenv/unsetenv/getenv for the HOME redirect

using namespace Constants;

static Hand makeHand(std::initializer_list<int> values, int bet = 0)
{
    Card card;
    Hand hand;
    for (int value : values)
    {
        card.value = value;
        card.suit = Suit::Spades;
        hand.addCard(card);
    }
    if (bet > 0)
        hand.setBet(bet);
    return hand;
}

TEST_CASE("Player settles an even-money win", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({7, 8}, 20), /*dealerValue*/ 13, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Win);
    CHECK(payout == 40);
    CHECK(p.getBankroll() == 520);
}

TEST_CASE("Player settles a loss against a higher dealer total", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({7, 8}, 20), /*dealerValue*/ 17, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Lose);
    CHECK(payout == 0);
    CHECK(p.getBankroll() == 480);
}

TEST_CASE("Player settles a bust as a loss regardless of the dealer", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({8, 8, 8}, 20), /*dealerValue*/ 18, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Lose);
    CHECK(payout == 0);
    CHECK(p.getBankroll() == 480);
}

TEST_CASE("Player settles a dealer bust as a win", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({7, 8}, 20), /*dealerValue*/ 22, /*dealerBust*/ true, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Win);
    CHECK(payout == 40);
    CHECK(p.getBankroll() == 520);
}

TEST_CASE("Player settles a push with the bankroll unchanged", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({10, 5}, 20), /*dealerValue*/ 15, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Push);
    CHECK(payout == 20);
    CHECK(p.getBankroll() == 500);
}

TEST_CASE("Player settles a natural blackjack at 3:2", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    int payout = 0;
    SettlementResult result = p.settle(makeHand({1, 10}, 20), /*dealerValue*/ 20, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Win);
    CHECK(payout == 50); // 20 * (1 + 1.5)
    CHECK(p.getBankroll() == 530);
}

TEST_CASE("Player settles a split hand totaling 21 at even money", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    Hand twoOne = makeHand({1, 10}, 20);
    twoOne.setSplit(true, 11); // makes it a split hand, so not a natural blackjack

    int payout = 0;
    SettlementResult result = p.settle(twoOne, /*dealerValue*/ 20, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Win);
    CHECK(payout == 40); // even money, not 3:2
    CHECK(p.getBankroll() == 520);
}

TEST_CASE("Player settles a surrendered hand losing half the bet", "[Player][Settlement]")
{
    Player p("Test");
    p.setBankroll(500);

    Hand surrendered = makeHand({7, 8}, 20);
    surrendered.setSurrendered(true);

    int payout = 0;
    SettlementResult result = p.settle(surrendered, /*dealerValue*/ 5, /*dealerBust*/ false, /*dealerHasBlackjack*/ false, payout);

    CHECK(result == SettlementResult::Surrender);
    CHECK(payout == 10); // half the bet returned
    CHECK(p.getBankroll() == 490); // half the bet lost
}

TEST_CASE("shouldTakeInsurance follows the Hi-Lo +3 rule", "[Player][Insurance]")
{
    Player p("Test");

    CHECK(p.shouldTakeInsurance(20, 3.5f) == true);
    CHECK(p.shouldTakeInsurance(20, 3.0f) == true);
    CHECK(p.shouldTakeInsurance(20, 2.9f) == false);
    CHECK(p.shouldTakeInsurance(0, 5.0f) == false);
}

TEST_CASE("shouldTakeInsurance follows the Reko +2 rule", "[Player][Insurance]")
{
    // Reko's deviation threshold is +2 (lower than Hi-Lo's +3), so a true count between 2 and 3 that would
    // not trigger insurance under Hi-Lo does here.
    Player p("Test");

    CHECK(p.shouldTakeInsurance(20, 2.0f, BasicStrategy::CountingStyle::REKO) == true);
    CHECK(p.shouldTakeInsurance(20, 2.5f, BasicStrategy::CountingStyle::REKO) == true);
    CHECK(p.shouldTakeInsurance(20, 1.9f, BasicStrategy::CountingStyle::REKO) == false);
    CHECK(p.shouldTakeInsurance(0, 5.0f, BasicStrategy::CountingStyle::REKO) == false);

    // Sanity: the same counts under Hi-Lo would still decline (threshold +3).
    CHECK(p.shouldTakeInsurance(20, 2.0f, BasicStrategy::CountingStyle::HI_LO) == false);
}

TEST_CASE("takeInsurance places the wager and records it on the hand", "[Player][Insurance]")
{
    Player p("Test");
    p.setBankroll(500);
    p.resetRound(20);

    p.takeInsurance(10);
    CHECK(p.hands[0].getInsuranceBet() == 10);
    CHECK(p.getBankroll() == 500); // placeBet is a reservation, not an immediate deduction

    // A zero wager is a no-op.
    p.takeInsurance(0);
    CHECK(p.hands[0].getInsuranceBet() == 10);
}

TEST_CASE("raise/lower insurance are clamped to half the main bet and zero", "[Player][Insurance]")
{
    Player p("Test");
    p.resetRound(20); // main bet 20 -> max insurance 10

    for (int i = 0; i < 12; ++i)
        p.raiseInsurance();
    CHECK(p.hands[0].getInsuranceBet() == 10); // clamped at half the bet

    for (int i = 0; i < 12; ++i)
        p.lowerInsurance();
    CHECK(p.hands[0].getInsuranceBet() == 0); // clamped at zero
}

TEST_CASE("adjustHandBet clamps to table limits and defaults to the active hand", "[Player][Betting]")
{
    Player p("Test");
    p.resetRound(10);

    p.adjustHandBet(5);
    CHECK(p.hands[0].getBet() == 15);

    p.adjustHandBet(-50);
    CHECK(p.hands[0].getBet() == MINIMUM_BET);

    p.adjustHandBet(999999);
    CHECK(p.hands[0].getBet() == MAXIMUM_BET);

    // An out-of-range hand index falls back to the active (only) hand; the result then clamps.
    p.adjustHandBet(5, /*handIndex*/ 999);
    CHECK(p.hands[0].getBet() == MAXIMUM_BET);
}

TEST_CASE("determineRoundBet sizes Kelly off the true count and bankroll", "[Player][Betting]")
{
    Player p("Test");
    p.setBankroll(10000);
    p.setBettingStrategy(BettingStrategy::Kelly, 10);

    CHECK(p.determineRoundBet(5.0f) == BasicStrategy::calculateKellyBet(5.0f, GameConfig::KELLY_FRACTION, 10000, MINIMUM_BET, MAXIMUM_BET));
    CHECK(p.determineRoundBet(1.0f) == MINIMUM_BET); // below the Kelly threshold returns min bet
}

TEST_CASE("determineRoundBet sizes progressive strategies off internal state", "[Player][Betting]")
{
    Player p("Test");
    p.setBettingStrategy(BettingStrategy::Martingale, 10);

    p.recordRoundResult(true); // a prior win -> Martingale doubles the base bet
    CHECK(p.determineRoundBet(0.0f) == 20);

    p.recordRoundResult(false); // a prior loss -> Martingale resets to the base bet
    CHECK(p.determineRoundBet(0.0f) == 10);

    Player flat("Flat");
    flat.setBettingStrategy(BettingStrategy::Flat, 25);
    CHECK(flat.determineRoundBet(9.9f) == 25);
}

// U8 parity: a brand-new / recovered human profile seeds the standard starting bankroll (GameConfig::STARTING_BANKROLL) so it
// matches an NPC seat -- not the stale $1000 default that had humans starting with 1/10th of every opponent's stack. HOME is
// redirected to an isolated temp dir so load() returns a fresh profile, exactly like the B7 test; original HOME is restored.
TEST_CASE("U8: a brand-new human player starts at the same bankroll as an NPC seat", "[Player][Bankroll]")
{
    const std::filesystem::path home = std::filesystem::temp_directory_path() / "bj_u8_test";
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
    std::filesystem::create_directories(home);

    const char* originalHome = std::getenv("HOME");
    setenv("HOME", home.string().c_str(), 1);
    try {
        Player human("William");                              // fresh profile (empty name) -> seeded to STARTING_BANKROLL
        Player npc("Alice", GameConfig::STARTING_BANKROLL);   // NPC seat at the standard stack
        CHECK(human.getBankroll() == GameConfig::STARTING_BANKROLL);
        CHECK(npc.getBankroll() == GameConfig::STARTING_BANKROLL);
        CHECK(human.getBankroll() == npc.getBankroll());      // no more 10x human-vs-NPC gap
    } catch (...) {
        if (originalHome) setenv("HOME", originalHome, 1); else unsetenv("HOME");
        std::filesystem::remove_all(home, ec);
        throw;
    }
    if (originalHome) setenv("HOME", originalHome, 1); else unsetenv("HOME");
    std::filesystem::remove_all(home, ec);
}
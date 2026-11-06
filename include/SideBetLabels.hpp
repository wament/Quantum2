#pragma once

#include <string>

// Pure label builder for the "Side Bet: <kind> $<wager><RESULT>" readout shown during DealerTurn/Cleanup. Kept free of
// any SFML type so it can be unit-tested headlessly and reused by GameRenderer without pulling graphics into a test TU.
// `wager` is the placed stake; `payout` is what resolveSideBets returned (0 on a loss). net = payout - wager, i.e. the
// signed change to the bankroll -- but this first copy mirrors the renderer's existing logic exactly so the regression
// test fails here before the fix:

namespace sidebet {

    inline std::string resultLabel(int wager, int payout)
    {
        const int net = payout - wager; // gross payout - stake == net on the bankroll
        const int mag = net < 0 ? -net : net; // |net| for display
        // Branch three ways: a zero net is a push, not a win. The sign is preserved so WON shows a plus and LOST a minus.
        return (net > 0 ? "  WON +" : net < 0 ? "  LOST -" : "  PUSH  ") + std::to_string(mag);
    }

} // namespace sidebet

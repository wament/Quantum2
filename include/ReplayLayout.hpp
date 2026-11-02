// Pure pixel-geometry helpers for the replay history list. Shared by Replay (hover hit-testing +
// scroll queries) and ReplayView (drawing)
// row can never drift out of sync with the rows that are hoverable/clickable. Header-only, no SFML
// runtime types -> fully headless-testable and usable behind the Graphics link without rendering.

#pragma once

namespace ReplayLayout {

inline constexpr float kRowTopStart = 120.f; // y above which the title/instructions header sits
inline constexpr float kRowHeight = 40.f;    // height of each history-list row in window pixels
inline constexpr float kListLeftX = 50.f;    // left edge of the selectable row band
inline constexpr float kListRightX = 1250.f; // right edge of the selectable row band

// Top pixel of list row `rowIndex` at `scrollOffset`, or -1 if it would clip into the header.
// Mirrors the original per-row formula exactly: topStart + i*height - scroll*height.
inline float historyRowTop(int rowIndex, int scrollOffset)
{
    float top = kRowTopStart + static_cast<float>(rowIndex) * kRowHeight -
                static_cast<float>(scrollOffset) * kRowHeight;
    return top < kRowTopStart ? -1.f : top;
}

// Whether `x` lies inside the selectable row band (hover/click hit-test).
inline bool inSelectableXBand(float x)
{
    return x >= kListLeftX && x <= kListRightX;
}

// Number of history-list rows that fit below the header in a window `windowHeight` pixels tall.
// Mirrors the original: floor((windowHeight - topStart - height) / height) + 1, or 0 when it does
// not even clear the header.
inline int visibleHistoryRows(int windowHeight)
{
    if (windowHeight <= static_cast<int>(kRowTopStart + kRowHeight))
        return 0;
    return static_cast<int>((static_cast<float>(windowHeight) - kRowTopStart - kRowHeight) /
                            kRowHeight) +
           1;
}

} // namespace ReplayLayout

#pragma once

#include <array>

// Pure model for the "Review History" browser's source switch. Kept free of any SFML type so it can be
// unit-tested headlessly -- Replay takes a window/texture/font in its constructor, so nothing inside it
// is reachable from a test TU (the same reason ReplayLayout.hpp exists). GameRenderer/ReplayView consume
// these and the tests lock them here.
namespace historybrowser
{
    // Which recorded history the list is showing. The shoe-mode split keeps Daily and tournament shoes
    // OUT of career longHistory (career stats are derived from it), so a browser that reads
    // longHistory alone can never show them -- the three vectors need three views. Career is the
    // default because it is what "Review History" has always listed.
    enum class Source
    {
        Career = 0,
        Daily,
        Tournament
    };

    constexpr int kCount = 3;

    // Which of the three histories currently have shoes in them, indexed by Source.
    using Populated = std::array<bool, kCount>;

    // Display name for the title strip. ASCII only -- it reaches sf::Text (U13: SFML 3 decodes
    // std::string as ANSI, so a non-ASCII literal arrives as mojibake).
    inline const char* label(Source source)
    {
        switch (source)
        {
        case Source::Daily:
            return "Daily Challenge";
        case Source::Tournament:
            return "Tournament";
        case Source::Career:
            break;
        }
        return "Career";
    }

    // The next NON-EMPTY source after `current`, cycling Career -> Daily -> Tournament -> Career.
    // Skipping empties is the point: the three histories are populated by three different session
    // kinds, so "exactly one is non-empty" is the common case, and cycling onto a zero-row list reads
    // as a broken feature. When no OTHER source has shoes this returns `current` unchanged, which
    // makes cycling a no-op rather than a flicker.
    inline Source nextNonEmpty(Source current, const std::array<bool, kCount>& nonEmpty)
    {
        for (int step = 1; step <= kCount; ++step)
        {
            const Source candidate = static_cast<Source>((static_cast<int>(current) + step) % kCount);
            if (nonEmpty[static_cast<int>(candidate)])
                return candidate;
        }
        return current;
    }

    // The source to OPEN on: `requested` if it has shoes, otherwise the first non-empty one in
    // Career-first order, otherwise Career (an empty list, which is the honest answer).
    inline Source preferredOpen(Source requested, const std::array<bool, kCount>& nonEmpty)
    {
        if (nonEmpty[static_cast<int>(requested)])
            return requested;
        for (int i = 0; i < kCount; ++i)
            if (nonEmpty[static_cast<std::size_t>(i)])
                return static_cast<Source>(i);
        return Source::Career;
    }

} // namespace historybrowser

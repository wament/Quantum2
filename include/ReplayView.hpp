#pragma once

// include/ReplayView.hpp
// View layer for replay playback, extracted from Replay.cpp (Phase R3). Holds the visual
// resources (window / card texture / font) and a reference to the owning Replay so it can read
// the engine's step/state data. The rendering methods are defined in src/ReplayView.cpp; Replay
// grants friendship (`friend class ReplayView`) so this direct member access compiles identically.

#include <SFML/Graphics.hpp>

namespace ReplayInternals
{
    struct ReplayState;
}

class Replay;

class ReplayView
{
public:
    // Resources come straight from the owning Replay (which received them from Game).
    ReplayView(sf::RenderWindow &window, sf::Texture &cardTexture, sf::Font &font, Replay &replay);

    void drawHistoryList();
    void drawReplayView();
    void drawTable(const ReplayInternals::ReplayState &state);
    void drawControls(const ReplayInternals::ReplayState &state);
    void drawAnalysisOverlay(const ReplayInternals::ReplayState &state);
    void drawRoundSlider(const ReplayInternals::ReplayState &state);
    void drawStepSummary(const ReplayInternals::ReplayState &state);

private:
    sf::RenderWindow &window;
    sf::Texture &cardTexture;
    sf::Font &font;
    Replay &replay;
};

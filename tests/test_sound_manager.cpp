#include <catch2/catch_all.hpp>
#include "test_support.hpp"

#include "Game.hpp"
#include "PlayerProfile.hpp"
#include "SoundManager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Feature 7: Sound Effects & Music. All three layers are device-free so they run headless in ctest:
//   [Sound][Logic]    Game event -> SoundManager queue + volume defaults/setters (pure logic, no audio device).
//   [Sound][Assets]   SoundManager decodes the shipped WAV buffers and queues effect requests.
//   [Sound][Persistence] profile sound prefs are additive, so a pre-feature-7 JSON still loads with defaults.

TEST_CASE("Game plays bet + card effects through the headless queue", "[Sound][Logic]")
{
    Game game(true); // headless: no window/assets/audio device; playSound() only enqueues

    // Deterministic low-value shoe so the human's first two cards total 5 (never a natural) and the dealer up-card is
    // not an ace -> roundState stays PlayerTurn after startTestRound, so hit() is valid.
    std::vector<Card> shoe = {
        Card{2, Suit::Spades}, Card{3, Suit::Hearts}, Card{4, Suit::Diamonds}, Card{5, Suit::Clubs},
        Card{6, Suit::Spades}, Card{7, Suit::Hearts}, Card{8, Suit::Diamonds}, Card{9, Suit::Clubs},
        Card{10, Suit::Spades}, Card{2, Suit::Hearts}, Card{3, Suit::Diamonds}, Card{4, Suit::Clubs}
    };
    test_support::seed_deck(game,shoe);

    REQUIRE(game.soundTotalPlayedForTest() == 0);
    REQUIRE(game.soundVolumeForTest() == Constants::Sound::DEFAULT_SOUND_VOLUME);
    REQUIRE(game.musicVolumeForTest() == Constants::Sound::DEFAULT_MUSIC_VOLUME);
    REQUIRE(game.soundMutedForTest() == false);

    int cardsBefore = game.soundTotalPlayedForTest();
    game.startTestRound(); // deals initial cards from the injected shoe -> 'card' events
    REQUIRE(game.soundTotalPlayedForTest() > cardsBefore);
    const std::vector<std::string> q0 = game.soundQueueForTest();
    REQUIRE(std::count(q0.begin(), q0.end(), "card") >= 1);

    // Placing a bet enqueues exactly one 'bet' effect. The real game also deals the hands at this point (each draw
    // plays its own 'card'), so we assert on the count of 'bet' events rather than queue order.
    const int betsBefore = static_cast<int>(std::count(q0.begin(), q0.end(), "bet"));
    game.confirmBet();
    const std::vector<std::string> q1 = game.soundQueueForTest();
    REQUIRE(std::count(q1.begin(), q1.end(), "bet") == betsBefore + 1);

    if (game.roundStateForTest() == RoundState::PlayerTurn && !game.testHand(0, 0).isFinished())
    {
        const int beforeHit = game.soundTotalPlayedForTest();
        game.hit(); // a drawn card -> exactly one 'card' event
        REQUIRE(game.soundTotalPlayedForTest() == beforeHit + 1);
    }

    // Volume setters clamp to SFML's 0..100 range.
    game.setSoundVolumeForTest(250.f);
    REQUIRE(game.soundVolumeForTest() == 100.f);
    game.setMusicVolumeForTest(-40.f);
    REQUIRE(game.musicVolumeForTest() == 0.f);
    game.setMutedForTest(true);
    REQUIRE(game.soundMutedForTest() == true);
}

TEST_CASE("SoundManager queues known events, rejects unknown ones and counts them", "[Sound][Logic]")
{
    SoundManager sm;

    REQUIRE(sm.playSound("card"));
    REQUIRE(sm.playSound("card"));
    REQUIRE(sm.playSound("bet"));
    REQUIRE(sm.pendingSounds().size() == 3);
    REQUIRE(sm.pendingSounds().front() == "card");
    REQUIRE(sm.pendingSounds().back() == "bet");
    REQUIRE(sm.totalSoundsPlayed() == 3);

    REQUIRE_FALSE(sm.playSound("does-not-exist")); // unknown event is rejected, does not enqueue
    REQUIRE(sm.pendingSounds().size() == 3);

    // The shipped effect vocabulary is stable (card/bet/chip/blackjack/win/lose).
    REQUIRE(SoundManager::knownEvents().size() == 6);
    for (const auto& e : {"card", "bet", "chip", "blackjack", "win", "lose"})
        REQUIRE(std::find(SoundManager::knownEvents().begin(), SoundManager::knownEvents().end(), e) !=
                SoundManager::knownEvents().end());

    // Mute flips the flag but keeps the queue predictable (request is still recorded).
    sm.setMuted(true);
    REQUIRE(sm.isMuted());
    REQUIRE(sm.playSound("chip"));
    REQUIRE(sm.pendingSounds().size() == 4);
    sm.setMuted(false);
    REQUIRE_FALSE(sm.isMuted());

    // Volume clamping.
    sm.setSoundVolume(150.f);
    REQUIRE(sm.settings().soundVolume == 100.f);
    sm.setSoundVolume(-5.f);
    REQUIRE(sm.settings().soundVolume == 0.f);
    sm.setMusicVolume(33.f);
    REQUIRE(sm.settings().musicVolume == 33.f);
}

TEST_CASE("SoundManager decodes the shipped WAV assets", "[Sound][Assets]")
{
    // Relative to the working directory, matching test_assets.cpp (ctest runs from the project root).
    SoundManager sm;
    sm.registerAssetPaths("assets/sounds/", "assets/music/");

    REQUIRE(sm.loadAssets()); // every known effect buffer must decode
    for (const auto& event : SoundManager::knownEvents())
        REQUIRE(sm.isEventReady(event));
}

TEST_CASE("Sound settings are additive; old profiles still load", "[Sound][Persistence]")
{
    const fs::path tmp = fs::temp_directory_path() / "blackjack2_sound_profile_test.json";

    // 1) A profile saved WITH sound prefs round-trips exactly.
    {
        PlayerProfile p("William");
        p.preferredSoundVolume = 42;
        p.preferredMusicVolume = 88;
        p.preferredMuted = true;
        p.save(tmp.string());
    }
    {
        const PlayerProfile loaded = PlayerProfile::load(tmp.string());
        REQUIRE(loaded.preferredSoundVolume == 42);
        REQUIRE(loaded.preferredMusicVolume == 88);
        REQUIRE(loaded.preferredMuted == true);
    }

    // 2) A pre-feature-7 profile (sound keys absent) still loads with sane defaults.
    {
        PlayerProfile p("William");
        p.preferredSoundVolume = 42;
        p.preferredMusicVolume = 88;
        p.preferredMuted = true;
        p.save(tmp.string());

        std::ifstream in(tmp);
        std::stringstream ss;
        ss << in.rdbuf();
        json j = json::parse(ss.str());
        j.erase("preferredSoundVolume");
        j.erase("preferredMusicVolume");
        j.erase("preferredMuted");
        std::ofstream out(tmp, std::ios::trunc);
        out << j.dump(2);
    }
    {
        const PlayerProfile loaded = PlayerProfile::load(tmp.string());
        REQUIRE(loaded.preferredSoundVolume == 100); // default when absent (additive schema)
        REQUIRE(loaded.preferredMusicVolume == 60);
        REQUIRE(loaded.preferredMuted == false);
    }

    fs::remove(tmp);
}

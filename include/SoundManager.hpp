#pragma once

#include <SFML/Audio.hpp>
#include <memory>
#include <string>
#include <vector>

// Sound & music manager (Feature 7: Sound Effects & Music).
//
// Responsibility split on purpose so the *logic* is headless-safe while real audio stays behind a gated path:
//   - Pure logic (Settings + event queue + play counters) never touches an audio device. Headless tests drive it
//     entirely through playSound()/settings and read back queue state, so no display or sound card is required.
//   - All SFML audio objects live in the pImpl (see SoundManager.cpp). They are created lazily by loadAssets(),
//     which the headless Game constructor deliberately skips. sf::SoundBuffer decode reads files only and needs no
//     device, so even the asset-loading test stays safe in a headless CI environment.
//
// Known effect events: card, bet, chip, blackjack, win, lose. Music track: theme (see Constants::Sound).
class SoundManager
{
  public:
    struct Settings
    {
        float soundVolume = 100.f; // SFML units, 0..100
        float musicVolume = 60.f;  // SFML units, 0..100
        bool muted = false;
    };

    SoundManager();
    ~SoundManager();

    // Register the on-disk asset directories (relative to the working directory). Call once before loadAssets().
    void registerAssetPaths(const std::string& soundDir, const std::string& musicDir);

    // Decode every registered effect buffer and open the music clip. Headless-safe: file decode only, no device.
    // Returns true when all known effect buffers loaded (music-open failures are tolerated with a warning).
    bool loadAssets();

    bool assetsLoaded() const;

    // Queue + emit a named sound effect. Always records the request in the queue + counter regardless of whether
    // audio is loaded or muted, so headless tests get deterministic state. Playback happens immediately from decoded
    // buffers only when loaded and unmuted (see loadAssets / setMuted). Returns false for unknown events.
    bool playSound(const std::string& event);

    // Queue + start the music track by name. Enqueues into the music queue + counter; playback starts from the
    // opened sf::Music once assets are loaded. Returns false for an unknown track.
    bool playMusic(const std::string& track);

    const Settings& settings() const { return settings_; }
    bool isMuted() const { return settings_.muted; }

    void setSettings(const Settings& s);
    void setSoundVolume(float v);
    void setMusicVolume(float v);
    // Mute stops any live playback; future plays are still queued so the queue state stays predictable.
    void setMuted(bool m);

    // ---- Test accessors: pure logic, headless-safe (no device). ----
    size_t pendingSoundCount() const { return pendingQueue_.size(); }
    const std::vector<std::string>& pendingSounds() const { return pendingQueue_; }
    int totalSoundsPlayed() const { return totalSounds_; }

    // Number of music-play requests queued (excludes effect events).
    int totalMusicPlays() const { return totalMusicPlays_; }

    static const std::vector<std::string>& knownEvents();
    // True once this specific effect's buffer has been decoded by loadAssets().
    bool isEventReady(const std::string& event) const;

  private:
    static float clamp0100(float v)
    {
        if (v < 0.f) return 0.f;
        if (v > 100.f) return 100.f;
        return v;
    }

    // pImpl owns ALL SFML audio state (buffers, sf::Sound, sf::Music). It is forward-declared here so SoundManager's
    // header stays free of deep SFML types; the unique_ptr only needs a complete type at destruction, which happens
    // in SoundManager.cpp where Impl is fully defined.
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Settings settings_;
    std::vector<std::string> pendingQueue_; // ordered list of requested effect events
    std::vector<std::string> pendingMusic_; // ordered list of requested music tracks
    int totalSounds_ = 0;
    int totalMusicPlays_ = 0;
};

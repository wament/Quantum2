#include "SoundManager.hpp"

#include "Constants.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

// All SFML audio state lives here, behind the pImpl. sf::Sound has no default constructor (only a buffer ctor), so
// the sound/music objects are stored as unique_ptrs and created lazily by loadAssets() from their real constructors.
struct SoundManager::Impl
{
    std::unordered_map<std::string, sf::SoundBuffer> soundBuffers_;
    std::string soundDir_;
    std::string musicDir_;
    bool loaded = false;

    std::unique_ptr<sf::Music> music_;      // background music (streamed)
    std::unique_ptr<sf::Sound> playback_;   // reusable effect player, setBuffer()'d per event

    void applySettings(const SoundManager::Settings& s)
    {
        if (s.muted)
        {
            if (playback_) playback_->stop();
            if (music_) music_->stop();
        }
        if (playback_) playback_->setVolume(s.soundVolume);
        if (music_) music_->setVolume(s.musicVolume);
    }

    void setSoundVolume(float v) { if (playback_) playback_->setVolume(v); }
    void setMusicVolume(float v) { if (music_) music_->setVolume(v); }

    bool isEventReady(const std::string& event) const
    {
        return soundBuffers_.find(event) != soundBuffers_.end();
    }
};

const std::vector<std::string>& SoundManager::knownEvents()
{
    static const std::vector<std::string> events = {"card", "bet", "chip", "blackjack", "win", "lose"};
    return events;
}

SoundManager::SoundManager() : impl_(std::make_unique<Impl>()) {}
SoundManager::~SoundManager() = default;

void SoundManager::registerAssetPaths(const std::string& soundDir, const std::string& musicDir)
{
    if (impl_)
    {
        impl_->soundDir_ = soundDir;
        impl_->musicDir_ = musicDir;
    }
}
bool SoundManager::loadAssets()
{
    if (!impl_)
        return false;
    impl_->loaded = false;

    int loaded = 0;
    const int total = static_cast<int>(knownEvents().size());
    for (const auto& event : knownEvents())
    {
        const std::string path = impl_->soundDir_ + event + ".wav";
        if (!fs::exists(path))
        {
            Logger::warn() << "SoundManager: asset missing, skipping '" << event << "' (" << path << ")";
            continue;
        }
        sf::SoundBuffer buffer;
        if (buffer.loadFromFile(path))
        {
            impl_->soundBuffers_[event] = std::move(buffer);
            ++loaded;
        }
        else
        {
            Logger::warn() << "SoundManager: failed to decode '" << event << "' (" << path << ")";
        }
    }

    // Music is best-effort: a missing clip disables background music but must not disable sound effects. sf::Music has
    // a public default constructor + openFromFile(...) returning bool
    const std::string musicPath = impl_->musicDir_ + Constants::Sound::MUSIC_TRACK + ".wav";
    if (fs::exists(musicPath))
    {
        impl_->music_ = std::make_unique<sf::Music>();
        if (!impl_->music_->openFromFile(musicPath))
            Logger::warn() << "SoundManager: failed to open music (" << musicPath << ")";
    }
    else
    {
        Logger::warn() << "SoundManager: music file missing (" << musicPath << ")";
    }

    impl_->loaded = (loaded == total);
    if (!impl_->loaded)
        Logger::warn() << "SoundManager: only " << loaded << "/" << total << " effect buffers loaded";
    else
        Logger::info("SoundManager: all sound effects + music loaded.");
    return impl_->loaded;
}

bool SoundManager::playSound(const std::string& event)
{
    bool known = false;
    for (const auto& e : knownEvents())
        if (e == event)
        {
            known = true;
            break;
        }
    if (!known)
        return false;

    // Record the request so tests/UX can observe queue state even when audio is muted or not yet loaded.
    pendingQueue_.push_back(event);
    ++totalSounds_;

    if (impl_ && impl_->loaded && !settings_.muted)
    {
        auto it = impl_->soundBuffers_.find(event);
        if (it != impl_->soundBuffers_.end())
        {
            // Reuse one sf::Sound: construct from a buffer the first time, then re-point via setBuffer().
            if (!impl_->playback_)
                impl_->playback_ = std::make_unique<sf::Sound>(it->second);
            else
                impl_->playback_->setBuffer(it->second);
            impl_->playback_->setVolume(settings_.soundVolume);
            impl_->playback_->play();
        }
    }
    return true;
}

bool SoundManager::playMusic(const std::string& track)
{
    if (track != Constants::Sound::MUSIC_TRACK)
        return false;
    pendingMusic_.push_back(track);
    ++totalMusicPlays_;
    if (impl_ && impl_->loaded && !settings_.muted &&
        impl_->music_ && impl_->music_->getStatus() != sf::SoundSource::Status::Playing)
        impl_->music_->play();
    return true;
}

bool SoundManager::isEventReady(const std::string& event) const
{
    return impl_ && impl_->isEventReady(event);
}

void SoundManager::setSettings(const Settings& s)
{
    settings_ = s;
    if (impl_)
        impl_->applySettings(settings_);
}

void SoundManager::setSoundVolume(float v)
{
    settings_.soundVolume = clamp0100(v);
    if (impl_)
        impl_->setSoundVolume(settings_.soundVolume);
}

void SoundManager::setMusicVolume(float v)
{
    settings_.musicVolume = clamp0100(v);
    if (impl_)
        impl_->setMusicVolume(settings_.musicVolume);
}

void SoundManager::setMuted(bool m)
{
    settings_.muted = m;
    if (impl_)
        impl_->applySettings(settings_);
}

bool SoundManager::assetsLoaded() const
{
    return impl_ && impl_->loaded;
}

#include "FontManager.hpp"
#include "Logger.hpp"

sf::Font FontManager::s_font;
bool FontManager::s_loaded = false;
std::string FontManager::s_fontPath;

bool FontManager::load(const std::string& primaryPath, const std::string& fallbackPath)
{
    if (s_loaded)
        return true;

    s_fontPath = primaryPath;
    s_loaded = s_font.openFromFile(primaryPath);

    if (s_loaded)
    {
        Logger::info() << "Font loaded: " << primaryPath;
    }
    else if (!fallbackPath.empty())
    {
        Logger::warn() << "Primary font failed, trying fallback: " << fallbackPath;
        s_fontPath = fallbackPath;
        s_loaded = s_font.openFromFile(fallbackPath);
        if (s_loaded)
        {
            Logger::info() << "Fallback font loaded: " << fallbackPath;
        }
    }

    return s_loaded;
}

sf::Font &FontManager::get()
{
    if (!s_loaded)
    {
        throw std::runtime_error("FontManager::get() called before load() or load failed.");
    }
    return s_font;
} 
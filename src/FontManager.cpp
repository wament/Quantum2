#include "FontManager.hpp"
#include <iostream>

sf::Font FontManager::s_font;
bool FontManager::s_loaded = false;
bool FontManager::s_hasUnicode = false;
std::string FontManager::s_fontPath;

bool FontManager::load(const std::string& primaryPath, const std::string& fallbackPath)
{
    if (s_loaded)
        return true;

    s_fontPath = primaryPath;
    s_loaded = s_font.openFromFile(primaryPath);

    if (s_loaded)
    {
        std::cout << "Font loaded: " << primaryPath << std::endl;
    }
    else if (!fallbackPath.empty())
    {
        std::cout << "Primary font failed, trying fallback: " << fallbackPath << std::endl;
        s_fontPath = fallbackPath;
        s_loaded = s_font.openFromFile(fallbackPath);
        if (s_loaded)
        {
            std::cout << "Fallback font loaded: " << fallbackPath << std::endl;
        }
    }

    // Noto Sans has full Unicode coverage; RobotoFlex does not
    s_hasUnicode = (s_fontPath.find("NotoSans") != std::string::npos);

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

const sf::Font &FontManager::getConst()
{
    return get();
}

bool FontManager::hasUnicodeSupport()
{
    return s_hasUnicode;
}

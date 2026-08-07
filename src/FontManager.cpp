#include "FontManager.hpp"

sf::Font FontManager::s_font;
bool     FontManager::s_loaded = false;

bool FontManager::load(const std::string& path)
{
    if (s_loaded) return true;
    s_loaded = s_font.loadFromFile(path);
    return s_loaded;
}

sf::Font& FontManager::get()
{
    if (!s_loaded)
    {
        throw std::runtime_error("FontManager::get() called before load() or load failed.");
    }
    return s_font;
}

const sf::Font& FontManager::getConst()
{
    return get();
}

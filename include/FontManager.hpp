#pragma once

#include <SFML/Graphics.hpp>
#include <stdexcept>

class FontManager
{
public:
    // Load once at startup
    static bool load(const std::string& path);

    // Get reference after load()
    static sf::Font& get();

    // Throws if not loaded
    static const sf::Font& getConst();

private:
    static sf::Font s_font;
    static bool     s_loaded;
};
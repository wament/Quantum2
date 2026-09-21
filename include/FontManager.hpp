#pragma once

#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <string>

class FontManager
{
public:
    // Load with fallback: tries primary first, then fallback if it fails
    static bool load(const std::string& primaryPath, const std::string& fallbackPath = "");

    // Get reference after load()
    static sf::Font& get();

    // Throws if not loaded
    static const sf::Font& getConst();

    // Check if the loaded font supports Unicode (Noto Sans)
    static bool hasUnicodeSupport();

private:
    static sf::Font s_font;
    static bool     s_loaded;
    static bool     s_hasUnicode;
    static std::string s_fontPath;
};
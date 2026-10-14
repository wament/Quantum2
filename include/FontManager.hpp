#pragma once

#include <SFML/Graphics.hpp>
#include <stdexcept>
#include <string>

class FontManager
{
public: 
    static bool load(const std::string& primaryPath, const std::string& fallbackPath = "");
    static sf::Font& get(); 

private:
    static sf::Font s_font;
    static bool     s_loaded; 
    static std::string s_fontPath;
};
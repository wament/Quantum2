#include <catch2/catch_all.hpp>
#include <SFML/Graphics.hpp>
#include <string>

// Test that all required asset files are loadable

TEST_CASE("Font loads correctly", "[Assets]")
{
    sf::Font font;
    REQUIRE(font.openFromFile("assets/fonts/roboto/RobotoFlex-VariableFont.ttf"));
}

TEST_CASE("Background texture loads correctly", "[Assets]")
{
    sf::Texture texture;
    REQUIRE(texture.loadFromFile("assets/images/felt.png"));
}

TEST_CASE("Card texture loads correctly", "[Assets]")
{
    sf::Texture texture;
    REQUIRE(texture.loadFromFile("assets/images/cardsLarge_tilemap_packed.png"));
}

TEST_CASE("All assets load without error", "[Assets]")
{
    // Font
    sf::Font font;
    REQUIRE(font.openFromFile("assets/fonts/roboto/RobotoFlex-VariableFont.ttf"));

    // Background
    sf::Texture bgTexture;
    REQUIRE(bgTexture.loadFromFile("assets/images/felt.png"));

    // Card texture
    sf::Texture cardTexture;
    REQUIRE(cardTexture.loadFromFile("assets/images/cardsLarge_tilemap_packed.png"));
}
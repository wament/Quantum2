#include <catch2/catch_all.hpp>

#include "Game.hpp"
#include "InputHandler.hpp"
#include <array>

// Main-menu arrow-key navigation must reach every option. The menu has nine rows (PlayGame -> PlayerCount ->
// GameSpeed -> ReviewHistory -> StrategyTrainer -> Tournament -> Achievements -> GameRules -> Statistics). This
// exercises the pure navigation logic in InputHandler::navigateMenu() headlessly -- no window, deck or deal needed --
// and pins the reported fix that Achievements / Custom Game Rules / Statistics were unreachable via Up/Down.
TEST_CASE("Main menu arrow keys reach every option in order", "[MenuNavigation][MainMenu]")
{
    Game game(true); // headless
    InputHandler input(game);

    const std::array<MenuOption, 9> expected = {
        MenuOption::PlayGame,       MenuOption::PlayerCount,      MenuOption::GameSpeed,
        MenuOption::ReviewHistory,  MenuOption::StrategyTrainer,  MenuOption::Tournament,
        MenuOption::Achievements,   MenuOption::GameRules,        MenuOption::Statistics};

    // Forward (Down) from PlayGame must visit all nine options in order and wrap back to the top.
    game.setMenuSelectionForTest(MenuOption::PlayGame);
    for (int step = 1; step <= static_cast<int>(expected.size()); ++step)
    {
        input.navigateMenu(true); // Down
        REQUIRE(game.menuSelectionForTest() == expected[step % static_cast<int>(expected.size())]);
    }

    // The reported bug: after Tournament, Down must land on Achievements (not wrap to the top).
    game.setMenuSelectionForTest(MenuOption::Tournament);
    input.navigateMenu(true);
    REQUIRE(game.menuSelectionForTest() == MenuOption::Achievements);
    input.navigateMenu(true);
    REQUIRE(game.menuSelectionForTest() == MenuOption::GameRules);
    input.navigateMenu(true);
    REQUIRE(game.menuSelectionForTest() == MenuOption::Statistics);

    // Backward (Up) from the first option must wrap to the last option (Statistics), then step back through all rows.
    game.setMenuSelectionForTest(MenuOption::PlayGame);
    input.navigateMenu(false); // Up -> wraps to last
    REQUIRE(game.menuSelectionForTest() == MenuOption::Statistics);

    for (int step = 1; step <= static_cast<int>(expected.size()); ++step)
    {
        input.navigateMenu(false); // Up
        int index = ((static_cast<int>(expected.size()) - 1) - step) % static_cast<int>(expected.size());
        index = (index + static_cast<int>(expected.size())) % static_cast<int>(expected.size());
        REQUIRE(game.menuSelectionForTest() == expected[index]);
    }
}

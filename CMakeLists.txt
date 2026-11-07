cmake_minimum_required(VERSION 3.10)
project(Blackjack2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

add_executable(Blackjack2)

# Include directories
target_include_directories(Blackjack2 PRIVATE include)

# ==================== SFML Configuration ====================

# Detect build environment
if(DEFINED ENV{GITHUB_ACTIONS})
    message(STATUS "=== GitHub Actions build detected ===")
    set(GITHUB_BUILD ON)
elseif(CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    message(STATUS "=== vcpkg toolchain detected (local) ===")
    set(GITHUB_BUILD OFF)
else()
    message(STATUS "=== Standard local build (no vcpkg) ===")
    set(GITHUB_BUILD OFF)
endif()

if(GITHUB_BUILD OR CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    # --- vcpkg-based build (GitHub Actions or local vcpkg) ---

    # vcpkg defaults to dynamic linking; don't force static
    set(SFML_STATIC_LIBRARIES OFF CACHE BOOL "Use static SFML" FORCE)
    message(STATUS "SFML_STATIC_LIBRARIES = OFF (vcpkg dynamic)")

    # Find SFML via vcpkg (toolchain handles paths)
    # Use minimum version requirement instead of exact match for flexibility
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System Audio Network)
    if(NOT SFML_FOUND)
        message(FATAL_ERROR "SFML not found via vcpkg toolchain. Check vcpkg install step.")
    endif()

    message(STATUS "SFML version: ${SFML_VERSION}")
    message(STATUS "SFML libraries: ${SFML_LIBRARIES}")
    target_link_libraries(Blackjack2 PRIVATE
        SFML::Graphics SFML::Window SFML::System SFML::Audio SFML::Network
    )

elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # --- Cross-compiling to Windows with MinGW (manual SFML_ROOT) ---

    message(STATUS "Cross-compiling for Windows with MinGW")
    option(SFML_STATIC_LIBRARIES "Use static SFML libraries" ON)

    if(NOT DEFINED SFML_ROOT)
        message(FATAL_ERROR "SFML_ROOT must be set when cross-compiling with MinGW")
    endif()

    target_include_directories(Blackjack2 PRIVATE ${SFML_ROOT}/include)
    target_link_directories(Blackjack2 PRIVATE ${SFML_ROOT}/lib)
    target_link_libraries(Blackjack2 PRIVATE
        sfml-graphics sfml-window sfml-system sfml-audio sfml-network
    )

else()
    # --- Standard local build (Linux/macOS/Windows native) ---

    option(SFML_STATIC_LIBRARIES "Use static SFML libraries" ON)
    message(STATUS "SFML_STATIC_LIBRARIES = ${SFML_STATIC_LIBRARIES}")

    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics Window System Audio Network)
    target_link_libraries(Blackjack2 PRIVATE
        SFML::Graphics SFML::Window SFML::System SFML::Audio SFML::Network
    )
endif()

# ==================== Sources ====================

# Sources
target_sources(Blackjack2 PRIVATE
    main.cpp
    src/Card.cpp
    src/Deck.cpp
    src/Game.cpp
    src/BettingHandler.cpp
    src/BasicStrategy.cpp
    src/Graphics.cpp
    src/Hand.cpp
    src/Player.cpp
    src/PlayerProfile.cpp
    src/HUD.cpp
    src/StatBox.cpp
    src/FontManager.cpp
    src/Replay.cpp
    src/ReplayView.cpp
    src/GameRenderer.cpp
    src/InputHandler.cpp
    src/Logger.cpp
    src/SoundManager.cpp
    src/SideBet.cpp
    src/StrategyTrainer.cpp
    src/Tournament.cpp
    src/Achievement.cpp
    src/GameRules.cpp
    src/StatsDashboard.cpp
    src/AchievementFeeder.cpp
    src/HandEvaluator.cpp
    src/RoundDealer.cpp
    src/RoundEngine.cpp
    src/SessionLifecycle.cpp
)

# Copy assets to build output (for local testing)
add_custom_command(TARGET Blackjack2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Blackjack2>/assets
)

# ==================== Testing ====================

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)

FetchContent_MakeAvailable(Catch2)

add_executable(Blackjack2Tests
    src/Card.cpp
    src/Deck.cpp
    src/Game.cpp
    src/BettingHandler.cpp
    src/BasicStrategy.cpp
    src/Graphics.cpp
    src/Hand.cpp
    src/Player.cpp
    src/PlayerProfile.cpp
    src/HUD.cpp
    src/StatBox.cpp
    src/FontManager.cpp
    src/Replay.cpp
    src/ReplayView.cpp
    src/GameRenderer.cpp
    src/InputHandler.cpp
    src/Logger.cpp
    src/SoundManager.cpp
    src/SideBet.cpp
    src/StrategyTrainer.cpp
    src/Tournament.cpp
    src/Achievement.cpp
    src/GameRules.cpp
    src/StatsDashboard.cpp
    src/AchievementFeeder.cpp
    src/HandEvaluator.cpp
    src/RoundDealer.cpp
    src/RoundEngine.cpp
    src/SessionLifecycle.cpp
    tests/test_card.cpp
    tests/test_deck.cpp
    tests/test_hand.cpp
    tests/test_game.cpp
    tests/test_player.cpp
    tests/test_assets.cpp
    tests/test_reko_counting.cpp
    tests/test_advisor_feedback.cpp
    tests/test_betting_handler.cpp
    tests/test_settlement.cpp
    tests/test_side_bets.cpp
    tests/test_analytics.cpp
    tests/test_stats_batching.cpp
    tests/test_deal_timing.cpp
    tests/test_achievements.cpp
    tests/test_rendering.cpp
    tests/test_replay_layout.cpp
    tests/test_session_lifecycle.cpp
    tests/test_hud_layout.cpp
    tests/test_profile_settings.cpp
    tests/test_sound_manager.cpp
    tests/test_strategy_trainer.cpp
    tests/test_tournament_mode.cpp
    tests/test_game_rules.cpp
    tests/test_menu_navigation.cpp
    tests/test_input_return_to_main_menu.cpp
    tests/test_rules_panel_editing.cpp
    tests/test_logger.cpp
)

target_include_directories(Blackjack2Tests PRIVATE include)
# Test-only shared infrastructure (e.g. test_support.hpp). Kept off include/ so production headers stay clean; only the
# test target needs it, and each test_*.cpp already compiles as its own translation unit.
target_include_directories(Blackjack2Tests PRIVATE tests)

# test_achievements.cpp uses C++20 designated initializers (e.g. recordHand({ .isBlackjack = true })), which MSVC rejects
# under the project-wide C++17 standard with "C7555: use of designated initializers requires at least '/std:c++20'". Those
# constructs are test-only, so upgrade just this target to C++20 and leave the production app (Blackjack2) on C++17.
set_target_properties(Blackjack2Tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# For tests, always use dynamic SFML (simpler with Catch2)
if(GITHUB_BUILD OR CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics)
    # Feature 7: SoundManager.cpp is now linked into the test binary (it uses sf::SoundBuffer/sf::Sound), so the tests
    # target needs SFML::Audio too. Loading buffers/decoding files needs no display or audio device, so headless ctest
    # stays safe. This is the one required deviation from "tests link only Graphics" for this feature.
    target_link_libraries(Blackjack2Tests PRIVATE Catch2::Catch2WithMain SFML::Graphics SFML::Audio)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_include_directories(Blackjack2Tests PRIVATE ${SFML_ROOT}/include)
    target_link_directories(Blackjack2Tests PRIVATE ${SFML_ROOT}/lib)
    target_link_libraries(Blackjack2Tests PRIVATE
        Catch2::Catch2WithMain
        sfml-graphics sfml-window sfml-system
    )
else()
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics)
    # Feature 7: SoundManager.cpp is now linked into the test binary (it uses sf::SoundBuffer/sf::Sound), so the tests
    # target needs SFML::Audio too. Loading buffers/decoding files needs no display or audio device, so headless ctest
    # stays safe. This is the one required deviation from "tests link only Graphics" for this feature.
    target_link_libraries(Blackjack2Tests PRIVATE Catch2::Catch2WithMain SFML::Graphics SFML::Audio)
endif()

include(CTest)
include(Catch)
catch_discover_tests(Blackjack2Tests)


cmake_minimum_required(VERSION 3.10)
project(Blackjack2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

add_executable(Blackjack2)

# Include directories
target_include_directories(Blackjack2 PRIVATE include)

# SFML setup
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Cross-compiling to Windows with MinGW (manual SFML_ROOT)
    message(STATUS "Cross-compiling for Windows with MinGW")
    target_include_directories(Blackjack2 PRIVATE ${SFML_ROOT}/include)
    target_link_directories(Blackjack2 PRIVATE ${SFML_ROOT}/lib)
    # Try SFML 3.0 targets first, fall back to legacy names
    if(TARGET SFML::Graphics)
    target_link_libraries(Blackjack2 PRIVATE
            SFML::Graphics SFML::Window SFML::System SFML::Audio SFML::Network
    )
else()
    target_link_libraries(Blackjack2 PRIVATE
        sfml-graphics sfml-window sfml-system sfml-audio sfml-network
    )
endif()
else()
    # Native build (Linux, macOS, or Windows with vcpkg)
    # SFML 3.0 uses capitalized component names, 2.x uses lowercase
    # Try 3.0 style first
    find_package(SFML QUIET COMPONENTS Graphics Window System Audio Network)
    if(NOT SFML_FOUND)
        find_package(SFML REQUIRED COMPONENTS graphics window system audio network)
    endif()
    # SFML 3.0 uses new target names (SFML::Graphics, etc.)
    # but older versions use legacy names (sfml-graphics, etc.)
    if(TARGET SFML::Graphics)
        target_link_libraries(Blackjack2 PRIVATE
            SFML::Graphics SFML::Window SFML::System SFML::Audio SFML::Network
)
    else()
        target_link_libraries(Blackjack2 PRIVATE
            sfml-graphics sfml-window sfml-system sfml-audio sfml-network
)
    endif()
endif()

# Sources
target_sources(Blackjack2 PRIVATE
    main.cpp
    src/Card.cpp
    src/Deck.cpp
    src/Game.cpp
    src/Graphics.cpp
    src/Hand.cpp
    src/Player.cpp
    src/PlayerProfile.cpp
    src/HUD.cpp
    src/Panel.cpp
    src/StatBox.cpp
    src/FontManager.cpp
    #src/Replay.cpp
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
    src/Hand.cpp
    tests/test_card.cpp
    tests/test_deck.cpp
    tests/test_hand.cpp
)

target_include_directories(Blackjack2Tests PRIVATE include)
target_link_libraries(Blackjack2Tests PRIVATE Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(Blackjack2Tests)



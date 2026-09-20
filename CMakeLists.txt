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
    src/Graphics.cpp
    src/Hand.cpp
    src/Player.cpp
    src/PlayerProfile.cpp
    src/HUD.cpp
    src/Panel.cpp
    src/StatBox.cpp
    src/FontManager.cpp
    src/Replay.cpp
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
    tests/test_game.cpp
    tests/test_assets.cpp
)

target_include_directories(Blackjack2Tests PRIVATE include)

# For tests, always use dynamic SFML (simpler with Catch2)
if(GITHUB_BUILD OR CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics)
target_link_libraries(Blackjack2Tests PRIVATE Catch2::Catch2WithMain SFML::Graphics)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_include_directories(Blackjack2Tests PRIVATE ${SFML_ROOT}/include)
    target_link_directories(Blackjack2Tests PRIVATE ${SFML_ROOT}/lib)
    target_link_libraries(Blackjack2Tests PRIVATE
        Catch2::Catch2WithMain
        sfml-graphics sfml-window sfml-system
    )
else()
    find_package(SFML 3.0 REQUIRED COMPONENTS Graphics)
    target_link_libraries(Blackjack2Tests PRIVATE Catch2::Catch2WithMain SFML::Graphics)
endif()

include(CTest)
include(Catch)
catch_discover_tests(Blackjack2Tests)


# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ // .exe. // ~~~~~~~~~~~~~~~~~~~~~~~~~~~
cmake_minimum_required(VERSION 3.10)

project(Blackjack2 VERSION 0.1.0 LANGUAGES CXX)

add_executable(Blackjack2)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ // libraries // ~~~~~~~~~~~~~~~~~~~~~~~~~~~
find_package(SFML 2.5 REQUIRED COMPONENTS graphics window system audio network)

target_link_libraries(Blackjack2 PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
    sfml-network
) 

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ // link files // ~~~~~~~~~~~~~~~~~~~~~~~~~~~
target_include_directories(Blackjack2 PRIVATE include)

target_sources(Blackjack2 PRIVATE
    main.cpp
    src/Card.cpp
    src/Deck.cpp
    src/Game.cpp
    src/Graphics.cpp
    src/Hand.cpp
    src/Player.cpp
    src/PlayerProfile.cpp
)
# assets
add_custom_command(TARGET Blackjack2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Blackjack2>/assets
)

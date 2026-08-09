# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ // .exe. // ~~~~~~~~~~~~~~~~~~~~~~~~~~~
cmake_minimum_required(VERSION 3.10)

project(Blackjack2 VERSION 0.1.0 LANGUAGES CXX)

add_executable(Blackjack2)

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~ // libraries // ~~~~~~~~~~~~~~~~~~~~~~~~~~~

if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Cross-compiling to Windows with MinGW
    message(STATUS "Cross-compiling for Windows")
    message(STATUS "SFML_ROOT: ${SFML_ROOT}")

    target_include_directories(Blackjack2 PRIVATE ${SFML_ROOT}/include include)
    target_link_directories(Blackjack2 PRIVATE ${SFML_ROOT}/lib)
target_link_libraries(Blackjack2 PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
    sfml-network
) 

    # Copy SFML DLLs
add_custom_command(TARGET Blackjack2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SFML_ROOT}/bin $<TARGET_FILE_DIR:Blackjack2>/bin
        COMMENT "Copying SFML DLLs to output directory"
)

    # Copy MinGW runtime DLLs (required so users don't need to install dependencies)
    set(MINGW_RUNTIME_DLLS
        libgcc_s_seh-1.dll
        libstdc++-6.dll
        libwinpthread-1.dll
        libatomic-1.dll
        libssp-0.dll
    )

    foreach(DLL ${MINGW_RUNTIME_DLLS})
        if(EXISTS "/usr/lib/gcc/x86_64-w64-mingw32/13-win32/${DLL}")
add_custom_command(TARGET Blackjack2 POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "/usr/lib/gcc/x86_64-w64-mingw32/13-win32/${DLL}"
                    $<TARGET_FILE_DIR:Blackjack2>
                COMMENT "Copying ${DLL} to output directory"
)
        endif()
    endforeach()
else()
    # Native Linux build
    find_package(SFML 2.5 REQUIRED COMPONENTS graphics window system audio network)
    target_link_libraries(Blackjack2 PRIVATE
        sfml-graphics
        sfml-window
        sfml-system
        sfml-audio
        sfml-network
    )
endif()

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
    src/HUD.cpp
    src/Panel.cpp
    src/StatBox.cpp
    src/FontManager.cpp
)

# Copy assets
add_custom_command(TARGET Blackjack2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:Blackjack2>/assets
)


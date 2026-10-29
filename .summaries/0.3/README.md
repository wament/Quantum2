# Blackjack2

A blackjack card game built with C++17 and SFML 3.0. Features multiple players, Kelly criterion betting, basic strategy advice, replay history, and a full HUD.

## Dependencies

- **CMake** 3.10 or newer
- **C++17 compiler** (GCC 13+, Clang 14+, or MSVC 2022)
- **SFML 3.0** (Graphics, Window, System, Audio, Network modules)
- **Catch2** v3.6.0 (auto-fetched by CMake for tests)

### Installing SFML

**Linux (Debian/Ubuntu):**
```bash
sudo apt-get install libsfml-dev
```

**macOS (Homebrew):**
```bash
brew install sfml
```

**Windows (vcpkg):**
```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install sfml:x64-windows
```

## Building

### Standard Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Using CMake Preset (Linux)

A preset is provided for GCC 13.3.0 on Linux:

```bash
cmake --preset "GCC 13.3.0 x86_64-linux-gnu"
cmake --build out/build/"GCC 13.3.0 x86_64-linux-gnu" --parallel
```

### Building with vcpkg (Windows/macOS)

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Running

```bash
./build/Blackjack2
```

The game's assets (images, fonts) are automatically copied to the build output directory.

## Controls

- **Menu:** Navigate with arrow keys, select with Enter
- **Betting:** Adjust bet amount, confirm to deal
- **Player Turn:** Hit, Stand, Double Down, Split, or Surrender
- **Pause:** Access pause menu to resume, restart, or quit to menu
- **Replay:** Review hand history from the main menu

## Testing

Tests are written with Catch2 and auto-fetched during configuration.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
cd build
ctest --output-on-failure
```

Or run the test binary directly:

```bash
./build/Blackjack2Tests
```

## Project Structure

```
Blackjack2/
├── main.cpp           # Entry point
├── CMakeLists.txt     # Build configuration
├── CMakePresets.json  # CMake presets
├── include/           # Header files
│   ├── Game.hpp       # Main game logic
│   ├── Card.hpp       # Card representation
│   ├── Deck.hpp       # Deck/shoe management
│   ├── Hand.hpp       # Player/dealer hand
│   ├── Player.hpp     # Player state and stats
│   ├── HUD.hpp        # Heads-up display
│   ├── Replay.hpp     # Hand replay system
│   └── ...
├── src/               # Implementation files
├── tests/             # Catch2 test suites
├── assets/            # Game images, fonts, etc.
└── .github/workflows/ # CI/CD build & release pipeline
```

## Features

- Single or multiple player games
- Kelly criterion-based betting with adjustable risk slider
- Basic strategy advice and deviation tracking
- Insurance and split support
- Replay history of all hands
- Session and shoe win/loss tracking
- Configurable game speed (Fast / Normal / Slow / AutoPlay; AutoPlay advances every seat on basic strategy for analysis)

## CI/CD

GitHub Actions builds release **zips** for both Windows and macOS on tagged releases (`v*`) — the macOS build ships a `.zip` of the app bundle rather than a DMG (hdiutil is unreliable on GitHub runners).
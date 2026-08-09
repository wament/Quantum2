# Blackjack2

A transparent, strategy-focused blackjack game built in C++ for playing, learning, and competing.

![Blackjack2 Screenshot](docs/screenshots/game-sample-1.png)

## What Is This?

Blackjack2 is a single-player blackjack game that respects the mathematics and strategy of the game. It's not a casino app trying to hook you—it's a tool for learning card counting, testing betting strategies, and tracking your decisions over real shoes.

Built in C++ with SFML, it features a complete HUD, persistent profiles, full game recording, and a clean architecture designed to grow into something bigger.

## Features

### Play

- Full blackjack rules: 2-deck shoe, 3:2 payouts, dealer hits soft 17, late surrender
- Split up to 4 hands, double down, insurance
- Responsive SFML-based UI with card sprites and felt table

### Learn

- **Basic strategy advisor**: Press `I` during your turn to see the optimal play
- **Hi-Lo card counting**: Running count and true count displayed live in the HUD
- **Kelly criterion betting**: Adjustable risk slider lets you experiment with optimal bet sizing from conservative to aggressive

### Track

- Every hand, round, and shoe is recorded—cards dealt, actions taken, counts, timestamps
- Persistent player profile with lifetime stats (wins, losses, blackjacks, greatest session win/loss, etc.)
- Full history saved as JSON for later analysis or replay

### Built to Extend

- Clean separation between game logic, rendering, and persistence
- Daily shuffle seed competition (planned)
- Configurable rules (planned)
- Replay system infrastructure in place via `ShoeRecord` and deck reconstruction

## Quick Start

### Prerequisites

- CMake 3.10+
- GCC 13.3.0+ (or any C++17 compiler)
- SFML 2.5+

### Linux

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake g++ libsfml-dev

# Build
cmake --preset default
cmake --build --preset default

# Run
./build/Blackjack2
```

### Windows (via MinGW on Linux)

```bash
# Install MinGW and SFML for MinGW
sudo apt install mingw-w64 mingw-w64-sfml

# Build
mkdir build-windows && cd build-windows
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-win64-toolchain.cmake ..
cmake --build .
```

See [INSTALL.md](INSTALL.md) for full setup instructions.

## Controls

| Phase | Key | Action |
|-------|-----|--------|
| Betting | ↑ ↓ | Adjust bet |
| Betting | ← | Auto Kelly bet |
| Betting | → | Confirm bet and deal |
| Player Turn | H | Hit |
| Player Turn | S | Stand |
| Player Turn | D | Double down |
| Player Turn | P | Split |
| Player Turn | Y | Surrender |
| Player Turn | I | Basic strategy advice |
| Round End | R | Next round / Restart game |

## Tech Stack

- **Language**: C++17
- **Graphics**: SFML 2.5
- **JSON**: nlohmann/json (embedded header-only)
- **Build**: CMake with presets
- **Platforms**: Linux (native), Windows (MinGW cross-compile)

## Project Structure

```
Blackjack2/
├── main.cpp                 # Entry point
├── CMakeLists.txt           # Build configuration
├── include/                 # Headers: Game, Deck, Hand, HUD, etc.
├── src/                     # Implementations
├── assets/                  # Fonts, card sprites, table background
└── docs/                    # Documentation, summaries, screenshots
```

## Design Philosophy

This is a personal sandbox project guided by a few principles:

- **Clarity over cleverness**—code should be easy to read and reason about.
- **Transparency builds trust**—the math is visible, the history is recorded, nothing is hidden.
- **Play and study are not opposites**—learning features are woven into gameplay, not bolted on.
- **Constraints are creative**—starting with real blackjack rules gives a solid foundation to experiment from.

The project is designed to be extended incrementally. Read more in [PROJECT_DESIGN.md](PROJECT_DESIGN.md) and [PROJECT_PHILOSOPHY.md](PROJECT_PHILOSOPHY.md).

## Roadmap

- [x] Core gameplay with full rules
- [x] HUD with stats, counts, and Kelly slider
- [x] Persistent profiles and game history
- [x] Card counting display
- [x] Basic strategy advisor
- [ ] Replay past shoes
- [ ] Daily shuffle seed competition
- [ ] Configurable rules and variants
- [ ] Multiple player profiles

## License

Check the repository for license details.
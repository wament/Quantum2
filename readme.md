# Blackjack2

A feature-rich Blackjack implementation in C++ using the Simple and Fast Multimedia Library (SFML). This project provides a complete casino-style Blackjack experience with advanced features for card counting, statistical tracking, and optimal betting strategies.

## Features

*   **Full Blackjack Rules:** Supports standard casino rules including Splitting, Double Down, Insurance, and Surrender. The dealer hits on Soft 17 (H17).
*   **Hi-Lo Card Counting:** Tracks the Running Count and calculates the True Count to help players determine the house edge.
*   **Kelly Criterion Betting:** An auto-betting system that calculates the optimal wager based on the player's bankroll, the current count, and risk tolerance (adjustable via slider).
*   **Basic Strategy Advisor:** Press **'I'** during your turn to see the mathematically optimal move (H, S, D, or P) for your specific hand and the dealer's up-card.
*   **Dynamic HUD & Stats:** A Heads-Up Display showing Bankroll, Running Count, True Count, Cards Remaining, and Win/Loss statistics.
*   **Resizable Window:** The game scales correctly when the window is resized.
*   **Automated Testing:** Includes unit tests for game logic using Catch2.

## Prerequisites

*   **C++17** Compliant Compiler (GCC, Clang, or MSVC)
*   **CMake** (Version 3.10 or newer)
*   **SFML** (Version 3.0 or newer)
    *   Components required: `Graphics`, `Window`, `System`, `Audio`, `Network`

## Building the Project

### 1. Clone the Repository
```bash
git clone <repository-url>
cd Blackjack2
```

### 2. Install Dependencies
**On Linux (Ubuntu/Debian):**
```bash
sudo apt install libsfml-dev
```

**On macOS (Homebrew):**
```bash
brew install sfml
```

**On Windows (using vcpkg):**
```bash
vcpkg install sfml:x64-windows
```

### 3. Compile
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Running the Game
Navigate to the `build` directory (where the executable is located) and run:
```bash
./Blackjack2
```
*Note: The game copies assets from the source `assets/` folder to the build folder automatically during the build process.*

## Controls

| Action | Key |
| :--- | :--- |
| **Betting** | |
| Increase Bet | `↑` (Up Arrow) |
| Decrease Bet | `↓` (Down Arrow) |
| Auto-Bet (Kelly) | `←` (Left Arrow) |
| Confirm Bet | `→` (Right Arrow) or `Enter` |
| **Gameplay** | |
| Hit | `H` |
| Stand | `S` |
| Double Down | `D` |
| Split | `P` |
| Surrender | `Y` |
| **Strategy** | |
| Get Basic Strategy Advice | `I` |
| **Game State** | |
| Next Round / Restart | `R` |

## Project Structure

*   **`src/`**: Contains the core logic for the Game loop, Player, Deck, Hand, Graphics rendering, and HUD.
*   **`include/`**: Header files defining classes and structures.
*   **`assets/`**: External resources, including the card tilemap, felt background texture, and fonts.
*   **`tests/`**: Unit tests for `Card`, `Deck`, `Hand`, and asset loading.

## Testing

Run the unit tests from the build directory:
```bash
./Blackjack2Tests
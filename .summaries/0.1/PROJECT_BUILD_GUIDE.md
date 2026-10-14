# Blackjack2 - Installation & Setup Guide

## Overview

Blackjack2 is a single-player blackjack game built in C++17 using SFML. It features card counting (Hi-Lo system), Kelly criterion betting, basic strategy guidance, full game recording, and persistent player profiles.

This guide covers how to build and run the project on Linux (native) and Windows (via MinGW cross-compilation).

---

## Prerequisites

### Minimum Requirements

- **CMake**: 3.10 or later
- **C++ Compiler**: GCC 13.3.0 or compatible (C++17 support required)
- **SFML**: 2.5 or later (graphics, window, system, audio, network modules)

### Optional

- Git (to clone the repository)

---

## Linux Installation

### Step 1: Install Dependencies

On Ubuntu/Debian-based systems:

```bash
sudo apt update
sudo apt install cmake g++ libsfml-dev
```

On Fedora/RHEL-based systems:

```bash
sudo dnf install cmake gcc-c++ sfml-devel
```

On Arch Linux:

```bash
sudo pacman -S cmake gcc sfml
```

> **Note:** The project was tested with GCC 13.3.0 and SFML 2.5. Newer versions may work but are not guaranteed.

### Step 2: Clone or Extract the Project

If using Git:

```bash
git clone <repository-url>
cd Blackjack2
```

Or extract the project to a directory of your choice and navigate into it.

### Step 3: Build the Project

The project uses CMake presets for easy building.

#### Using the Preset (Recommended)

```bash
# Configure
cmake --preset default

# Build
cmake --build --preset default
```

The preset is configured for GCC 13.3.0 with Debug build type.

#### Manual Build (Alternative)

```bash
mkdir build
cd build
cmake ..
make
```

### Step 4: Run the Game

After a successful build, run the executable from the build directory:

```bash
./Blackjack2
```

The `assets/` directory is automatically copied to the build output by CMake, so no manual setup is needed.

### Step 5: Save Files

Player profiles are saved to `../saves/player_profile.json` relative to the executable's working directory. This directory is created automatically on first save.

---

## Windows Installation (via MinGW Cross-Compilation)

This project is designed to build Windows executables from a Linux host using MinGW. If you're building natively on Windows, see the **Native Windows Build** section below.

### Prerequisites for Cross-Compilation

On your Linux system, install MinGW-w64:

```bash
# Ubuntu/Debian
sudo apt install mingw-w64

# Fedora
sudo dnf install mingw64-gcc-c++ mingw64-sfml

# Arch
sudo pacman -S mingw-w64-gcc mingw-w64-sfml
```

You'll also need SFML built for MinGW (or pre-built SFML MinGW binaries).

### Step 1: Obtain MinGW SFML

Download or build SFML for MinGW x86_64:

- Pre-built: Download from [SFML official site](https://www.sfml-dev.org/download/sfml/2.5.1/) (Windows + MinGW package)
- Or build from source with MinGW toolchain

Extract/place SFML in a known location, e.g., `/opt/sfml-mingw`.

### Step 2: Configure the Toolchain

The project includes `mingw-win64-toolchain.cmake`. Edit it if needed to match your MinGW/SFML paths.

Typical settings (verify these match your system):

```cmake
set(MINGW_PREFIX "x86_64-w64-mingw32-")
set(CMAKE_SYSTEM_NAME Windows)
# SFML root path
set(SFML_ROOT "/opt/sfml-mingw")
```

### Step 3: Build for Windows

```bash
mkdir build-windows
cd build-windows

# Configure with MinGW toolchain
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-win64-toolchain.cmake ..

# Build
cmake --build .
```

### Step 4: Run on Windows

The CMakeLists.txt automatically copies required DLLs to the output `bin/` directory:

- SFML DLLs (`sfml-graphics.dll`, `sfml-window.dll`, etc.)
- MinGW runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`)
- All assets from `assets/`

To run:

- Copy the entire `bin/` folder to your Windows machine
- Run `Blackjack2.exe` from within `bin/`

Alternatively, run from Linux using Wine:

```bash
wine bin/Blackjack2.exe
```

---

## Native Windows Build

If you're building directly on Windows:

### Step 1: Install Tools

- **CMake**: Download from [cmake.org](https://cmake.org/download/)
- **MinGW-w64** or **MSVC** (Visual Studio Build Tools)
- **SFML**: Download Windows package from [SFML official site](https://www.sfml-dev.org/download/sfml/2.5.1/) matching your compiler

### Step 2: Configure SFML Path

Set `SFML_ROOT` to where SFML is installed, e.g.:

```
C:\SFML-2.5.1
```

You can set this as an environment variable or pass it to CMake:

```bash
cmake -DSFML_ROOT="C:\SFML-2.5.1" ..
```

### Step 3: Build

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Step 4: Run

Ensure SFML DLLs are in the same directory as `Blackjack2.exe` (or in your system PATH), then run the executable.

---

## Troubleshooting

### CMake Cannot Find SFML

Set `SFML_ROOT` explicitly:

```bash
cmake -DSFML_ROOT="/path/to/sfml" ..
```

### Missing MinGW DLLs on Windows

Verify these are present in the `bin/` directory alongside `Blackjack2.exe`:

- `libgcc_s_seh-1.dll` or `libgcc_s_dw2-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`
- All SFML DLLs (`sfml-graphics.dll`, `sfml-window.dll`, `sfml-system.dll`, `sfml-audio.dll`, `sfml-network.dll`)

### Assets Not Found / Black Screen

The CMake build should copy `assets/` to the output directory. Verify that the following exist next to the executable (or in a subdirectory named `assets`):

- `assets/fonts/roboto/RobotoFlex.ttf` (or similar font file)
- `assets/images/felt.png`
- `assets/images/cardsLarge_tilemap_packed.png`

If missing, run the build again or manually copy the `assets/` folder.

### Compiler Version Issues

The project targets C++17. If you encounter compilation errors, ensure:

- Your compiler supports C++17 (GCC 7+, Clang 5+, MSVC 2017+)
- The project was tested with GCC 13.3.0

### Save File Errors

The game saves to `../saves/player_profile.json`. If you see save/load errors:

- Ensure the `saves/` directory is writable
- The directory is created automatically if it doesn't exist
- Check file permissions

---

## Project Structure (For Reference)

```
Blackjack2/
├── CMakeLists.txt                 # Build configuration
├── CMakePresets.json              # CMake presets
├── mingw-win64-toolchain.cmake    # MinGW cross-compilation settings
├── main.cpp                       # Entry point
├── include/                       # Header files
│   ├── Card.hpp
│   ├── Deck.hpp
│   ├── Hand.hpp
│   ├── Game.hpp
│   ├── Player.hpp
│   ├── PlayerProfile.hpp
│   ├── GameRecord.hpp
│   ├── Serialization.hpp
│   ├── Constants.hpp
│   ├── Graphics.hpp
│   ├── HUD.hpp
│   ├── HUDState.hpp
│   ├── Panel.hpp
│   ├── StatBox.hpp
│   ├── FontManager.hpp
│   └── nlohmann/json.hpp          # Embedded JSON library
├── src/                           # Implementation files
├── assets/                        # Game assets
│   ├── fonts/roboto/
│   └── images/
│       ├── felt.png
│       └── cardsLarge_tilemap_packed.png
└── docs/                          # Documentation
```

---

## Controls (Quick Reference)

| Phase | Key | Action |
|-------|-----|--------|
| Betting | ↑ | Increase bet |
| Betting | ↓ | Decrease bet |
| Betting | ← | Auto Kelly bet |
| Betting | → | Confirm bet |
| Player Turn | H | Hit |
| Player Turn | S | Stand |
| Player Turn | D | Double down |
| Player Turn | P | Split |
| Player Turn | Y | Surrender |
| Player Turn | I | Basic strategy advice |
| Round End | R | Next round |
| Game Over | R | Restart game |

---

## Additional Documentation

- [PROJECT_DESIGN.md](PROJECT_DESIGN.md) — C++ design principles and architecture
- [PROJECT_PHILOSOPHY.md](PROJECT_PHILOSOPHY.md) — Philosophy and vision behind the project
- [PROJECT_TECHNICAL_SUMMARY.md](PROJECT_TECHNICAL_SUMMARY.md) — Detailed technical reference

---

## License

This project is personal and open-source friendly. Check the repository for license details.
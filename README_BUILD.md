# Blackjack2 - Build Guide

## Overview

Blackjack2 is a modern C++17 Blackjack game built with SFML. It supports advanced features like Hi-Lo card counting, Kelly Criterion betting, Basic Strategy guidance, and a full replay system. The project is developed on Linux but targets all major platforms: Linux, macOS, and Windows.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Project Structure](#project-structure)
3. [Quick Start (Linux Development)](#quick-start-linux-development)
4. [Local Builds](#local-builds)
   - [Linux (Native)](#linux-native)
   - [macOS (Native via vcpkg)](#macos-native-via-vcpkg)
   - [Windows (MinGW Cross-compile)](#windows-mingw-cross-compile)
   - [Windows (MSVC Native)](#windows-msvc-native)
5. [Testing](#testing)
6. [CI/CD with GitHub Actions](#cicd-with-github-actions)
7. [Packaging & Distribution](#packaging--distribution)
8. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required

- **CMake** ≥ 3.10
- **C++ Compiler** supporting C++17:
  - GCC ≥ 9 (Linux)
  - Clang ≥ 10 (macOS/Linux)
  - MSVC ≥ 19.20 (Windows)
  - MinGW-w64 (Windows cross-compile from Linux)
- **SFML** ≥ 3.0 (Graphics, Window, System, Audio, Network)
- **Git** (for cloning vcpkg)

### Optional

- **vcpkg** — Microsoft's C++ package manager (recommended for macOS and Windows builds)
- **Catch2** — Downloaded automatically via CMake FetchContent for testing

### SFML Dependencies

SFML requires several system libraries. On **Ubuntu/Debian**, install them with:

```bash
sudo apt install libxrandr-dev libfreetype6-dev libsndfile1-dev libopenal-dev libgl1-mesa-dev libegl1-mesa-dev libudev-dev
```

On **macOS**, vcpkg handles these automatically. On **Windows (MSVC)**, you may need to download SFML binaries directly or use vcpkg.

---

## Project Structure

```
.
├── CMakeLists.txt          # Main CMake build configuration
├── CMakePresets.json       # Default CMake presets for local development
├── main.cpp                # Application entry point
├── README_BUILD.md         # This file
├── README_PHIL.md          # Design philosophy
├── README_TECH.md          # Technical architecture overview
├── assets/
│   ├── fonts/
│   │   └── roboto/
│   │       ├── RobotoFlex-VariableFont.ttf   # Main font
│   │       ├── OFL.txt
│   │       └── README.txt
│   └── images/
│       ├── cardsLarge_tilemap_packed.png     # Card sprite sheet
│       └── felt.png                          # Table background
├── include/
│   ├── Card.hpp
│   ├── Constants.hpp
│   ├── Deck.hpp
│   ├── FontManager.hpp
│   ├── Game.hpp
│   ├── GameRecord.hpp
│   ├── Graphics.hpp
│   ├── HUD.hpp
│   ├── HUDState.hpp
│   ├── Hand.hpp
│   ├── Panel.hpp
│   ├── Player.hpp
│   ├── PlayerProfile.hpp
│   ├── Serialization.hpp
│   ├── StatBox.hpp
│   ├── catch.hpp
│   ├── catch_all.hpp
│   └── nlohmann/
│       └── json.hpp       # Single-header JSON library
├── src/
│   ├── Card.cpp
│   ├── Deck.cpp
│   ├── FontManager.cpp
│   ├── Game.cpp
│   ├── Graphics.cpp
│   ├── HUD.cpp
│   ├── Hand.cpp
│   ├── Panel.cpp
│   ├── Player.cpp
│   ├── PlayerProfile.cpp
│   └── StatBox.cpp
├── tests/
│   ├── test_card.cpp
│   ├── test_deck.cpp
│   ├── test_hand.cpp
│   └── test_assets.cpp
└── .github/workflows/
    └── build.yml           # CI/CD pipeline
```

---

## Quick Start (Linux Development)

The fastest way to build and run the game on Linux:

```bash
# 1. Clone the repository
git clone <repository-url>
cd <project-directory>

# 2. Create and enter build directory
mkdir -p build && cd build

# 3. Configure (auto-detects SFML on your system)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 4. Build
cmake --build . --parallel

# 5. Run
./Blackjack2
```

> **Note:** Assets are automatically copied to the build directory on each build via a CMake post-build step, so you don't need to manage them manually during development.

---

## Local Builds

### Linux (Native)

Uses system-installed SFML. No external package manager required.

```bash
# Install SFML on Ubuntu/Debian
sudo apt install libsfml-dev

# Configure and build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

**Build types:**
- `Debug` — Debug symbols, no optimization. Best for development.
- `Release` — Full optimization, stripped symbols. Best for distribution.

### macOS (Native via vcpkg)

vcpkg ensures all SFML dependencies (FreeType, OpenAL, etc.) are bundled correctly.

```bash
# Install vcpkg (one-time setup)
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics

# Install SFML (arm64 for Apple Silicon, x64 for Intel)
./vcpkg install sfml --triplet arm64-osx   # Apple Silicon
# or
./vcpkg install sfml --triplet x64-osx     # Intel Mac

# Configure CMake with vcpkg toolchain
mkdir -p build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

cmake --build . --parallel
```

### Windows (MinGW Cross-compile from Linux)

Cross-compile to produce a Windows `.exe` from your Linux development machine.

```bash
# Install MinGW-w64
sudo apt install mingw-w64

# Download SFML Windows binaries from https://www.sfml-dev.org/download/
# Extract to a known location, e.g.:
mkdir -p /opt/sfml-windows
# (copy SFML contents into /opt/sfml-windows/)

# Configure with SFML_ROOT pointing to the extracted SFML
mkdir -p build-win && cd build-win
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=/usr/share/cmake-3.x/Modules/Platform/Windows.GNU.cmake \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DSFML_ROOT=/opt/sfml-windows \
    -DCMAKE_BUILD_TYPE=Release

cmake --build . --parallel
```

The output will be `Blackjack2.exe`. Distribute this alongside the `assets/` directory.

> **Important:** The MinGW build uses **static** SFML linking (`SFML_STATIC_LIBRARIES=ON`). This bundles SFML into your executable but increases binary size.

### Windows (MSVC Native)

```bash
# Open "x64 Native Tools Command Prompt for VS 2022"

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

SFML can be installed via vcpkg (`vcpkg install sfml:x64-windows`) or downloaded as pre-built binaries from the SFML website. If using pre-built binaries, set `SFML_ROOT` to the installation path.

---

## Testing

Catch2 is fetched automatically via CMake's `FetchContent` module. No manual installation needed.

```bash
# Build and run tests
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel

# Run tests
ctest --output-on-failure

# Or run the test executable directly
./Blackjack2Tests
```

### Running Specific Tests

Catch2 supports filtering by tag or name:

```bash
# Run only card-related tests
./Blackjack2Tests "[card]"

# Run a specific test by name
./Blackjack2Tests "*hand_value*"
```

> **Note:** The test binary links SFML dynamically even in static builds, as Catch2 has known issues with static linking.

---

## CI/CD with GitHub Actions

The project uses GitHub Actions to build and package releases for macOS and Windows on every git tag push (`v*`).

### Trigger

Push a tag starting with `v` to the repository:

```bash
git tag v0.2.0
git push origin v0.2.0
```

### Pipeline Overview

The `.github/workflows/build.yml` workflow:

1. **Builds for macOS** (Apple Silicon) — Produces a `.dmg` installer
2. **Builds for Windows** (x64) — Produces a `.zip` archive
3. **Creates a GitHub Release** — Attaches both artifacts

### Configuration

Set the following repository secrets:

| Secret | Description |
|--------|-------------|
| `RELEASE_REPO_OWNER` | GitHub account/organization that owns the release repository |
| `RELEASE_REPO_NAME` | Name of the repository for releases |
| `RELEASE_REPO_TOKEN` | Personal Access Token (PAT) with `repo` scope |

The workflow caches the vcpkg installation between runs to speed up CI builds.

### Output Artifacts

| Platform | Format | Contents |
|----------|--------|----------|
| macOS | `.dmg` | Self-contained `.app` bundle with SFML frameworks and assets |
| Windows | `.zip` | `.exe` + SFML DLLs + `assets/` directory |

---

## Packaging & Distribution

### macOS (.dmg)

The CI pipeline produces a self-contained application bundle:

```
Blackjack2.app/
├── Contents/
│   ├── Info.plist          (generated by hdiutil)
│   ├── MacOS/
│   │   └── Blackjack2      (executable)
│   ├── Frameworks/
│   │   ├── SFML-Graphics.framework
│   │   ├── SFML-Window.framework
│   │   ├── SFML-System.framework
│   │   ├── SFML-Audio.framework
│   │   ├── SFML-Network.framework
│   │   └── libfreetype.dylib
│   └── Resources/
│       └── assets/
│           ├── fonts/
│           └── images/
```

The app bundle uses `@executable_path/../Frameworks` rpaths so all frameworks resolve correctly at runtime.

### Windows (.zip)

The Windows package includes:

```
Blackjack2-Windows.zip/
├── Blackjack2.exe
├── assets/
│   ├── fonts/
│   └── images/
├── sfml-graphics-2.dll
├── sfml-window-2.dll
├── sfml-system-2.dll
├── sfml-audio-2.dll
├── sfml-network-2.dll
├── freetype-6.dll          # SFML dependency
├── libopenal-1.dll         # SFML dependency
└── (other DLLs from vcpkg)
```

All SFML and system DLLs are copied from vcpkg's `bin/` directory. Distribute the entire `.zip` as-is.

### Linux

For Linux distribution, ship:
- The `Blackjack2` executable
- The `assets/` directory alongside it
- Ensure the user has the SFML system libraries installed

---

## Troubleshooting

### "SFML not found"

- **Linux:** `sudo apt install libsfml-dev`
- **macOS/Windows:** Ensure vcpkg is properly initialized and SFML is installed. Verify the toolchain file path is correct.

### MinGW Cross-compile: "SFML_STATIC_LIBRARIES must be set"

The CMakeLists.txt requires `SFML_ROOT` to be set when cross-compiling with MinGW. Make sure the path points to your extracted SFML Windows binaries.

### macOS: "Library not loaded" at runtime

Verify the rpath is set correctly:
```bash
otool -l Blackjack2.app/Contents/MacOS/Blackjack2 | grep -A2 rpath
```
You should see `@executable_path/../Frameworks`. If missing, fix with:
```bash
install_name_tool -add_rpath "@executable_path/../Frameworks" Blackjack2.app/Contents/MacOS/Blackjack2
```

### Assets not found at runtime

The CMake post-build step copies assets automatically during `cmake --build`. If assets are missing, rebuild:
```bash
cmake --build .
```
If the issue persists, verify the `assets/` directory exists at the project root and contains `fonts/` and `images/` subdirectories.

### vcpkg cache not working in CI

vcpkg caching is keyed on `**/CMakeLists.txt`. If you add/remove dependencies, the cache key will change and vcpkg will be reinstalled. This is normal. The first build of a tag will always install vcpkg fresh.

### Tests fail to discover

Ensure `CTest` and the `Catch` CMake module are included in the CMakeLists.txt. The `catch_discover_tests()` call is at the bottom of the file. Run `cmake ..` again after any CMakeLists.txt changes.

---

## Build Configuration Reference

The CMakeLists.txt uses conditional logic to handle different build environments:

| Condition | SFML Mode | Linking |
|-----------|-----------|---------|
| vcpkg toolchain detected | vcpkg package | Dynamic (`SFML_STATIC_LIBRARIES=OFF`) |
| MinGW cross-compile | Manual `SFML_ROOT` | Static (`SFML_STATIC_LIBRARIES=ON`) |
| Linux/macOS native system SFML | system `find_package` | Static (`SFML_STATIC_LIBRARIES=ON`) |
| GitHub Actions (CI) | vcpkg package | Dynamic (`SFML_STATIC_LIBRARIES=OFF`) |

To override any of these defaults, set the corresponding cache variable before configuring:

```bash
cmake .. -DSFML_STATIC_LIBRARIES=OFF
```
</think>

```tool
TOOL_NAME: create_new_file
BEGIN_ARG: filepath
README_BUILD.md
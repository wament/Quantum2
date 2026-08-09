# Cross-compilation toolchain: Linux -> Windows (MinGW-w64)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Point to SFML Windows MinGW installation (corrected path with nested folder)
set(SFML_ROOT "$ENV{HOME}/sfml-windows-mingw/SFML-2.6.1" CACHE PATH "SFML root for Windows")

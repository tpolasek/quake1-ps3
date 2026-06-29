# CMake toolchain file for building Chocolate Quake for PlayStation 3 (PSL1GHT).
#
# Targets the PPU (PowerPC 64-bit, powerpc64-ps3-elf) using the ps3dev
# toolchain (ppu-gcc). Expects the hldtux/ps3dev-sdl2 docker image (or an
# equivalent ps3dev install) where libraries live under
# /usr/local/ps3dev/portlibs/ppu.
#
# Usage:
#   cmake -S . -B build-ps3 -DCMAKE_TOOLCHAIN_FILE=cmake/ps3.toolchain.cmake \
#         -DCMAKE_BUILD_TYPE=Release
#

# PS3 toolchain root
set(PS3DEV "/usr/local/ps3dev" CACHE PATH "Path to ps3dev install")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ppu)

set(CMAKE_C_COMPILER "${PS3DEV}/ppu/bin/ppu-gcc")
set(CMAKE_CXX_COMPILER "${PS3DEV}/ppu/bin/ppu-g++")

# Search behavior: look for libs/headers only in the PS3 root, but keep
# finding build-side tools (like sdl2-config) from the host path.
set(CMAKE_FIND_ROOT_PATH "${PS3DEV}/portlibs/ppu" "${PS3DEV}/ppu")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Tell pkg-config to target the PS3 sysroot.
set(ENV{PKG_CONFIG_LIBDIR} "${PS3DEV}/portlibs/ppu/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${PS3DEV}/portlibs/ppu")

# PS3 doesn't have shared lib loading, /proc, or a full glibc.
set(CMAKE_C_COMPILER_WORKS ON)
set(CMAKE_C_PLATFORM_HAS_FPU ON)

# Project-side flag used to switch to PS3-specific deps (e.g. SDL2_net stub).
set(CHOCOLATE_QUAKE_PS3 ON CACHE BOOL "Building for PS3 (PPU/PSL1GHT)" FORCE)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.21.0)
project("Dragonfly Quake" VERSION 2.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)

# C89's implicit-function-declaration fallback has caused real ABI-truncation
# bugs on PS3 (PPC64: int return read from r3 while the real function returns
# double in f1, silently corrupting parsed values). Force this class of bug
# to be a compile error.
add_compile_options(
    -Werror=implicit-function-declaration
    -Werror=implicit-int
)

string(TIMESTAMP CURRENT_YEAR "%Y")
set(PACKAGE_NAME ${PROJECT_NAME})
set(PACKAGE_TARNAME "dragonfly-quake")
set(PACKAGE_VERSION ${PROJECT_VERSION})
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_COPYRIGHT "Copyright (C) 1996-${CURRENT_YEAR}")
set(PACKAGE_LICENSE "GNU General Public License, version 3")

# PS3 (PPU/PSL1GHT) path: native PSL1GHT APIs. PS3 is the only supported
# target; the toolchain file (cmake/ps3.toolchain.cmake) sets
# DRAGONFLY_QUAKE_PS3=ON so platform branches in source (e.g.
# Sys_GetDefaultBaseDir in src/sys) pick the PS3 layout.
add_compile_definitions(DRAGONFLY_QUAKE_PS3=1)

# PSL1GHT SDK headers (sysutil/sysutil.h, rsx/*.h, audio/audio.h,
# io/pad.h, etc.) live in ppu/include; codec headers (vorbis/vorbisfile.h,
# mad.h, FLAC/stream_decoder.h) live in the portlibs include dir. Neither
# is on the compiler's default search path.
include_directories("${PS3DEV}/portlibs/ppu/include" "${PS3DEV}/ppu/include")

# PSL1GHT runtime libraries, required by the native backends. Applied
# globally so every target links them. -lm is required by the codec libs
# (vorbis/FLAC); it lives in the powerpc64-ps3-elf sysroot, not the
# portlibs prefix.
link_libraries(-lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2 -lm)

find_library(VORBIS_LIBRARY vorbis REQUIRED)
find_library(VORBISFILE_LIBRARY vorbisfile REQUIRED)
find_library(MAD_LIBRARY mad REQUIRED)
find_library(FLAC_LIBRARY FLAC REQUIRED)
find_library(OGG_LIBRARY ogg REQUIRED)

configure_file(config.h.in config.h)

add_subdirectory("src")

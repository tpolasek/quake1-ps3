# FindSDL2.cmake
#
# chocolate-quake's CMake calls `find_package(SDL2 CONFIG REQUIRED)`. With
# CONFIG that looks for SDL2Config.cmake, but the ps3dev SDL2 port ships
# only sdl2-config + sdl2.pc. To avoid patching the project's find_package
# call, this FindSDL2 module runs sdl2-config and exposes the same
# SDL2_LIBRARIES / SDL2_INCLUDE_DIRS variables plus an `SDL2::SDL2` target
# that callers using the imported target syntax will pick up.
#
# This module is intentionally narrow and only supports what chocolate-quake
# needs (static SDL2 link). It is only loaded when CMake's built-in FindSDL2
# is bypassed via CMAKE_MODULE_PATH.

find_program(SDL2_CONFIG_EXECUTABLE NAMES sdl2-config)
if(NOT SDL2_CONFIG_EXECUTABLE)
    set(SDL2_CONFIG_EXECUTABLE "${PS3DEV}/portlibs/ppu/bin/sdl2-config")
endif()

if(EXISTS "${SDL2_CONFIG_EXECUTABLE}")
    execute_process(COMMAND "${SDL2_CONFIG_EXECUTABLE}" --cflags
        OUTPUT_VARIABLE _sdl2_cflags OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${SDL2_CONFIG_EXECUTABLE}" --libs
        OUTPUT_VARIABLE _sdl2_libs OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND "${SDL2_CONFIG_EXECUTABLE}" --version
        OUTPUT_VARIABLE SDL2_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Pull -I... out of cflags
    string(REGEX MATCHALL "-I[^ ]+" _sdl2_incs "${_sdl2_cflags}")
    foreach(_inc ${_sdl2_incs})
        string(SUBSTRING "${_inc}" 2 -1 _path)
        list(APPEND SDL2_INCLUDE_DIRS "${_path}")
    endforeach()

    # sdl2-config --libs gives flags like:
    #   -L<path> <path>/libSDL2.a -lm -lgcm_sys -lrsx -lsysutil -lio -laudio -lrt -llv2
    # Split into -L dirs, full .a paths, and -l flags. Stored in
    # SDL2_LIBRARIES_RAW so we can attach them to the SDL2::SDL2 imported
    # target's INTERFACE_LINK_LIBRARIES (and keep SDL2_LIBRARIES itself as
    # the target for legacy callers).
    separate_arguments(_sdl2_lib_args UNIX_COMMAND "${_sdl2_libs}")
    foreach(_arg ${_sdl2_lib_args})
        if(_arg MATCHES "^-L(.+)$")
            list(APPEND SDL2_LIB_DIR "${CMAKE_MATCH_1}")
        elseif(_arg MATCHES "^-l(.+)$")
            list(APPEND SDL2_LIBRARIES_RAW "${_arg}")
        elseif(_arg MATCHES "\\.(a|so)$")
            list(APPEND SDL2_LIBRARIES_RAW "${_arg}")
        elseif(_arg MATCHES "^-")
            list(APPEND SDL2_LIBRARIES_RAW "${_arg}")
        endif()
    endforeach()

    set(SDL2_FOUND TRUE)
    message(STATUS "Found SDL2 (sdl2-config): version=${SDL2_VERSION}")
    message(STATUS "  includes: ${SDL2_INCLUDE_DIRS}")
    message(STATUS "  raw libs: ${_sdl2_libs}")

    if(NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 INTERFACE IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARIES_RAW}")
    endif()
    if(NOT TARGET SDL2::SDL2main)
        add_library(SDL2::SDL2main INTERFACE IMPORTED)
    endif()

    # Expose SDL2_LIBRARIES as the imported target so callers that do
    # `target_link_libraries(... ${SDL2_LIBRARIES})` inherit includes too,
    # matching how SDL2Config.cmake behaves on Linux/vcpkg.
    set(SDL2_LIBRARIES SDL2::SDL2)
else()
    set(SDL2_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2
    REQUIRED_VARS SDL2_LIBRARIES_RAW SDL2_INCLUDE_DIRS
    VERSION_VAR SDL2_VERSION)

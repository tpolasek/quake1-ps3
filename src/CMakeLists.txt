include(CheckDefine)

check_define(strcpy "string.h" HAVE_STRCPY)
check_define(strncpy "string.h" HAVE_STRNCPY)
check_define(strcat "string.h" HAVE_STRCAT)

set(LIBS
    camera
    client
    cmd
    common
    console
    crc
    host
    input
    mathlib
    memory
    menu
    model
    net
    progs
    renderer
    screen
    server
    sound
    status_bar
    sys
    video
    wad
)
foreach(LIB ${LIBS})
    add_subdirectory(${LIB})
endforeach()


add_executable(${PACKAGE_TARNAME} main.c)

target_include_directories(${PACKAGE_TARNAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

target_link_libraries(${PACKAGE_TARNAME} ${LIBS})

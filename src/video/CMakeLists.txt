set(LIB video)

add_library(${LIB} STATIC
    src/vid_buffers.c
    src/vid_buffers.h
    src/vid_main.c
    src/vid_menu.c
    src/vid_menu.h
    src/vid_modes.c
    src/vid_modes.h
    src/vid_ps3.c
    src/vid_window.c
    src/vid_window.h
)

target_include_directories(${LIB} PRIVATE ${CMAKE_BINARY_DIR} "../")
target_include_directories(${LIB} PUBLIC "./include")
target_link_libraries(${LIB} PUBLIC common)
target_link_libraries(${LIB} PRIVATE
    ${SDL2_LIBRARIES}
    cmd
    console
    host
    input
    renderer
    sound
    sys
    wad
)

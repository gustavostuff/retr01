cmake_minimum_required(VERSION 3.16)

find_package(SDL2 REQUIRED)

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
)
FetchContent_MakeAvailable(imgui)

set(IMGUI_DIR ${imgui_SOURCE_DIR})

add_executable(retr01_studio
    main.cpp
    studio.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

target_include_directories(retr01_studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(retr01_studio PRIVATE
    retr01_core
    retr01_pack
    SDL2::SDL2
)

target_compile_definitions(retr01_studio PRIVATE
    RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
)

target_compile_features(retr01_studio PRIVATE cxx_std_11)
target_compile_options(retr01_studio PRIVATE -Wall -Wextra)

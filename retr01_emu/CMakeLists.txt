find_package(SDL2 REQUIRED)

add_library(retr01_emu_core STATIC
    src/ppu.c
)

target_include_directories(retr01_emu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(retr01_emu_core PUBLIC retr01_core)

add_executable(retr01_emu src/main.c)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core SDL2::SDL2)
target_compile_definitions(retr01_emu PRIVATE
    RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
)

if(RETR01_BUILD_TESTS)
    add_executable(test_ppu_blit test/test_ppu_blit.c)
    target_link_libraries(test_ppu_blit PRIVATE retr01_emu_core)
    target_include_directories(test_ppu_blit PRIVATE
        ${CMAKE_SOURCE_DIR}/retr01_world_studio/tests
    )
    target_compile_definitions(test_ppu_blit PRIVATE
        RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
    )
    add_test(NAME test_ppu_blit COMMAND test_ppu_blit)
endif()

add_subdirectory(core)
add_subdirectory(pack)

if(RETR01_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_executable(gen_one_screen_fixture tools/gen_one_screen_fixture.c)
target_link_libraries(gen_one_screen_fixture PRIVATE retr01_core)

add_executable(retr01_export tools/export_cart.c)
target_link_libraries(retr01_export PRIVATE retr01_pack retr01_core)
target_compile_definitions(retr01_export PRIVATE
    RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
)

if(RETR01_BUILD_STUDIO)
    add_subdirectory(app)
endif()

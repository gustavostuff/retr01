function(retr01_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE retr01_core)
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(${name} PRIVATE
        RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
        RETR01_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

retr01_add_test(test_screen unit/test_screen.c)
retr01_add_test(test_rle unit/test_rle.c)
retr01_add_test(test_palette unit/test_palette.c)
retr01_add_test(test_cart_io integration/test_cart_io.c)
retr01_add_test(test_map_roundtrip integration/test_map_roundtrip.c)

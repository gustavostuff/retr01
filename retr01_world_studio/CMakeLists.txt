add_subdirectory(core)

if(RETR01_BUILD_TESTS)
    add_subdirectory(tests)
endif()

add_executable(gen_one_screen_fixture tools/gen_one_screen_fixture.c)
target_link_libraries(gen_one_screen_fixture PRIVATE retr01_core)

if(RETR01_BUILD_STUDIO)
    message(STATUS "retr01_studio UI not implemented yet (Phase 1)")
endif()

cmake_minimum_required(VERSION 3.16)
project(retr01_studio C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

add_library(retr01_studio_core
  core/src/project.c
  core/src/chr_pack.c
  core/src/json_io.c
  core/src/palette.c
  core/src/spr_pack.c
  core/src/play.c
  core/src/cart.c
)
target_include_directories(retr01_studio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

enable_testing()
add_executable(test_core core/tests/test_core.c)
target_link_libraries(test_core PRIVATE retr01_studio_core)
add_test(NAME core COMMAND test_core)
set_tests_properties(core PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(retr01_studio_app
  app/src/app_shell.c
  app/src/ui.c
  app/src/font.c
)
target_include_directories(retr01_studio_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)
target_link_libraries(retr01_studio_app PUBLIC retr01_studio_core PkgConfig::SDL2)

add_executable(retr01_studio app/src/main.c)
target_link_libraries(retr01_studio PRIVATE retr01_studio_app)

# Visual + functional E2E (drives real SDL UI via app_shell).
set(R01_E2E_GOLDEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/app/tests/goldens")
set(R01_E2E_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/e2e_out")
file(MAKE_DIRECTORY "${R01_E2E_OUT_DIR}")

add_executable(test_e2e
  app/tests/test_e2e.c
  app/tests/e2e_harness.c
)
target_include_directories(test_e2e PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/app/src
  ${CMAKE_CURRENT_SOURCE_DIR}/app/tests
)
target_compile_definitions(test_e2e PRIVATE
  R01_E2E_GOLDEN_DIR="${R01_E2E_GOLDEN_DIR}"
  R01_E2E_OUT_DIR="${R01_E2E_OUT_DIR}"
)
target_link_libraries(test_e2e PRIVATE retr01_studio_app)
add_test(NAME e2e COMMAND test_e2e)
set_tests_properties(e2e PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
)

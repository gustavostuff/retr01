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

add_executable(retr01_studio
  app/src/main.c
  app/src/ui.c
  app/src/font.c
)
target_link_libraries(retr01_studio PRIVATE retr01_studio_core PkgConfig::SDL2)

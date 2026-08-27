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
  core/src/play.c
  core/src/prg_phase1.c
  core/src/cart.c
)
target_include_directories(retr01_studio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
target_link_libraries(retr01_studio_core PUBLIC PkgConfig::LIBPNG)

enable_testing()
add_executable(test_core core/tests/test_core.c)
target_link_libraries(test_core PRIVATE retr01_studio_core)
add_test(NAME core COMMAND test_core)
set_tests_properties(core PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_library(retr01_studio_app
  app/src/app_shell.c
  app/src/ui.c
  app/src/font.c
)
target_include_directories(retr01_studio_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)
target_link_libraries(retr01_studio_app PUBLIC retr01_studio_core PkgConfig::SDL2 PkgConfig::FREETYPE)
target_compile_definitions(retr01_studio_app PRIVATE
  "R01_STUDIO_ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")

add_executable(retr01_studio app/src/main.c)
target_link_libraries(retr01_studio PRIVATE retr01_studio_app)

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
  core/src/collision.c
  core/src/prg_phase1.c
  core/src/cart.c
  core/src/paths.c
  core/src/sprites.c
  core/src/entities.c
)
target_include_directories(retr01_studio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/include
)
set(R01_REPO_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Retr01 repo root")
set(R01_ROM_DIR "${R01_REPO_ROOT}/rom")
set(R01_TEST_CART "${R01_ROM_DIR}/test.retr01")
set(R01_TEST_PROJECT "${R01_ROM_DIR}/test.r01proj")
target_compile_definitions(retr01_studio_core PUBLIC "R01_REPO_ROOT=\"${R01_REPO_ROOT}\"")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
target_link_libraries(retr01_studio_core PUBLIC PkgConfig::LIBPNG)

enable_testing()

set(STUDIO_TEST_HARNESS core/tests/test_harness.c)

function(r01_studio_add_test name)
  add_executable(test_${name} core/tests/test_${name}.c ${STUDIO_TEST_HARNESS})
  target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/tests)
  target_link_libraries(test_${name} PRIVATE retr01_studio_core)
  add_test(NAME ${name} COMMAND test_${name})
  set_tests_properties(${name} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

r01_studio_add_test(project)
r01_studio_add_test(play)
r01_studio_add_test(chr)
r01_studio_add_test(json)
r01_studio_add_test(cart)
r01_studio_add_test(palette)
r01_studio_add_test(sprites)
r01_studio_add_test(entities)

add_executable(test_fixture core/tests/test_fixture.c ${STUDIO_TEST_HARNESS})
target_include_directories(test_fixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/core/tests)
target_link_libraries(test_fixture PRIVATE retr01_studio_core)
add_test(NAME fixture COMMAND test_fixture ${R01_TEST_PROJECT} ${R01_TEST_CART})
set_tests_properties(fixture PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

add_executable(export_rom core/tools/export_rom.c)
target_link_libraries(export_rom PRIVATE retr01_studio_core)

add_library(retr01_studio_app
  app/src/shell/app_shell.c
  app/src/font/font.c
  app/src/ui/core.c
  app/src/ui/primitives.c
  app/src/ui/layout.c
  app/src/ui/draw/frame.c
  app/src/ui/draw/sidebar.c
  app/src/ui/draw/mode.c
  app/src/ui/screen/selection.c
  app/src/ui/screen/paint.c
  app/src/ui/screen/edit.c
  app/src/ui/screen/draw.c
  app/src/ui/input/event.c
  app/src/ui/input/project.c
  app/src/ui/input/world.c
  app/src/ui/menu/menu.c
  app/src/ui/menu/draw.c
  app/src/ui/modals/pal_edit.c
  app/src/ui/modals/pal_edit_draw.c
  app/src/ui/modals/tile_edit.c
  app/src/ui/modals/sprite_edit.c
  app/src/ui/modals/entity_edit.c
)
target_include_directories(retr01_studio_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/app/src
)
target_link_libraries(retr01_studio_app PUBLIC retr01_studio_core PkgConfig::SDL2 PkgConfig::FREETYPE PkgConfig::LIBPNG)
target_compile_definitions(retr01_studio_app PRIVATE
  "R01_STUDIO_ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")

add_executable(retr01_studio app/src/main.c)
target_link_libraries(retr01_studio PRIVATE retr01_studio_app)

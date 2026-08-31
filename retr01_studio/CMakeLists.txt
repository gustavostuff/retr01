cmake_minimum_required(VERSION 3.16)
project(retr01_studio C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

set(R01_REPO_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Retr01 repo root")
set(R01_OUTPUT_DIR "${R01_REPO_ROOT}/output")
set(R01_TEST_CART "${R01_OUTPUT_DIR}/test.retr01")
set(R01_TEST_PROJECT "${R01_OUTPUT_DIR}/test.r01proj")

# Shared emu core for Studio Play (standalone ./emu still built from retr01_emu).
if(NOT TARGET retr01_emu_core)
  add_subdirectory(${R01_REPO_ROOT}/retr01_emu ${CMAKE_BINARY_DIR}/retr01_emu EXCLUDE_FROM_ALL)
endif()

add_library(retr01_studio_core
  core/src/project.c
  core/src/chr_pack.c
  core/src/json_io.c
  core/src/palette.c
  core/src/play.c
  core/src/collision.c
  core/src/prg_phase1.c
  core/src/cart.c
  core/src/export_codegen.c
  core/src/paths.c
  core/src/sprites.c
  core/src/entities.c
  core/src/metasprites.c
  core/src/metatiles.c
  core/src/warps.c
  core/src/game_runtime.c
  core/src/player_anim.c
)
target_include_directories(retr01_studio_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/core/include
  ${R01_REPO_ROOT}/common
)
target_compile_definitions(retr01_studio_core PUBLIC "R01_REPO_ROOT=\"${R01_REPO_ROOT}\"")
target_link_libraries(retr01_studio_core PUBLIC r01_play_common)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPNG REQUIRED IMPORTED_TARGET libpng)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
target_link_libraries(retr01_studio_core PUBLIC PkgConfig::LIBPNG m)

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
r01_studio_add_test(metasprites)
r01_studio_add_test(prg_phase1)
r01_studio_add_test(export_codegen)
r01_studio_add_test(warps)
r01_studio_add_test(game_runtime)
r01_studio_add_test(player_anim)

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
  app/src/ui/clipboard.c
  app/src/ui/widgets/dot_strip.c
  app/src/ui/widgets/radio.c
  app/src/ui/widgets/checkbox.c
  app/src/ui/widgets/palette_grid.c
  app/src/ui/widgets/button.c
  app/src/ui/widgets/tabs.c
  app/src/ui/widgets/modal.c
  app/src/ui/widgets/textfield.c
  app/src/ui/widgets/compose.c
  app/src/ui/layout.c
  app/src/ui/draw/frame.c
  app/src/ui/draw/sidebar.c
  app/src/ui/draw/mode.c
  app/src/ui/screen/selection.c
  app/src/ui/screen/paint.c
  app/src/ui/screen/edit.c
  app/src/ui/screen/draw.c
  app/src/ui/screen/play_emu.c
  app/src/ui/input/event.c
  app/src/ui/input/project.c
  app/src/ui/input/world.c
  app/src/ui/menu/menu.c
  app/src/ui/menu/draw.c
  app/src/ui/modals/pal_edit.c
  app/src/ui/modals/pal_edit_draw.c
  app/src/ui/modals/tile_edit.c
  app/src/ui/modals/sprite_edit.c
  app/src/ui/modals/metasprite_edit.c
  app/src/ui/modals/entity_edit.c
)
target_include_directories(retr01_studio_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/app/src
  ${R01_REPO_ROOT}/retr01_emu/include
)
target_link_libraries(retr01_studio_app PUBLIC retr01_studio_core retr01_emu_core PkgConfig::SDL2 PkgConfig::FREETYPE PkgConfig::LIBPNG)
find_package(X11)
if(X11_FOUND)
  target_compile_definitions(retr01_studio_app PRIVATE R01_HAS_X11=1)
  target_include_directories(retr01_studio_app PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(retr01_studio_app PUBLIC ${X11_LIBRARIES})
endif()
target_compile_definitions(retr01_studio_app PRIVATE
  "R01_STUDIO_ASSETS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")

add_executable(retr01_studio app/src/main.c)
target_link_libraries(retr01_studio PRIVATE retr01_studio_app)

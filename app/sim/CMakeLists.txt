cmake_minimum_required(VERSION 3.16)
project(retr01_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

add_library(retr01_sim_core
  src/pin.c
  src/entity.c
  src/bus.c
  src/gamepad.c
  src/island.c
  src/island_group.c
  src/island_builder.c
  src/board.c
  src/board_debug.c
  src/frame_log.c
  src/play.c
  src/timing.c
  chips/stub14.c
  chips/pwr5v.c
  chips/osc8m.c
  chips/sn74hc14.c
  chips/sn74hc04.c
  chips/sn74hc00.c
  chips/sn74hc08.c
  chips/sn74hc32.c
  chips/sn74hc245.c
  chips/atf22v10.c
  chips/i2c_eeprom.c
  chips/sn74hc157.c
  chips/as6c62256.c
  chips/prg_rom.c
  chips/pads.c
  chips/attiny85.c
  chips/pad_uart.c
  chips/flasher_bus.c
  chips/sst39sf040.c
  chips/w65c02s.c
  chips/osc_dot.c
  chips/beam_xy.c
  chips/bg_fetch.c
  chips/at28c16.c
  chips/compositor.c
  chips/video_sink.c
  chips/atmega328p.c
  chips/atmega1284p.c
  chips/atmega32u4.c
  chips/sn74hc595.c
  chips/usbc_receptacle.c
  chips/pc_host.c
  chips/sprite_fetch.c
  chips/integration.c
  src/cart_module.c
  src/cart_slot.c
  src/board_flasher.c
  src/ui_button.c
  src/flasher_bench.c
)
set(R01_PKG_ROOT "${CMAKE_SOURCE_DIR}/..")
set(R01_REPO_ROOT "${R01_PKG_ROOT}/.." CACHE PATH "Retr01 repo root")
set(R01_COMMON_DIR "${R01_PKG_ROOT}/common")
set(R01_ASSETS_DIR "${R01_PKG_ROOT}/assets")
set(R01_ASSETS_PNG "${R01_ASSETS_DIR}/png")
set(R01_ASSETS_OTHER "${R01_ASSETS_DIR}/other")
add_library(r01_play_common
  ${R01_COMMON_DIR}/r01_play_anim.c
  ${R01_COMMON_DIR}/r01_play_anim_cart.c
  ${R01_COMMON_DIR}/r01_play_camera.c
  ${R01_COMMON_DIR}/r01_custom_logic_scan.c
)
target_include_directories(r01_play_common PUBLIC ${R01_COMMON_DIR})
target_include_directories(retr01_sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_link_libraries(retr01_sim_core PUBLIC r01_play_common)

set(R01_OUTPUT_DIR "${R01_REPO_ROOT}/output")
set(R01_TEST_CART "${R01_OUTPUT_DIR}/test.retr01")
set(R01_DEFAULT_CART "${R01_OUTPUT_DIR}/test_2.retr01")

add_library(retr01_sim_app
  src/ui.c
  src/ui_font.c
  src/ui_draw_chip.c
  src/ui_placement.c
  src/ui_draw.c
  src/ui_input.c
  src/ui_pin_net.c
  src/ui_layout.c
  src/app.c
)
target_include_directories(retr01_sim_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_link_libraries(retr01_sim_app PUBLIC retr01_sim_core PkgConfig::SDL2 PkgConfig::FREETYPE)
target_compile_definitions(retr01_sim_app PRIVATE
  "R01S_ASSETS_DIR=\"${R01_ASSETS_OTHER}\""
  "R01S_ASSETS_PNG_DIR=\"${R01_ASSETS_PNG}\"")

add_executable(retr01_sim src/main.c)
target_link_libraries(retr01_sim PRIVATE retr01_sim_app)
target_compile_definitions(retr01_sim PRIVATE "R01S_DEFAULT_CART=\"${R01_DEFAULT_CART}\"")

enable_testing()

function(r01s_unit_test name)
  add_executable(${name} tests/${name}.c)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(${name} PRIVATE retr01_sim_core)
  target_compile_definitions(${name} PRIVATE "R01_TEST_CART=\"${R01_TEST_CART}\"")
  add_test(NAME ${name} COMMAND ${name} ${R01_TEST_CART})
endfunction()

r01s_unit_test(test_timing)
r01s_unit_test(test_entity)
r01s_unit_test(test_bus)
r01s_unit_test(test_gamepad)
r01s_unit_test(test_pwr5v)
r01s_unit_test(test_osc8m)
r01s_unit_test(test_sn74hc14)
r01s_unit_test(test_sn74hc_glue)
r01s_unit_test(test_sn74hc245)
r01s_unit_test(test_sn74hc157)
r01s_unit_test(test_beam_xy)
r01s_unit_test(test_bg_fetch)
r01s_unit_test(test_at28c16)
r01s_unit_test(test_as6c62256)
r01s_unit_test(test_prg_rom)
r01s_unit_test(test_sst39sf040)
r01s_unit_test(test_w65c02s)
r01s_unit_test(test_atmega328p)
r01s_unit_test(test_atmega1284p)
r01s_unit_test(test_atmega32u4)
r01s_unit_test(test_sn74hc595)
r01s_unit_test(test_usbc_pc_host)
r01s_unit_test(test_cart_slot)
r01s_unit_test(test_island_cart_module)
r01s_unit_test(test_island_flasher)
r01s_unit_test(test_cart_flash_flow)
r01s_unit_test(test_island_abcdeghiojklmnp)
r01s_unit_test(test_pads)
r01s_unit_test(test_attiny85)
r01s_unit_test(test_pad_uart)
r01s_unit_test(test_cart)
r01s_unit_test(test_compositor)
r01s_unit_test(test_sprite_fetch)
r01s_unit_test(test_osc_dot)

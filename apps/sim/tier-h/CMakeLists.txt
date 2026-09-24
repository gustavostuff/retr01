cmake_minimum_required(VERSION 3.16)
project(retr01_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

# apps/sim/tier-h. Engine and shared sources are this repo's.
set(NETLIST_SIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../netlist_sim")

if(NOT TARGET netlist_sim)
  add_subdirectory("${NETLIST_SIM_DIR}" "${CMAKE_BINARY_DIR}/netlist_sim")
endif()

add_library(retr01_sim_core
  src/gamepad.c
  src/bom_count.c
  src/board.c
  src/board_netlist.c
  src/board_schematic.c
  src/spi_mailbox.c
  src/board_debug.c
  src/frame_log.c
  src/play.c
  chips/pwr5v.c
  chips/osc8m.c
  chips/atf22v10.c
  chips/i2c_eeprom.c
  chips/sn74hc157.c
  chips/sn74hc573.c
  chips/sn74hc574.c
  chips/avr128db28_m.c
  chips/avr128db28_s1.c
  chips/avr128db28_s2.c
  chips/as6c62256.c
  chips/prg_rom.c
  chips/pads.c
  chips/attiny85.c
  chips/pad_uart.c
  chips/sst39sf040.c
  chips/w65c02s.c
  chips/osc_dot.c
  chips/beam_xy.c
  chips/bg_fetch.c
  chips/at27c256r.c
  chips/compositor.c
  chips/atmega328p.c
  chips/sprite_fetch.c
  chips/integration.c
  src/cart_module.c
  src/cart_slot.c
  src/board_flasher.c
  src/ui_button.c
)
set(R01_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
set(R01_FW_COMMON_DIR "${R01_COMMON_DIR}/fw")
set(R01_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/assets")
set(R01_ASSETS_PNG "${R01_ASSETS_DIR}/png")
set(R01_ASSETS_OTHER "${R01_ASSETS_DIR}/other")
add_library(r01_play_common
  ${R01_COMMON_DIR}/r01_play_anim.c
  ${R01_COMMON_DIR}/r01_play_anim_cart.c
  ${R01_COMMON_DIR}/r01_play_camera.c
  ${R01_COMMON_DIR}/r01_play_physics.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/r01_custom_logic_scan.c
  ${R01_COMMON_DIR}/r01_nes_synth.c
  ${R01_COMMON_DIR}/r01_apu_fd.c
  ${R01_COMMON_DIR}/r01_apu_tracker.c
  ${R01_COMMON_DIR}/r01_bgm_fd.c
)
target_include_directories(r01_play_common PUBLIC ${R01_COMMON_DIR} ${R01_FW_COMMON_DIR})
target_link_libraries(r01_play_common PUBLIC m)
target_include_directories(retr01_sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${R01_COMMON_DIR}
  ${R01_FW_COMMON_DIR}
)
target_link_libraries(retr01_sim_core PUBLIC r01_play_common netlist_sim)

set(R01_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../example_01")
set(R01_EXAMPLE_CART "${R01_OUTPUT_DIR}/example_01.retr01")
set(R01_TEST_CART "${R01_EXAMPLE_CART}")
set(R01_DEFAULT_CART "${R01_EXAMPLE_CART}")
target_compile_definitions(retr01_sim_core PUBLIC
  "R01S_OUTPUT_DIR=\"${R01_OUTPUT_DIR}\"")

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
r01s_unit_test(test_sn74hc157)
r01s_unit_test(test_beam_xy)
r01s_unit_test(test_bg_fetch)
r01s_unit_test(test_at27c256r)
r01s_unit_test(test_as6c62256)
r01s_unit_test(test_prg_rom)
r01s_unit_test(test_sst39sf040)
r01s_unit_test(test_w65c02s)
r01s_unit_test(test_atmega328p)
r01s_unit_test(test_avr128db28_m)
r01s_unit_test(test_avr128db28_s1)
r01s_unit_test(test_spi_mailbox)
r01s_unit_test(test_soft_sel_demux)
r01s_unit_test(test_apu_fd)
r01s_unit_test(test_apu_tracker)
r01s_unit_test(test_sn74hc573)
r01s_unit_test(test_sn74hc574)
r01s_unit_test(test_cart_slot)
r01s_unit_test(test_island_cart_module)
r01s_unit_test(test_island_abcdeghiojklmnp)
add_executable(test_play_camera_clamp tests/test_play_camera_clamp.c)
target_include_directories(test_play_camera_clamp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(test_play_camera_clamp PRIVATE retr01_sim_core)
target_compile_definitions(test_play_camera_clamp PRIVATE
  "R01S_DEFAULT_CART=\"${R01_DEFAULT_CART}\"")
add_test(NAME test_play_camera_clamp COMMAND test_play_camera_clamp ${R01_DEFAULT_CART})
add_executable(test_host_play_smoke tests/test_host_play_smoke.c)
target_include_directories(test_host_play_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(test_host_play_smoke PRIVATE retr01_sim_core)
target_compile_definitions(test_host_play_smoke PRIVATE
  "R01S_DEFAULT_CART=\"${R01_TEST_CART}\"")
add_test(NAME test_host_play_smoke COMMAND test_host_play_smoke ${R01_TEST_CART})
r01s_unit_test(test_pads)
r01s_unit_test(test_attiny85)
r01s_unit_test(test_pad_uart)
r01s_unit_test(test_board_netlist)
r01s_unit_test(test_cart)
r01s_unit_test(test_compositor)
r01s_unit_test(test_sprite_fetch)
r01s_unit_test(test_osc_dot)

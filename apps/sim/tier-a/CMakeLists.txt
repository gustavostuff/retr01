cmake_minimum_required(VERSION 3.16)
project(retr01_sim_tier_a C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

set(DISCRETE_IC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../tools/discrete_ic")
if(NOT TARGET discrete_ic)
  add_subdirectory("${DISCRETE_IC_DIR}" "${CMAKE_BINARY_DIR}/discrete_ic")
endif()
target_compile_definitions(discrete_ic PRIVATE R01A_BB_3PX=1)
if(TARGET test_ns_entity)
  target_compile_definitions(test_ns_entity PRIVATE R01A_BB_3PX=1)
endif()

set(R01_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
set(SIM_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../common")

add_library(retr01_sim_tier_a_core
  ${SIM_COMMON}/chips/sn74hcu04.c
  ${SIM_COMMON}/chips/sn74hc74.c
  ${SIM_COMMON}/chips/rgbs_hdr.c
  chips/atf22v10.c
  ${SIM_COMMON}/chips/at27c256r.c
  ${SIM_COMMON}/src/r01a_rgb_netlist.c
  src/board.c
  src/r01a_bb_snap.c
  ${SIM_COMMON}/src/layout.c
  src/netlist.c
  ${R01_COMMON_DIR}/r01_kit_palette.c
)
target_include_directories(retr01_sim_tier_a_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/include
  ${SIM_COMMON}/chips
  ${SIM_COMMON}/chips
  ${R01_COMMON_DIR}
)
target_compile_definitions(retr01_sim_tier_a_core PRIVATE R01A_BB_3PX=1 R01A_ENTITY_PITCH=6)
target_link_libraries(retr01_sim_tier_a_core PUBLIC discrete_ic)

set(TIER_H "${CMAKE_CURRENT_SOURCE_DIR}/../tier-h")
set(R01_FW_COMMON_DIR "${R01_COMMON_DIR}/fw")
add_library(retr01_sim_tier_a_lab STATIC ${SIM_COMMON}/src/lab_sim.c)
target_include_directories(retr01_sim_tier_a_lab PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/include
  ${SIM_COMMON}/chips
)
target_link_libraries(retr01_sim_tier_a_lab PUBLIC retr01_sim_tier_a_core)
add_library(retr01_sim_tier_a_app
  ${SIM_COMMON}/src/lab_host.c
  ${SIM_COMMON}/src/lab_ui_stubs.c
  ${TIER_H}/src/ui.c
  ${TIER_H}/src/ui_font.c
  ${TIER_H}/src/ui_draw_chip.c
  ${TIER_H}/src/ui_placement.c
  ${TIER_H}/src/ui_draw.c
  ${TIER_H}/src/ui_input.c
  ${TIER_H}/src/ui_pin_net.c
  ${TIER_H}/src/ui_layout.c
  ${TIER_H}/src/ui_button.c
  ${TIER_H}/src/airwire.c
  ${TIER_H}/src/gamepad.c
)
target_include_directories(retr01_sim_tier_a_app PRIVATE
  ${TIER_H}/src
  ${TIER_H}/include
  ${TIER_H}/chips
  ${SIM_COMMON}/include
  ${SIM_COMMON}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/chips
  ${R01_COMMON_DIR}
  ${R01_FW_COMMON_DIR}
)
target_compile_definitions(retr01_sim_tier_a_app PRIVATE
  "R01A_LAB_TITLE=\"Retr01 Tier A\""
  "R01A_ISLAND_LAYOUT=\"${CMAKE_CURRENT_SOURCE_DIR}/island_layout.json\""
  "R01S_LAYOUT_DEFAULT=\"${CMAKE_CURRENT_SOURCE_DIR}/island_layout.json\""
  "R01S_ASSETS_DIR=\"${SIM_COMMON}/assets/other\""
  "R01S_ASSETS_PNG_DIR=\"${SIM_COMMON}/assets/png\""
)
target_link_libraries(retr01_sim_tier_a_app PUBLIC retr01_sim_tier_a_lab retr01_sim_tier_a_core PkgConfig::SDL2 PkgConfig::FREETYPE)

add_executable(retr01_sim_tier_a ${SIM_COMMON}/src/main.c)
target_include_directories(retr01_sim_tier_a PRIVATE
  ${SIM_COMMON}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(retr01_sim_tier_a PRIVATE retr01_sim_tier_a_app)

enable_testing()

function(r01a_unit_test name src)
  add_executable(${name} ${src})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${SIM_COMMON}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/chips
    ${SIM_COMMON}/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_compile_definitions(${name} PRIVATE R01A_BB_3PX=1)
  target_link_libraries(${name} PRIVATE retr01_sim_tier_a_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

r01a_unit_test(test_clock_stage tests/test_clock_stage.c)
r01a_unit_test(test_beam_x tests/test_beam_x.c)
r01a_unit_test(test_beam_y ${SIM_COMMON}/tests/test_beam_y.c)
r01a_unit_test(test_at27c256r ${SIM_COMMON}/tests/test_at27c256r.c)
r01a_unit_test(test_at27_pins tests/test_at27_pins.c)
r01a_unit_test(test_atf_pins tests/test_atf_pins.c)
r01a_unit_test(test_tier_a_bars tests/test_tier_a_bars.c)
r01a_unit_test(test_manual_bb tests/test_manual_bb.c)
r01a_unit_test(test_layout tests/test_layout.c)
r01a_unit_test(test_passives tests/test_passives.c)
r01a_unit_test(test_netlist tests/test_netlist.c)

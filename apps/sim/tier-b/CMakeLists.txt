cmake_minimum_required(VERSION 3.16)
project(retr01_sim_tier_b C)

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

set(R01_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
set(SIM_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../common")

add_library(retr01_sim_tier_b_core
  ${SIM_COMMON}/chips/osc_dot.c
  ${SIM_COMMON}/chips/rgbs_hdr.c
  chips/atf22v10.c
  ${SIM_COMMON}/chips/at27c256r.c
  ${SIM_COMMON}/src/r01a_rgb_netlist.c
  src/board.c
  ${SIM_COMMON}/src/layout.c
  src/netlist.c
  ${R01_COMMON_DIR}/r01_kit_palette.c
)
target_include_directories(retr01_sim_tier_b_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/include
  ${SIM_COMMON}/chips
  ${SIM_COMMON}/chips
  ${R01_COMMON_DIR}
)
target_link_libraries(retr01_sim_tier_b_core PUBLIC discrete_ic)

set(TIER_H "${CMAKE_CURRENT_SOURCE_DIR}/../tier-h")
set(R01_FW_COMMON_DIR "${R01_COMMON_DIR}/fw")
add_library(retr01_sim_tier_b_lab STATIC ${SIM_COMMON}/src/lab_sim.c)
target_include_directories(retr01_sim_tier_b_lab PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/include
  ${SIM_COMMON}/chips
)
target_link_libraries(retr01_sim_tier_b_lab PUBLIC retr01_sim_tier_b_core)
add_library(retr01_sim_tier_b_app
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
target_include_directories(retr01_sim_tier_b_app PRIVATE
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
target_compile_definitions(retr01_sim_tier_b_app PRIVATE
  "R01A_LAB_TITLE=\"Retr01 Tier B\""
  "R01A_ISLAND_LAYOUT=\"${CMAKE_CURRENT_SOURCE_DIR}/island_layout.json\""
  "R01S_ASSETS_DIR=\"${SIM_COMMON}/assets/other\""
  "R01S_ASSETS_PNG_DIR=\"${SIM_COMMON}/assets/png\""
)
target_link_libraries(retr01_sim_tier_b_app PUBLIC retr01_sim_tier_b_lab retr01_sim_tier_b_core PkgConfig::SDL2 PkgConfig::FREETYPE)

add_executable(retr01_sim_tier_b ${SIM_COMMON}/src/main.c)
target_include_directories(retr01_sim_tier_b PRIVATE
  ${SIM_COMMON}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(retr01_sim_tier_b PRIVATE retr01_sim_tier_b_app)

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
  target_link_libraries(${name} PRIVATE retr01_sim_tier_b_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

r01a_unit_test(test_osc_dot ${SIM_COMMON}/tests/test_osc_dot.c)
r01a_unit_test(test_beam_x tests/test_beam_x.c)
r01a_unit_test(test_beam_y ${SIM_COMMON}/tests/test_beam_y.c)
r01a_unit_test(test_at27c256r ${SIM_COMMON}/tests/test_at27c256r.c)
r01a_unit_test(test_compositor tests/test_compositor.c)
r01a_unit_test(test_tier_b_priority tests/test_tier_b_priority.c)
r01a_unit_test(test_tier_b_seat tests/test_tier_b_seat.c)
r01a_unit_test(test_manual_bb ${SIM_COMMON}/tests/test_manual_bb.c)
r01a_unit_test(test_layout tests/test_layout.c)
r01a_unit_test(test_passives tests/test_passives.c)
r01a_unit_test(test_netlist tests/test_netlist.c)

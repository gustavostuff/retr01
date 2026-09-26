cmake_minimum_required(VERSION 3.16)
project(retr01_sim_tier_c C)

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
get_filename_component(R01C_EXAMPLE01_CHR
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../example_01/data/chr_spr0.bin"
  REALPATH
)

add_library(retr01_sim_tier_c_core
  ${SIM_COMMON}/chips/osc_dot.c
  ${SIM_COMMON}/chips/osc_fsc.c
  chips/atf22v10.c
  chips/as6c62256.c
  chips/sn74hc573.c
  chips/avr128db28_s1.c
  ${SIM_COMMON}/chips/at27c256r.c
  ${SIM_COMMON}/chips/ad724.c
  src/board.c
  src/s1_lab.c
  ${SIM_COMMON}/src/layout.c
  src/netlist.c
  ${R01_COMMON_DIR}/r01_kit_palette.c
)
target_include_directories(retr01_sim_tier_c_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SIM_COMMON}/include
  ${SIM_COMMON}/chips
  ${R01_COMMON_DIR}
)
target_compile_definitions(retr01_sim_tier_c_core PRIVATE
  "R01C_EXAMPLE01_CHR=\"${R01C_EXAMPLE01_CHR}\""
)
target_link_libraries(retr01_sim_tier_c_core PUBLIC discrete_ic)

add_library(retr01_sim_tier_c_app
  src/ui.c
  ${SIM_COMMON}/src/ui_font.c
)
target_include_directories(retr01_sim_tier_c_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${SIM_COMMON}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${SIM_COMMON}/chips
)
target_compile_definitions(retr01_sim_tier_c_app PRIVATE
  "R01A_ASSETS_OTHER=\"${SIM_COMMON}/assets/other\""
  "R01A_LAYOUT_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/ui_layout.json\""
  "R01A_TIER_B_LAYOUT=\"${CMAKE_CURRENT_SOURCE_DIR}/../tier-b/ui_layout.json\""
)
target_link_libraries(retr01_sim_tier_c_app PUBLIC retr01_sim_tier_c_core PkgConfig::SDL2 PkgConfig::FREETYPE)

add_executable(retr01_sim_tier_c ${SIM_COMMON}/src/main.c)
target_include_directories(retr01_sim_tier_c PRIVATE
  ${SIM_COMMON}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(retr01_sim_tier_c PRIVATE retr01_sim_tier_c_app)

enable_testing()

function(r01a_unit_test name src)
  add_executable(${name} ${src})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${SIM_COMMON}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/chips
    ${SIM_COMMON}/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(${name} PRIVATE retr01_sim_tier_c_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

r01a_unit_test(test_osc_dot ${SIM_COMMON}/tests/test_osc_dot.c)
r01a_unit_test(test_beam_x tests/test_beam_x.c)
r01a_unit_test(test_beam_y ${SIM_COMMON}/tests/test_beam_y.c)
r01a_unit_test(test_at27c256r ${SIM_COMMON}/tests/test_at27c256r.c)
r01a_unit_test(test_ad724 ${SIM_COMMON}/tests/test_ad724.c)
r01a_unit_test(test_compositor tests/test_compositor.c)
r01a_unit_test(test_tier_c_priority tests/test_tier_c_priority.c)
r01a_unit_test(test_tier_c_field tests/test_tier_c_field.c)
r01a_unit_test(test_tier_c_seat tests/test_tier_c_seat.c)
r01a_unit_test(test_manual_bb ${SIM_COMMON}/tests/test_manual_bb.c)
r01a_unit_test(test_layout tests/test_layout.c)
r01a_unit_test(test_passives tests/test_passives.c)
r01a_unit_test(test_netlist tests/test_netlist.c)

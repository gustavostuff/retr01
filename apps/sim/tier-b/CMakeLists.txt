cmake_minimum_required(VERSION 3.16)
project(retr01_sim_tier_b C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)

set(NETLIST_SIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../netlist_sim")
if(NOT TARGET netlist_sim)
  add_subdirectory("${NETLIST_SIM_DIR}" "${CMAKE_BINARY_DIR}/netlist_sim")
endif()

set(R01_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../common")

add_library(retr01_sim_tier_b_core
  chips/osc_dot.c
  chips/osc_fsc.c
  chips/atf22v10.c
  chips/at27c256r.c
  chips/ad724.c
  src/board.c
  src/layout.c
  src/netlist.c
  ${R01_COMMON_DIR}/r01_kit_palette.c
)
target_include_directories(retr01_sim_tier_b_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
  ${R01_COMMON_DIR}
)
target_link_libraries(retr01_sim_tier_b_core PUBLIC netlist_sim)

add_library(retr01_sim_tier_b_app
  src/ui.c
  src/ui_font.c
)
target_include_directories(retr01_sim_tier_b_app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_compile_definitions(retr01_sim_tier_b_app PRIVATE
  "R01A_ASSETS_OTHER=\"${CMAKE_CURRENT_SOURCE_DIR}/assets/other\""
  "R01A_LAYOUT_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/ui_layout.json\""
  "R01A_TIER_A_LAYOUT=\"${CMAKE_CURRENT_SOURCE_DIR}/../tier-a/ui_layout.json\""
)
target_link_libraries(retr01_sim_tier_b_app PUBLIC retr01_sim_tier_b_core PkgConfig::SDL2 PkgConfig::FREETYPE)

add_executable(retr01_sim_tier_b src/main.c)
target_include_directories(retr01_sim_tier_b PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(retr01_sim_tier_b PRIVATE retr01_sim_tier_b_app)

enable_testing()

function(r01a_unit_test name)
  add_executable(${name} tests/${name}.c)
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${CMAKE_CURRENT_SOURCE_DIR}/chips
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(${name} PRIVATE retr01_sim_tier_b_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

r01a_unit_test(test_osc_dot)
r01a_unit_test(test_beam_x)
r01a_unit_test(test_beam_y)
r01a_unit_test(test_at27c256r)
r01a_unit_test(test_ad724)
r01a_unit_test(test_compositor)
r01a_unit_test(test_tier_b_priority)
r01a_unit_test(test_tier_b_seat)
r01a_unit_test(test_manual_bb)
r01a_unit_test(test_layout)
r01a_unit_test(test_passives)
r01a_unit_test(test_netlist)

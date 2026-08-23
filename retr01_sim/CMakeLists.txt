cmake_minimum_required(VERSION 3.16)
project(retr01_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(retr01_sim_core
  src/pin.c
  src/entity.c
  src/bus.c
  chips/stub14.c
  chips/pwr5v.c
  chips/osc8m.c
  chips/sn74hc14.c
  chips/as6c62256.c
  chips/prg_rom.c
  chips/w65c02s.c
)
target_include_directories(retr01_sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)

add_library(retr01_sim_app
  src/ui.c
  src/app.c
)
target_include_directories(retr01_sim_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_link_libraries(retr01_sim_app PUBLIC retr01_sim_core PkgConfig::SDL2)

add_executable(retr01_sim src/main.c)
target_link_libraries(retr01_sim PRIVATE retr01_sim_app)

enable_testing()

function(r01s_unit_test name)
  add_executable(${name} tests/${name}.c)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(${name} PRIVATE retr01_sim_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

r01s_unit_test(test_entity)
r01s_unit_test(test_pwr5v)
r01s_unit_test(test_osc8m)
r01s_unit_test(test_sn74hc14)
r01s_unit_test(test_as6c62256)
r01s_unit_test(test_prg_rom)
r01s_unit_test(test_w65c02s)

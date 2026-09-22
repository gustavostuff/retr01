cmake_minimum_required(VERSION 3.16)
project(netlist_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(netlist_sim
  src/pin.c
  src/entity.c
  src/bus.c
  src/island.c
  src/island_group.c
  src/island_builder.c
  src/timing.c
  src/chip.c
  src/ns_island.c
  chips/breadboard.c
  chips/passive.c
  chips/video_sink.c
)

target_include_directories(netlist_sim PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_link_libraries(netlist_sim PUBLIC PkgConfig::SDL2)

enable_testing()

add_executable(test_ns_entity tests/test_entity.c)
target_link_libraries(test_ns_entity PRIVATE netlist_sim)
add_test(NAME test_ns_entity COMMAND test_ns_entity)

add_executable(test_ns_bus tests/test_bus.c)
target_link_libraries(test_ns_bus PRIVATE netlist_sim)
add_test(NAME test_ns_bus COMMAND test_ns_bus)

add_executable(test_ns_chip_registry tests/test_chip_registry.c)
target_link_libraries(test_ns_chip_registry PRIVATE netlist_sim)
add_test(NAME test_ns_chip_registry COMMAND test_ns_chip_registry)

add_executable(test_ns_health_outline tests/test_health_outline.c)
target_link_libraries(test_ns_health_outline PRIVATE netlist_sim)
add_test(NAME test_ns_health_outline COMMAND test_ns_health_outline)

# Example only when this tree is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(example_nand examples/example_nand_IC.c examples/example_nand_island.c examples/example_nand_main.c)
  target_link_libraries(example_nand PRIVATE netlist_sim)
endif()

cmake_minimum_required(VERSION 3.16)
project(retr01_emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(retr01_emu_core
  src/cart.c
  src/cpu.c
  src/ppu.c
  src/machine.c
)
target_include_directories(retr01_emu_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(retr01_emu src/main.c)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)

enable_testing()

add_executable(test_cart tests/test_cart.c)
target_link_libraries(test_cart PRIVATE retr01_emu_core)
add_test(NAME test_cart COMMAND test_cart
  ${CMAKE_SOURCE_DIR}/../studio/project.retr01)

add_executable(test_boot tests/test_boot.c)
target_link_libraries(test_boot PRIVATE retr01_emu_core)
add_test(NAME test_boot COMMAND test_boot
  ${CMAKE_SOURCE_DIR}/../studio/project.retr01)

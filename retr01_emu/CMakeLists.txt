cmake_minimum_required(VERSION 3.16)
project(retr01_emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(R01E_CORE_SRCS
  src/cart.c
  src/cpu.c
  src/io.c
  src/video.c
  src/play.c
  src/machine.c
)

add_library(retr01_emu_core ${R01E_CORE_SRCS})
target_include_directories(retr01_emu_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(retr01_emu src/main.c)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)

enable_testing()

set(R01E_TEST_CART ${CMAKE_SOURCE_DIR}/../retr01_studio/test_game/test.retr01)

add_executable(test_cart tests/test_cart.c)
target_link_libraries(test_cart PRIVATE retr01_emu_core)
add_test(NAME test_cart COMMAND test_cart ${R01E_TEST_CART})

add_executable(test_boot tests/test_boot.c)
target_link_libraries(test_boot PRIVATE retr01_emu_core)
add_test(NAME test_boot COMMAND test_boot ${R01E_TEST_CART})

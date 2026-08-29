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

set(R01_REPO_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Retr01 repo root")
set(R01_ROM_DIR "${R01_REPO_ROOT}/rom")
set(R01_TEST_CART "${R01_ROM_DIR}/test.retr01")

target_compile_definitions(retr01_emu_core PUBLIC "R01_DEFAULT_CART=\"${R01_TEST_CART}\"")

add_executable(test_cart tests/test_cart.c)
target_link_libraries(test_cart PRIVATE retr01_emu_core)
add_test(NAME test_cart COMMAND test_cart ${R01_TEST_CART})

add_executable(test_boot tests/test_boot.c)
target_link_libraries(test_boot PRIVATE retr01_emu_core)
add_test(NAME test_boot COMMAND test_boot ${R01_TEST_CART})

add_executable(test_play tests/test_play.c)
target_link_libraries(test_play PRIVATE retr01_emu_core)
add_test(NAME test_play COMMAND test_play ${R01_TEST_CART})

add_executable(test_cpu tests/test_cpu.c)
target_link_libraries(test_cpu PRIVATE retr01_emu_core)
add_test(NAME test_cpu COMMAND test_cpu ${R01_TEST_CART})

add_executable(test_io tests/test_io.c)
target_link_libraries(test_io PRIVATE retr01_emu_core)
add_test(NAME test_io COMMAND test_io ${R01_TEST_CART})

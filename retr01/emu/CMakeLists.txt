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

# Package root is retr01/ (sibling: common, studio, sim, assets).
set(R01_PKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
# Repo root holds docs/, hw/, output/, bin/.
set(R01_REPO_ROOT "${R01_PKG_ROOT}/.." CACHE PATH "Retr01 repo root")
set(R01_COMMON_DIR "${R01_PKG_ROOT}/common")
set(R01_ASSETS_DIR "${R01_PKG_ROOT}/assets")

# CURRENT so nested add_subdirectory from Studio still finds common/.
if(NOT TARGET r01_play_common)
  add_library(r01_play_common
    ${R01_COMMON_DIR}/r01_play_anim.c
    ${R01_COMMON_DIR}/r01_play_anim_cart.c
    ${R01_COMMON_DIR}/r01_play_camera.c
    ${R01_COMMON_DIR}/r01_custom_logic_scan.c
  )
  target_include_directories(r01_play_common PUBLIC ${R01_COMMON_DIR})
endif()

if(NOT TARGET retr01_emu_core)
  add_library(retr01_emu_core ${R01E_CORE_SRCS})
  target_include_directories(retr01_emu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(retr01_emu_core PUBLIC r01_play_common)
endif()

set(R01_OUTPUT_DIR "${R01_REPO_ROOT}/output")
set(R01_TEST_CART "${R01_OUTPUT_DIR}/test.retr01")
target_compile_definitions(retr01_emu_core PUBLIC "R01_DEFAULT_CART=\"${R01_TEST_CART}\"")

# Standalone app + tests only when this is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(retr01_emu src/main.c)
  target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)

  enable_testing()

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
endif()

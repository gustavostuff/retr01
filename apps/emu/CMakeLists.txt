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

# Package root is apps/ (sibling: common, studio, assets).
set(R01_PKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
# Repo root holds docs/, hw/, output/, bin/.
set(R01_REPO_ROOT "${R01_PKG_ROOT}/.." CACHE PATH "Retr01 repo root")
set(R01_COMMON_DIR "${R01_PKG_ROOT}/common")
set(R01_ASSETS_DIR "${R01_PKG_ROOT}/assets")

# CURRENT so nested add_subdirectory from Studio still finds common/.
if(NOT TARGET r01_play_common)
  set(R01_FW_COMMON_DIR "${R01_PKG_ROOT}/common/fw")
  add_library(r01_play_common
    ${R01_COMMON_DIR}/r01_play_anim.c
    ${R01_COMMON_DIR}/r01_play_anim_cart.c
    ${R01_COMMON_DIR}/r01_play_camera.c
    ${R01_COMMON_DIR}/r01_custom_logic_scan.c
    ${R01_COMMON_DIR}/r01_nes_synth.c
    ${R01_COMMON_DIR}/r01_apu_fd.c
    ${R01_COMMON_DIR}/r01_apu_tracker.c
    ${R01_COMMON_DIR}/r01_bgm_fd.c
  )
  target_include_directories(r01_play_common PUBLIC ${R01_COMMON_DIR} ${R01_FW_COMMON_DIR})
  target_link_libraries(r01_play_common PUBLIC m)
endif()

if(NOT TARGET retr01_emu_core)
  add_library(retr01_emu_core ${R01E_CORE_SRCS})
  target_include_directories(retr01_emu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(retr01_emu_core PUBLIC r01_play_common)
endif()

set(R01_OUTPUT_DIR "${R01_REPO_ROOT}/output")
# No default cart: tests/app take cart path via argv.

# Standalone app + tests only when this is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(retr01_emu src/main.c ${R01_COMMON_DIR}/r01_bgm_host.c ${R01_COMMON_DIR}/r01_pad_keys.c)
  target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)

  enable_testing()

  # Cart-backed tests require argv cart path; skip when no fixture present.
  set(R01_TEST_CART "${R01_OUTPUT_DIR}/test.retr01")
  set(_r01e_cart_tests test_cart test_boot test_play test_cpu test_io)
  foreach(_t ${_r01e_cart_tests})
    add_executable(${_t} tests/${_t}.c)
    target_link_libraries(${_t} PRIVATE retr01_emu_core)
    if(EXISTS "${R01_TEST_CART}")
      add_test(NAME ${_t} COMMAND ${_t} ${R01_TEST_CART})
    else()
      add_test(NAME ${_t} COMMAND ${_t})
      set_tests_properties(${_t} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
  endforeach()
endif()

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
    ${R01_COMMON_DIR}/r01_kit_palette.c
    ${R01_COMMON_DIR}/r01_play_anim.c
    ${R01_COMMON_DIR}/r01_play_anim_cart.c
    ${R01_COMMON_DIR}/r01_play_camera.c
    ${R01_COMMON_DIR}/r01_play_physics.c
    ${R01_COMMON_DIR}/r01_custom_logic_scan.c
    ${R01_COMMON_DIR}/r01_nes_synth.c
    ${R01_COMMON_DIR}/r01_apu_fd.c
    ${R01_COMMON_DIR}/r01_apu_tracker.c
    ${R01_COMMON_DIR}/r01_apu_mix.c
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
  add_executable(retr01_emu src/main.c ${R01_COMMON_DIR}/r01_bgm_host.c ${R01_COMMON_DIR}/r01_pad_keys.c
                 ${R01_COMMON_DIR}/r01_pad_host.c ${R01_COMMON_DIR}/r01_readme_shot.c)
  target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)
  target_compile_definitions(retr01_emu PRIVATE "R01_REPO_ROOT=\"${R01_REPO_ROOT}\"")

  enable_testing()

  # Self-contained unit tests (in-memory stub cart; no ROM / project fixture).
  foreach(_t test_cart test_cpu test_io test_apu)
    add_executable(${_t} tests/${_t}.c)
    target_include_directories(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(${_t} PRIVATE retr01_emu_core)
    add_test(NAME ${_t} COMMAND ${_t})
  endforeach()

  # Optional integration binaries (need a real .retr01 on argv). Not part of ctest.
  foreach(_t test_boot test_play)
    add_executable(${_t} tests/${_t}.c)
    target_link_libraries(${_t} PRIVATE retr01_emu_core)
  endforeach()
endif()

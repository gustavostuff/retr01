cmake_minimum_required(VERSION 3.16)
project(retr01_emu C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(R01_PKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(R01_REPO_ROOT "${R01_PKG_ROOT}/.." CACHE PATH "Retr01 pack root")
set(R01_COMMON_DIR "${R01_PKG_ROOT}/common")
set(R01_FW_COMMON_DIR "${R01_COMMON_DIR}/fw")

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

add_library(retr01_emu_core
  src/cart.c
  src/cpu.c
  src/io.c
  src/video.c
  src/play.c
  src/machine.c
)
target_include_directories(retr01_emu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(retr01_emu_core PUBLIC r01_play_common)

add_executable(retr01_emu
  src/main.c
  ${R01_COMMON_DIR}/r01_bgm_host.c
  ${R01_COMMON_DIR}/r01_pad_keys.c
  ${R01_COMMON_DIR}/r01_pad_host.c
  ${R01_COMMON_DIR}/r01_readme_shot.c
)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core PkgConfig::SDL2)
target_compile_definitions(retr01_emu PRIVATE
  "R01_REPO_ROOT=\"${R01_REPO_ROOT}\""
  R01E_NO_DEBUG=1
  R01_README_SHOT=0
)

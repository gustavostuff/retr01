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
    ${R01_COMMON_DIR}/r01_play_collision.c
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
  target_link_libraries(retr01_emu_core PUBLIC r01_play_common ${CMAKE_DL_LIBS})
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

  # Optional llvm-mos PRG boot test (tools/llvm-mos or $LLVM_MOS).
  set(R01_LLVM_MOS "$ENV{LLVM_MOS}")
  if(NOT R01_LLVM_MOS)
    set(R01_LLVM_MOS "${R01_REPO_ROOT}/tools/llvm-mos")
  endif()
  set(R01_SDK_PRG "${R01_REPO_ROOT}/apps/sdk/r01_c/build/retr01.prg")
  add_executable(test_sdk_prg tests/test_sdk_prg.c)
  target_include_directories(test_sdk_prg PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(test_sdk_prg PRIVATE retr01_emu_core)
  if(EXISTS "${R01_LLVM_MOS}/bin/mos-common-clang")
    add_custom_command(
      OUTPUT "${R01_SDK_PRG}"
      COMMAND "${R01_REPO_ROOT}/apps/sdk/r01_c/build-prg.sh"
      DEPENDS
        "${R01_REPO_ROOT}/apps/sdk/r01_c/build-prg.sh"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/ld/retr01.ld"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/asm/boot.s"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/asm/nmi.s"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/hw.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/boot.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/game.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/play_tick.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/tracker.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/map_win.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/oam_pa.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/src/main.c"
        "${R01_REPO_ROOT}/apps/common/r01_play_camera.c"
        "${R01_REPO_ROOT}/apps/common/r01_play_physics.c"
        "${R01_REPO_ROOT}/apps/common/r01_play_collision.c"
        "${R01_REPO_ROOT}/apps/common/r01_play_anim.c"
        "${R01_REPO_ROOT}/apps/common/r01_play_anim_cart.c"
        "${R01_REPO_ROOT}/apps/sdk/r01_c/game_logic.c"
      COMMENT "llvm-mos Retr01 PRG"
      VERBATIM
    )
    add_custom_target(r01_sdk_prg DEPENDS "${R01_SDK_PRG}")
    add_dependencies(test_sdk_prg r01_sdk_prg)
    add_test(NAME test_sdk_prg COMMAND test_sdk_prg "${R01_SDK_PRG}")
  endif()

  # Optional integration binaries (need a real .retr01 on argv). Registered when the sample cart exists.
  set(R01E_EXAMPLE_CART "${R01_REPO_ROOT}/example_01/example_01.retr01")
  foreach(_t test_boot test_play)
    add_executable(${_t} tests/${_t}.c)
    target_link_libraries(${_t} PRIVATE retr01_emu_core)
    if(EXISTS "${R01E_EXAMPLE_CART}")
      add_test(NAME ${_t} COMMAND ${_t} "${R01E_EXAMPLE_CART}")
    endif()
  endforeach()
endif()

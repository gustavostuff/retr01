cmake_minimum_required(VERSION 3.16)
project(retr01_sdk_c_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

set(R01_SDK_C "${CMAKE_CURRENT_SOURCE_DIR}")
set(R01_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../../common")

enable_testing()

add_executable(test_game_logic
  tests/test_game_logic.c
  src/game.c
  src/hw.c
  src/boot.c
  src/play_tick.c
  game_logic.c
  ${R01_COMMON}/r01_play_camera.c
  ${R01_COMMON}/r01_play_physics.c
  ${R01_COMMON}/r01_play_collision.c
)
target_include_directories(test_game_logic PRIVATE ${R01_SDK_C}/include ${R01_COMMON})
target_compile_definitions(test_game_logic PRIVATE R01_HOST_TEST)
add_test(NAME test_game_logic COMMAND test_game_logic)

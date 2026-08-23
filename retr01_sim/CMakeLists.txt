cmake_minimum_required(VERSION 3.16)
project(retr01_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

add_library(retr01_sim_core
  src/pin.c
  src/entity.c
  chips/stub14.c
)
target_include_directories(retr01_sim_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)

add_library(retr01_sim_app
  src/ui.c
  src/app.c
)
target_include_directories(retr01_sim_app PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/chips
)
target_link_libraries(retr01_sim_app PUBLIC retr01_sim_core PkgConfig::SDL2)

add_executable(retr01_sim src/main.c)
target_link_libraries(retr01_sim PRIVATE retr01_sim_app)

enable_testing()
add_executable(test_entity tests/test_entity.c)
target_link_libraries(test_entity PRIVATE retr01_sim_core)
add_test(NAME entity COMMAND test_entity)

cmake_minimum_required(VERSION 3.16)
project(retr01 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RETR01_BUILD_TESTS "Build unit tests" ON)
option(RETR01_BUILD_EMU "Build retr01_emu" ON)
option(RETR01_BUILD_STUDIO "Build retr01_studio UI" OFF)

include(CTest)

add_subdirectory(retr01_world_studio)

if(RETR01_BUILD_EMU)
    add_subdirectory(retr01_emu)
endif()

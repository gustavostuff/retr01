add_library(retr01_core STATIC
    src/screen.c
    src/rle.c
    src/palette.c
    src/cart.c
    src/map.c
    src/project_io.c
)

target_include_directories(retr01_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(retr01_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(retr01::core ALIAS retr01_core)

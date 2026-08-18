add_library(retr01_pack STATIC src/chr_pack.c)
target_include_directories(retr01_pack PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(retr01_pack PUBLIC retr01_core)
target_compile_options(retr01_pack PRIVATE -Wall -Wextra -Wpedantic)

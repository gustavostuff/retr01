find_package(SDL2 REQUIRED)

add_library(retr01_emu_core STATIC
    src/ppu.c
    src/cpu.c
    src/bus.c
    src/emu.c
)

target_include_directories(retr01_emu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(retr01_emu_core PUBLIC retr01_core)
target_compile_options(retr01_emu_core PRIVATE -Wall -Wextra)

add_executable(retr01_emu src/main.c)
target_link_libraries(retr01_emu PRIVATE retr01_emu_core SDL2::SDL2)
target_compile_definitions(retr01_emu PRIVATE
    RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
)

if(RETR01_BUILD_TESTS)
    add_executable(test_ppu_blit test/test_ppu_blit.c)
    target_link_libraries(test_ppu_blit PRIVATE retr01_emu_core)
    target_include_directories(test_ppu_blit PRIVATE
        ${CMAKE_SOURCE_DIR}/retr01_world_studio/tests
    )
    target_compile_definitions(test_ppu_blit PRIVATE
        RETR01_PALETTE_V01_PATH="${CMAKE_SOURCE_DIR}/retr01_world_studio/retr01_palette_v_01.txt"
    )
    add_test(NAME test_ppu_blit COMMAND test_ppu_blit)

    add_executable(test_bus_decode test/test_bus_decode.c)
    target_link_libraries(test_bus_decode PRIVATE retr01_emu_core)
    target_include_directories(test_bus_decode PRIVATE
        ${CMAKE_SOURCE_DIR}/retr01_world_studio/tests
    )
    add_test(NAME test_bus_decode COMMAND test_bus_decode)

    add_executable(test_cpu_rom test/test_cpu_rom.c test/nmi_prg.c)
    target_link_libraries(test_cpu_rom PRIVATE retr01_emu_core)
    target_include_directories(test_cpu_rom PRIVATE
        ${CMAKE_SOURCE_DIR}/retr01_world_studio/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
    add_test(NAME test_cpu_rom COMMAND test_cpu_rom)

    add_executable(gen_cpu_nmi_cart test/gen_cpu_nmi_cart.c test/nmi_prg.c)
    target_link_libraries(gen_cpu_nmi_cart PRIVATE retr01_core)
    target_include_directories(gen_cpu_nmi_cart PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

    set(RETR01_CPU_NMI_CART "${CMAKE_CURRENT_BINARY_DIR}/cpu_nmi.retr01")
    add_custom_command(
        OUTPUT "${RETR01_CPU_NMI_CART}"
        COMMAND gen_cpu_nmi_cart "${RETR01_CPU_NMI_CART}"
        DEPENDS gen_cpu_nmi_cart
        VERBATIM
    )
    add_custom_target(cpu_nmi_cart ALL DEPENDS "${RETR01_CPU_NMI_CART}")
    add_test(NAME test_emu_headless_nmi
        COMMAND retr01_emu --cart "${RETR01_CPU_NMI_CART}" --headless --frames 60
    )
endif()

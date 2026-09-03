#include "retr01_sim/flasher_bench.h"
#include "test_common.h"

int main(void) {
    R01sFlasherBench bench;
    R01sIslandGroup *group;

    expect_true(r01s_flasher_bench_build(&bench) == 0, "build bench");
    group = r01s_flasher_bench_group(&bench);
    expect_true(r01s_island_group_count(group) == R01S_BENCH_ISLAND_COUNT, "three islands");
    expect_true(r01s_island_group_at(group, R01S_BENCH_ISLAND_FLASHER)->entity_count == 4,
                "mcu + 2x595 + usb");

    r01s_flasher_bench_shutdown(&bench);
    return test_done("test_island_flasher");
}

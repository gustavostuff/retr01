#include "retr01_sim/board.h"
#include "retr01_sim/bus.h"
#include "retr01_sim/island_builder.h"
#include "test_common.h"
#include "w65c02s.h"

/*
 * Layer-2 islands A–E smoke:
 *   A power, B clock, C CPU/RAM/PRG, D $FE02 latch, E $FE60 pads.
 * PRG: LDA #$55 / STA $FE02 / LDA $FE60 / JMP $8000
 */
int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;
    R01sIslandGroup *group;
    R01sBoard *b;
    int i;
    int saw_latch = 0;
    int saw_pad = 0;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    group = r01s_island_builder_group(&builder);
    expect_true(group != NULL, "group");
    expect_true(r01s_island_group_count(group) == 5, "5 islands");

    b = r01s_board_from_group(group);
    expect_true(b != NULL, "board ctx");
    r01s_pads_set(&b->pads, 0, 0xA5);

    for (i = 0; i < 400; i++) {
        r01s_island_group_step(group);
        if (r01s_sn74hc573_peek_q(&b->latch) == 0x55) {
            saw_latch = 1;
        }
        if (r01s_w65c02s_a(&b->cpu) == 0xA5) {
            saw_pad = 1;
        }
        if (saw_latch && saw_pad) {
            break;
        }
    }

    expect_true(saw_latch, "STA $FE02 hit island D latch");
    expect_true(saw_pad, "LDA $FE60 read island E pads");
    expect_true(b->cycles > 0, "CPU cycles advanced");
    expect_true(r01s_bus_conflict_count() == 0, "no bus fight");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_island_abcde");
}

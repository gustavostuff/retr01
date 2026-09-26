#include "r01a_board.h"

#include "test_common.h"

int main(void) {
    R01aBoard board;

    r01a_board_init(&board);
    expect_true(board.extra_bb_count == 0, "code starts with BB1, like Tier B");
    expect_true(r01a_avr128db28_s1_entity(&board.mcu_s1) != NULL, "S1 is present");
    expect_true(r01a_as6c62256_entity(&board.field_sram) != NULL, "field SRAM is present");
    expect_true(r01a_sn74hc573_entity(&board.field_latch) != NULL, "field latch is present");
    expect_true(r01a_board_entity_by_refdes(&board, "C8") != NULL, "S1 decoupling cap");
    r01a_board_shutdown(&board);
    return test_done("test_tier_c_seat");
}

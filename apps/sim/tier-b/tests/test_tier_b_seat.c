#include "r01a_board.h"

#include "test_common.h"

int main(void) {
    R01aBoard board;

    r01a_board_init(&board);
    expect_true(board.extra_bb_count == 0, "code starts with BB1, like Tier A");
    expect_true(r01a_atf22v10_entity(&board.compositor) != NULL, "compositor is present");
    expect_true(r01a_board_entity_by_refdes(&board, "C8") == NULL, "no extra decoupling cap");
    r01a_board_shutdown(&board);
    return test_done("test_tier_b_seat");
}

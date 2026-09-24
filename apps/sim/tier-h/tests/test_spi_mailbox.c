#include "avr128db28_m.h"
#include "avr128db28_s1.h"
#include "avr128db28_s2.h"
#include "retr01_sim/board.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/spi_mailbox.h"
#include "test_common.h"

int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;

    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");

    r01s_board_poke_fe(&board, 0x00u, 0xA5);
    expect_true(r01s_board_peek_fe(&board, 0x00u) == 0xA5, "peek FE00 soft");
    r01s_board_poke_fe(&board, 0x90u, 0x11);
    expect_true(r01s_board_peek_fe(&board, 0x90u) == 0x11, "peek FE90 soft");

    r01s_board_poke_fe(&board, 0x20u, 0x00);
    r01s_board_poke_fe(&board, 0x21u, 0x10);
    r01s_board_poke_fe(&board, 0x21u, 0x01);
    r01s_board_poke_fe(&board, 0x21u, 0x00);
    r01s_board_poke_fe(&board, 0x21u, 0x20);
    r01s_spi_mailbox_flush(&board);
    expect_true(r01s_avr128db28_s1_oam_peek(&board.mcu_s1, 0) == 0x10, "mailbox OAM Y");
    expect_true(r01s_avr128db28_s1_oam_peek(&board.mcu_s1, 1) == 0x01, "mailbox OAM tile");
    expect_true(r01s_avr128db28_s1_oam_peek(&board.mcu_s1, 3) == 0x20, "mailbox OAM X");

    r01s_board_poke_fe(&board, 0x41u, 0x10);
    r01s_board_poke_fe(&board, 0x42u, 0x00);
    r01s_board_poke_fe(&board, 0x40u, 0x8F);
    r01s_spi_mailbox_flush(&board);
    expect_true(r01s_avr128db28_s2_peek(&board.mcu_s2, 0) == 0x8F, "mailbox APU FE40");
    expect_true(r01s_avr128db28_s2_peek(&board.mcu_s2, 1) == 0x10, "mailbox APU FE41");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_spi_mailbox");
}

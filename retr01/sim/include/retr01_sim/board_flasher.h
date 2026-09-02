#ifndef retr01_SIM_BOARD_FLASHER_H
#define retr01_SIM_BOARD_FLASHER_H

struct R01sBoard;

/* Cart module + console socket only. Flasher bench is separate (flasher_bench). */
void r01s_board_init_cart_hw(struct R01sBoard *board);

#endif

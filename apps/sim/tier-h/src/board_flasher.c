#include "retr01_sim/board.h"

#include "retr01_sim/cart_slot.h"

void r01s_board_init_cart_hw(R01sBoard *board) {
    if (!board) {
        return;
    }
    r01s_cart_module_init(&board->cart_module);
    r01s_cart_slot_reset(&board->cart_slot);
    board->cart_mod_impl.module = &board->cart_module;
    (void)r01s_cart_slot_insert(&board->cart_slot, &board->cart_module, R01S_CART_SLOT_MOBO);
    board->cart_impl.flash = &board->cart_module.flash;
    board->cart_impl.save_eeprom = &board->cart_module.save;
}

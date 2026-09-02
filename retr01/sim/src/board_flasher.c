#include "retr01_sim/board.h"

#include "retr01_sim/cart_slot.h"

void r01s_board_init_flasher_hw(R01sBoard *board) {
    if (!board) {
        return;
    }
    r01s_cart_module_init(&board->cart_module);
    r01s_cart_slot_reset(&board->cart_slot);
    r01s_usbc_receptacle_init(&board->flasher_usb, "JUSB");
    r01s_atmega32u4_init(&board->flasher_mcu, "U32U4");
    r01s_sn74hc595_init(&board->flasher_shift_lo, "U595A");
    r01s_sn74hc595_init(&board->flasher_shift_hi, "U595B");
    board->flasher_impl.mcu = &board->flasher_mcu;
    board->flasher_impl.shift_lo = &board->flasher_shift_lo;
    board->flasher_impl.shift_hi = &board->flasher_shift_hi;
    board->flasher_impl.usb = &board->flasher_usb;
    board->cart_mod_impl.module = &board->cart_module;
    (void)r01s_cart_slot_insert(&board->cart_slot, &board->cart_module, R01S_CART_SLOT_MOBO);
    board->cart_impl.flash = &board->cart_module.flash;
    board->cart_impl.save_eeprom = &board->cart_module.save;
}

#include "retr01_sim/board.h"

#include "retr01_sim/cart_slot.h"
#include "retr01_sim/play.h"

#include <stdio.h>
#include <string.h>

static R01sBoard *g_flash_board;
static int g_flash_running;

static void board_bind_flasher_flash(R01sBoard *board) {
    R01sSst39sf040 *flash = NULL;
    int present = 0;
    if (!board) {
        return;
    }
    if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_FLASHER)) {
        flash = &board->cart_module.flash;
        present = 1;
    }
    r01s_atmega32u4_bind_flash(&board->flasher_mcu, flash, present);
}

static void board_clear_mobo_cart(R01sBoard *board) {
    board->cart_impl.flash = NULL;
    board->cart_impl.save_eeprom = NULL;
}

static int board_ensure_cart_flasher(R01sBoard *board) {
    if (!board) {
        return -1;
    }
    if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_FLASHER)) {
        return 0;
    }
    if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_MOBO)) {
        (void)r01s_cart_slot_remove(&board->cart_slot, R01S_CART_SLOT_MOBO);
        board_clear_mobo_cart(board);
    }
    if (r01s_cart_slot_insert(&board->cart_slot, &board->cart_module, R01S_CART_SLOT_FLASHER) != 0) {
        return -1;
    }
    board_bind_flasher_flash(board);
    return 0;
}

int r01s_board_cart_inserted(const R01sBoard *board) {
    if (!board) {
        return 0;
    }
    return r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_MOBO) ||
           r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_FLASHER);
}

void r01s_board_toggle_cart(R01sBoard *board) {
    if (!board) {
        return;
    }
    if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_MOBO)) {
        (void)r01s_cart_slot_remove(&board->cart_slot, R01S_CART_SLOT_MOBO);
        board_clear_mobo_cart(board);
        return;
    }
    if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_FLASHER)) {
        (void)r01s_cart_slot_remove(&board->cart_slot, R01S_CART_SLOT_FLASHER);
        board_bind_flasher_flash(board);
        return;
    }
    if (r01s_cart_slot_insert(&board->cart_slot, &board->cart_module, R01S_CART_SLOT_MOBO) == 0) {
        board->cart_impl.flash = &board->cart_module.flash;
        board->cart_impl.save_eeprom = &board->cart_module.save;
    }
}

const char *r01s_board_cart_path(const R01sBoard *board) {
    if (!board || board->cart_path[0] == '\0') {
        return NULL;
    }
    return board->cart_path;
}

int r01s_board_select_cart(R01sBoard *board, const char *path) {
    if (!board || !path || path[0] == '\0') {
        return -1;
    }
    if (r01s_board_cart_inserted(board)) {
        return -1;
    }
    if (g_flash_running) {
        return -1;
    }
    if (r01s_board_load_cart(board, path) != 0) {
        return -1;
    }
    r01s_play_reset(&board->play);
    return 0;
}

int r01s_board_start_flash(R01sBoard *board, const char *rom_path) {
    const char *path = rom_path;
    if (!board) {
        return -1;
    }
    if (board_ensure_cart_flasher(board) != 0) {
        r01s_cart_slot_log_warn("flasher: no cart inserted");
        return -1;
    }
    if (!path || path[0] == '\0') {
        path = board->cart_path[0] ? board->cart_path : NULL;
    }
    if (!path) {
        return -1;
    }
    if (r01s_pc_host_load_file(&board->pc_host, path) != 0) {
        return -1;
    }
    r01s_atmega32u4_reset_program(&board->flasher_mcu);
    g_flash_board = board;
    g_flash_running = 1;
    return r01s_pc_host_start_stream(&board->pc_host) ? 0 : -1;
}

int r01s_board_flash_active(const R01sBoard *board) {
    (void)board;
    return g_flash_running;
}

int r01s_board_flash_poll(R01sBoard *board, int budget) {
    int n = 0;
    if (!board || !g_flash_running) {
        return 0;
    }
    n += r01s_pc_host_stream_tick(&board->pc_host, budget > 0 ? budget : 64);
    n += r01s_atmega32u4_service(&board->flasher_mcu, budget > 0 ? budget : 256);
    if (r01s_pc_host_stream_done(&board->pc_host) && !r01s_usbc_host_pending(&board->flasher_usb) &&
        !r01s_atmega32u4_busy(&board->flasher_mcu) &&
        r01s_atmega32u4_bytes_programmed(&board->flasher_mcu) >= board->pc_host.rom_len) {
        g_flash_running = 0;
        g_flash_board = NULL;
        if (r01s_cart_slot_present(&board->cart_slot, R01S_CART_SLOT_MOBO)) {
            board->cart_loaded = 1;
        }
    }
    return n;
}

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
    r01s_pc_host_init(&board->pc_host, "PC1");
    r01s_atmega32u4_bind_usb(&board->flasher_mcu, &board->flasher_usb);
    r01s_atmega32u4_bind_shifts(&board->flasher_mcu, &board->flasher_shift_lo, &board->flasher_shift_hi);
    r01s_pc_host_bind_usb(&board->pc_host, &board->flasher_usb);
    board->flasher_impl.mcu = &board->flasher_mcu;
    board->flasher_impl.shift_lo = &board->flasher_shift_lo;
    board->flasher_impl.shift_hi = &board->flasher_shift_hi;
    board->flasher_impl.usb = &board->flasher_usb;
    board->cart_mod_impl.module = &board->cart_module;
    (void)r01s_cart_slot_insert(&board->cart_slot, &board->cart_module, R01S_CART_SLOT_MOBO);
    board->cart_impl.flash = &board->cart_module.flash;
    board->cart_impl.save_eeprom = &board->cart_module.save;
    board_bind_flasher_flash(board);
}

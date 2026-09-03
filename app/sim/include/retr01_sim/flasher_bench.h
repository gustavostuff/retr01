#ifndef retr01_SIM_FLASHER_BENCH_H
#define retr01_SIM_FLASHER_BENCH_H

#include "atmega32u4.h"
#include "pc_host.h"
#include "retr01_sim/cart_module.h"
#include "retr01_sim/cart_slot.h"
#include "retr01_sim/island_builder.h"
#include "sn74hc595.h"
#include "usbc_receptacle.h"

#include <stdint.h>

/* Bench-only islands (not part of 32-IC motherboard BOM). */
enum {
    R01S_BENCH_ISLAND_PC = 0,
    R01S_BENCH_ISLAND_FLASHER,
    R01S_BENCH_ISLAND_CART,
    R01S_BENCH_ISLAND_COUNT,
};

typedef struct R01sIslandFlasherImpl {
    R01sAtmega32u4 *mcu;
    R01sSn74hc595 *shift_lo;
    R01sSn74hc595 *shift_hi;
    R01sUsbcReceptacle *usb;
} R01sIslandFlasherImpl;

typedef struct R01sIslandPcImpl {
    R01sPcHost *pc;
} R01sIslandPcImpl;

typedef struct R01sFlasherBench {
    R01sIslandBuilder builder;
    R01sCartModule cart;
    R01sCartSlotMgr cart_slot;
    R01sUsbcReceptacle usb;
    R01sAtmega32u4 mcu;
    R01sSn74hc595 shift_lo;
    R01sSn74hc595 shift_hi;
    R01sPcHost pc;
    R01sIslandCartModuleImpl cart_impl;
    R01sIslandFlasherImpl flasher_impl;
    R01sIslandPcImpl pc_impl;
    int built;
} R01sFlasherBench;

int r01s_flasher_bench_build(R01sFlasherBench *bench);
void r01s_flasher_bench_shutdown(R01sFlasherBench *bench);

R01sIslandGroup *r01s_flasher_bench_group(R01sFlasherBench *bench);

int r01s_flasher_bench_insert_cart_flasher(R01sFlasherBench *bench);
int r01s_flasher_bench_remove_cart_flasher(R01sFlasherBench *bench);
int r01s_flasher_bench_insert_cart_mobo(R01sFlasherBench *bench);
int r01s_flasher_bench_remove_cart_mobo(R01sFlasherBench *bench);

int r01s_flasher_bench_load_rom(R01sFlasherBench *bench, const char *path);
int r01s_flasher_bench_flash_rom(R01sFlasherBench *bench);
int r01s_flasher_bench_run_until_done(R01sFlasherBench *bench, int max_ticks);

uint32_t r01s_flasher_bench_bytes_programmed(const R01sFlasherBench *bench);

#endif

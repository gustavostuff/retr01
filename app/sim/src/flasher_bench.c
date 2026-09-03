#include "retr01_sim/flasher_bench.h"

#include "retr01_sim/island_group.h"

#include <string.h>

static void island_pc_init(R01sIsland *island) {
    R01sIslandPcImpl *impl = (R01sIslandPcImpl *)island->impl;
    if (!impl || !impl->pc) {
        return;
    }
    r01s_island_add_entity(island, r01s_pc_host_entity(impl->pc));
}

static void island_flasher_init(R01sIsland *island) {
    R01sIslandFlasherImpl *impl = (R01sIslandFlasherImpl *)island->impl;
    if (!impl) {
        return;
    }
    if (impl->mcu) {
        r01s_island_add_entity(island, r01s_atmega32u4_entity(impl->mcu));
    }
    if (impl->shift_lo) {
        r01s_island_add_entity(island, r01s_sn74hc595_entity(impl->shift_lo));
    }
    if (impl->shift_hi) {
        r01s_island_add_entity(island, r01s_sn74hc595_entity(impl->shift_hi));
    }
    if (impl->usb) {
        r01s_island_add_entity(island, r01s_usbc_receptacle_entity(impl->usb));
    }
}

static const R01sIslandVTable BENCH_PC_VT = {island_pc_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable BENCH_FLASHER_VT = {island_flasher_init, NULL, NULL, NULL, NULL};
static const R01sIslandVTable BENCH_CART_VT = {r01s_island_cart_module_init, NULL, NULL, NULL, NULL};

int r01s_flasher_bench_build(R01sFlasherBench *bench) {
    R01sEntity *e;

    if (!bench) {
        return -1;
    }
    memset(bench, 0, sizeof(*bench));
    r01s_cart_module_init(&bench->cart);
    r01s_cart_slot_reset(&bench->cart_slot);

    r01s_usbc_receptacle_init(&bench->usb, "JUSB");
    r01s_atmega32u4_init(&bench->mcu, "U32U4");
    r01s_sn74hc595_init(&bench->shift_lo, "U595A");
    r01s_sn74hc595_init(&bench->shift_hi, "U595B");
    r01s_pc_host_init(&bench->pc, "PC1");

    r01s_atmega32u4_bind_usb(&bench->mcu, &bench->usb);
    r01s_atmega32u4_bind_shifts(&bench->mcu, &bench->shift_lo, &bench->shift_hi);
    r01s_pc_host_bind_usb(&bench->pc, &bench->usb);

    bench->cart_impl.module = &bench->cart;
    bench->flasher_impl.mcu = &bench->mcu;
    bench->flasher_impl.shift_lo = &bench->shift_lo;
    bench->flasher_impl.shift_hi = &bench->shift_hi;
    bench->flasher_impl.usb = &bench->usb;
    bench->pc_impl.pc = &bench->pc;

    r01s_island_builder_init(&bench->builder);
    if (r01s_island_builder_add(&bench->builder, &BENCH_PC_VT, "ISLAND PC  HOST", 0, 0, 1, 1,
                                &bench->pc_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(&bench->builder, &BENCH_FLASHER_VT, "ISLAND FLASHER USB-C", 0, 0, 1, 1,
                                &bench->flasher_impl) < 0) {
        return -1;
    }
    if (r01s_island_builder_add(&bench->builder, &BENCH_CART_VT, "ISLAND CART MODULE", 0, 0, 1, 1,
                                &bench->cart_impl) < 0) {
        return -1;
    }

    e = r01s_pc_host_entity(&bench->pc);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_PC, 0, 0);
    e = r01s_atmega32u4_entity(&bench->mcu);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_FLASHER, 0, 0);
    e = r01s_sn74hc595_entity(&bench->shift_lo);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_FLASHER, e->body_w + 8, 0);
    e = r01s_sn74hc595_entity(&bench->shift_hi);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_FLASHER, 2 * (e->body_w + 8), 0);
    e = r01s_usbc_receptacle_entity(&bench->usb);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_FLASHER, 3 * (e->body_w + 8), 0);
    e = r01s_sst39sf040_entity(&bench->cart.flash);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_CART, 0, 0);
    e = r01s_i2c_eeprom_entity(&bench->cart.save);
    r01s_island_builder_mount_rel(&bench->builder, e, R01S_BENCH_ISLAND_CART, e->body_w + 8, 0);

    r01s_island_builder_arrange_rows(&bench->builder, 20, 20, 8, 8, 400);
    if (r01s_island_builder_finish(&bench->builder) < 0) {
        return -1;
    }
    bench->built = 1;
    return 0;
}

void r01s_flasher_bench_shutdown(R01sFlasherBench *bench) {
    if (!bench) {
        return;
    }
    r01s_pc_host_free_rom(&bench->pc);
    if (bench->built) {
        r01s_island_builder_shutdown(&bench->builder);
        bench->built = 0;
    }
}

R01sIslandGroup *r01s_flasher_bench_group(R01sFlasherBench *bench) {
    return bench ? r01s_island_builder_group(&bench->builder) : NULL;
}

int r01s_flasher_bench_insert_cart_flasher(R01sFlasherBench *bench) {
    if (!bench) {
        return -1;
    }
    if (r01s_cart_slot_insert(&bench->cart_slot, &bench->cart, R01S_CART_SLOT_FLASHER) != 0) {
        return -1;
    }
    r01s_atmega32u4_bind_flash(&bench->mcu, &bench->cart.flash, 1);
    return 0;
}

int r01s_flasher_bench_remove_cart_flasher(R01sFlasherBench *bench) {
    if (!bench) {
        return -1;
    }
    if (r01s_cart_slot_remove(&bench->cart_slot, R01S_CART_SLOT_FLASHER) != 0) {
        return -1;
    }
    r01s_atmega32u4_bind_flash(&bench->mcu, NULL, 0);
    return 0;
}

int r01s_flasher_bench_insert_cart_mobo(R01sFlasherBench *bench) {
    if (!bench) {
        return -1;
    }
    return r01s_cart_slot_insert(&bench->cart_slot, &bench->cart, R01S_CART_SLOT_MOBO);
}

int r01s_flasher_bench_remove_cart_mobo(R01sFlasherBench *bench) {
    if (!bench) {
        return -1;
    }
    return r01s_cart_slot_remove(&bench->cart_slot, R01S_CART_SLOT_MOBO);
}

int r01s_flasher_bench_load_rom(R01sFlasherBench *bench, const char *path) {
    if (!bench) {
        return -1;
    }
    return r01s_pc_host_load_file(&bench->pc, path);
}

int r01s_flasher_bench_flash_rom(R01sFlasherBench *bench) {
    if (!bench) {
        return -1;
    }
    if (!r01s_cart_slot_present(&bench->cart_slot, R01S_CART_SLOT_FLASHER)) {
        r01s_cart_slot_log_warn("flasher: no cart inserted");
        return -1;
    }
    r01s_atmega32u4_bind_flash(&bench->mcu, &bench->cart.flash, 1);
    r01s_atmega32u4_reset_program(&bench->mcu);
    return r01s_pc_host_start_stream(&bench->pc) ? 0 : -1;
}

int r01s_flasher_bench_run_until_done(R01sFlasherBench *bench, int max_ticks) {
    R01sIslandGroup *group;
    int i;
    int n;

    if (!bench) {
        return -1;
    }
    group = r01s_flasher_bench_group(bench);
    n = group ? r01s_island_group_count(group) : 0;
    for (i = 0; i < max_ticks; i++) {
        int j;
        for (j = 0; j < n; j++) {
            R01sIsland *isl = r01s_island_group_at_mut(group, j);
            if (isl) {
                r01s_island_tick(isl);
            }
        }
        r01s_atmega32u4_service(&bench->mcu, 8192);
        r01s_pc_host_stream_tick(&bench->pc, 64);
        if (r01s_pc_host_stream_done(&bench->pc) && !r01s_usbc_host_pending(&bench->usb) &&
            !r01s_atmega32u4_busy(&bench->mcu) &&
            r01s_atmega32u4_bytes_programmed(&bench->mcu) >= bench->pc.rom_len) {
            return 0;
        }
    }
    fprintf(stderr, "flasher: timeout done=%d bytes=%u/%zu busy=%d\n", r01s_pc_host_stream_done(&bench->pc),
            (unsigned)r01s_atmega32u4_bytes_programmed(&bench->mcu), bench->pc.rom_len,
            r01s_atmega32u4_busy(&bench->mcu));
    return -1;
}

uint32_t r01s_flasher_bench_bytes_programmed(const R01sFlasherBench *bench) {
    return bench ? r01s_atmega32u4_bytes_programmed(&bench->mcu) : 0;
}

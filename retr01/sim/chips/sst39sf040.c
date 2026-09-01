#include "sst39sf040.h"

#include "retr01_sim/bus.h"

#include <stdlib.h>
#include <string.h>

#define SST_UNLOCK1 0x5555u
#define SST_UNLOCK2 0x2AAAu

static int flash_real_timing(void) {
    const char *e = getenv("R01S_FLASH_REAL_TIMING");
    return e && e[0] != '\0' && strcmp(e, "0") != 0;
}

static uint32_t flash_addr(R01sEntity *e) {
    static const char *const names[19] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6",  "A7",  "A8",
                                          "A9",  "A10", "A11", "A12", "A13", "A14", "A15", "A16", "A17",
                                          "A18"};
    uint32_t addr = 0;
    int i;
    for (i = 0; i < 19; i++) {
        if (r01s_level_is_high(r01s_entity_sense(e, names[i]))) {
            addr |= (1u << i);
        }
    }
    return addr;
}

static void flash_drive_dq(R01sEntity *e, uint8_t v) {
    r01s_bus_write(e, "DQ", 8, v);
}

static uint8_t flash_read_dq(const R01sEntity *e) {
    return (uint8_t)r01s_bus_read(e, "DQ", 8);
}

static void flash_sector_erase(R01sSst39sf040 *c, uint32_t addr) {
    uint32_t base = addr & ~0xFFFu;
    uint32_t end = base + 4096u;
    if (end > R01S_FLASH_BYTES) {
        end = R01S_FLASH_BYTES;
    }
    memset(c->mem + base, 0xFF, (size_t)(end - base));
}

static void flash_chip_erase(R01sSst39sf040 *c) {
    memset(c->mem, 0xFF, sizeof(c->mem));
}

static void flash_apply_write(R01sSst39sf040 *c, uint32_t addr, uint8_t data) {
    uint32_t off = addr & (R01S_FLASH_BYTES - 1u);
    c->mem[off] &= data;
    c->prog_shadow = data;
    c->busy = flash_real_timing() ? 1 : 0;
}

static void flash_handle_cmd_write(R01sSst39sf040 *c, uint32_t addr, uint8_t data) {
    switch (c->cmd) {
    case R01S_FLASH_CMD_IDLE:
        if ((addr & 0x7FFFFu) == SST_UNLOCK1 && data == 0xAAu) {
            c->cmd = R01S_FLASH_CMD_UNLOCK1;
        }
        break;
    case R01S_FLASH_CMD_UNLOCK1:
        if ((addr & 0x7FFFFu) == SST_UNLOCK2 && data == 0x55u) {
            c->cmd = R01S_FLASH_CMD_UNLOCK2;
        } else {
            c->cmd = R01S_FLASH_CMD_IDLE;
        }
        break;
    case R01S_FLASH_CMD_UNLOCK2:
        if ((addr & 0x7FFFFu) == SST_UNLOCK1 && data == 0xA0u) {
            c->cmd = R01S_FLASH_CMD_PROG_ARMED;
        } else if ((addr & 0x7FFFFu) == SST_UNLOCK1 && data == 0x80u) {
            c->cmd = R01S_FLASH_CMD_ERASE_ARMED;
        } else {
            c->cmd = R01S_FLASH_CMD_IDLE;
        }
        break;
    case R01S_FLASH_CMD_PROG_ARMED:
        flash_apply_write(c, addr, data);
        c->cmd = R01S_FLASH_CMD_IDLE;
        break;
    case R01S_FLASH_CMD_ERASE_ARMED:
        if ((addr & 0x7FFFFu) == SST_UNLOCK1 && data == 0xAAu) {
            c->cmd = R01S_FLASH_CMD_ERASE_UNLOCK1;
        } else {
            c->cmd = R01S_FLASH_CMD_IDLE;
        }
        break;
    case R01S_FLASH_CMD_ERASE_UNLOCK1:
        if ((addr & 0x7FFFFu) == SST_UNLOCK2 && data == 0x55u) {
            c->cmd = R01S_FLASH_CMD_ERASE_READY;
        } else {
            c->cmd = R01S_FLASH_CMD_IDLE;
        }
        break;
    case R01S_FLASH_CMD_ERASE_READY:
        if (data == 0x30u) {
            flash_sector_erase(c, addr);
            c->cmd = R01S_FLASH_CMD_IDLE;
            c->busy = flash_real_timing() ? 1 : 0;
        } else if ((addr & 0x7FFFFu) == SST_UNLOCK1 && data == 0x10u) {
            flash_chip_erase(c);
            c->cmd = R01S_FLASH_CMD_IDLE;
            c->busy = flash_real_timing() ? 1 : 0;
        } else {
            c->cmd = R01S_FLASH_CMD_IDLE;
        }
        break;
    default:
        c->cmd = R01S_FLASH_CMD_IDLE;
        break;
    }
}

int r01s_sst39sf040_write_cycle(R01sSst39sf040 *chip, uint32_t addr, uint8_t data) {
    if (!chip) {
        return -1;
    }
    flash_handle_cmd_write(chip, addr, data);
    return 0;
}

int r01s_sst39sf040_is_busy(const R01sSst39sf040 *chip) {
    return chip && chip->busy;
}

static void flash_reset(R01sEntity *e) {
    R01sSst39sf040 *c = (R01sSst39sf040 *)e;
    c->cmd = R01S_FLASH_CMD_IDLE;
    c->busy = 0;
    c->prog_shadow = 0;
    c->we_prev = R01S_LVL_H;
    r01s_bus_hiz(e, "DQ", 8);
}

static void flash_eval(R01sEntity *e) {
    R01sSst39sf040 *c = (R01sSst39sf040 *)e;
    int ce = r01s_level_is_low(r01s_entity_sense(e, "CE#"));
    int oe = r01s_level_is_low(r01s_entity_sense(e, "OE#"));
    int we = r01s_level_is_low(r01s_entity_sense(e, "WE#"));
    R01sLevel we_now = we ? R01S_LVL_L : R01S_LVL_H;
    uint32_t addr = flash_addr(e) & (R01S_FLASH_BYTES - 1u);
    int we_rise = r01s_level_is_low(c->we_prev) && r01s_level_is_high(we_now);

    c->we_prev = we_now;

    if (!ce) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }

    if (we_rise) {
        uint8_t data = flash_read_dq(e);
        flash_handle_cmd_write(c, addr, data);
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }

    if (we) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }

    if (!oe) {
        r01s_bus_hiz(e, "DQ", 8);
        return;
    }

    {
        uint8_t v = c->mem[addr];
        if (c->busy) {
            v = (uint8_t)((v & (uint8_t)~0x80u) | ((uint8_t)(~c->prog_shadow) & 0x80u));
        }
        flash_drive_dq(e, v);
    }
}

static void flash_tick(R01sEntity *e) {
    R01sSst39sf040 *c = (R01sSst39sf040 *)e;
    if (c->busy) {
        c->busy = 0;
    }
}

static void flash_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable FLASH_VT = {flash_reset, flash_eval, flash_tick, flash_destroy};

void r01s_sst39sf040_init(R01sSst39sf040 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &FLASH_VT, "SST39SF040", refdes ? refdes : "U?");
    chip->base.impl = chip;
    memset(chip->mem, 0xFF, sizeof(chip->mem));

    r01s_entity_add_pin(&chip->base, 1, "A18", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "A16", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "A15", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "A12", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "A7", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 6, "A6", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "A5", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 8, "A4", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 9, "A3", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 10, "A2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 11, "A1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 12, "A0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 13, "DQ0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "DQ1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 15, "DQ2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "VSS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 17, "DQ3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 18, "DQ4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 19, "DQ5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 20, "DQ6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 21, "DQ7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 22, "CE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "A10", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 24, "OE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 25, "A11", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 26, "A9", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "A8", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 28, "A13", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 29, "A14", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 30, "A17", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 31, "WE#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 32, "VDD", R01S_PIN_PWR);
    r01s_entity_set_dip_mm(&chip->base, 32, 42, 14);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_sst39sf040_entity(R01sSst39sf040 *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_sst39sf040_load(R01sSst39sf040 *chip, uint32_t addr, const uint8_t *data, uint32_t len) {
    uint32_t i;
    if (!chip || !data) {
        return;
    }
    for (i = 0; i < len; i++) {
        chip->mem[(addr + i) & (R01S_FLASH_BYTES - 1u)] = data[i];
    }
}

uint8_t r01s_sst39sf040_peek(const R01sSst39sf040 *chip, uint32_t addr) {
    if (!chip) {
        return 0xFF;
    }
    return chip->mem[addr & (R01S_FLASH_BYTES - 1u)];
}

void r01s_sst39sf040_poke(R01sSst39sf040 *chip, uint32_t addr, uint8_t data) {
    if (chip) {
        chip->mem[addr & (R01S_FLASH_BYTES - 1u)] = data;
    }
}

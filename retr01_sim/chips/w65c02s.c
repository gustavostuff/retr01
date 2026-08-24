#include "w65c02s.h"

#include "retr01_sim/bus.h"

#include <string.h>

static void cpu_drive_bus(R01sEntity *e, R01sW65C02S *c) {
    if (!r01s_level_is_high(r01s_entity_sense(e, "BE"))) {
        r01s_bus_hiz(e, "A", 16);
        r01s_bus_hiz(e, "D", 8);
        r01s_entity_drive(e, "RWB", R01S_LVL_Z);
        r01s_entity_drive(e, "SYNC", R01S_LVL_Z);
        r01s_entity_drive(e, "VPB", R01S_LVL_Z);
        r01s_entity_drive(e, "MLB", R01S_LVL_Z);
        return;
    }
    r01s_bus_write(e, "A", 16, c->ab);
    r01s_entity_drive(e, "RWB", c->rwb ? R01S_LVL_H : R01S_LVL_L);
    r01s_entity_drive(e, "SYNC", c->sync ? R01S_LVL_H : R01S_LVL_L);
    if (c->phase == R01S_CPU_VEC_PCL || c->phase == R01S_CPU_VEC_PCH) {
        r01s_entity_drive(e, "VPB", R01S_LVL_L);
    } else {
        r01s_entity_drive(e, "VPB", R01S_LVL_H);
    }
    r01s_entity_drive(e, "MLB", R01S_LVL_H);
    if (c->rwb) {
        r01s_bus_hiz(e, "D", 8);
    } else {
        r01s_bus_write(e, "D", 8, c->a);
    }
}

static void cpu_reset(R01sEntity *e) {
    R01sW65C02S *c = (R01sW65C02S *)e;
    c->a = c->x = c->y = 0;
    c->s = 0xFD;
    c->p = 0x34;
    c->pc = 0;
    c->ab = 0xFFFC;
    c->ea = 0;
    c->ir = 0;
    c->res_cycles = 7;
    c->phase = R01S_CPU_RES_HOLD;
    c->sync = 0;
    c->rwb = 1;
    cpu_drive_bus(e, c);
}

static void cpu_eval(R01sEntity *e) {
    R01sW65C02S *c = (R01sW65C02S *)e;
    if (r01s_level_is_low(r01s_entity_sense(e, "RESB"))) {
        c->phase = R01S_CPU_RES_HOLD;
        c->res_cycles = 7;
        c->sync = 0;
        c->rwb = 1;
        c->ab = 0xFFFC;
    }
    cpu_drive_bus(e, c);
}

static uint8_t cpu_sample_d(R01sEntity *e) {
    return (uint8_t)r01s_bus_read(e, "D", 8);
}

static void cpu_begin_fetch(R01sW65C02S *c) {
    c->phase = R01S_CPU_FETCH;
    c->ab = c->pc;
    c->rwb = 1;
    c->sync = 1;
}

static void cpu_set_zn(R01sW65C02S *c, uint8_t v) {
    if (v == 0) {
        c->p |= 0x02u; /* Z */
    } else {
        c->p = (uint8_t)(c->p & ~0x02u);
    }
    if (v & 0x80u) {
        c->p |= 0x80u; /* N */
    } else {
        c->p = (uint8_t)(c->p & ~0x80u);
    }
}

static void cpu_tick(R01sEntity *e) {
    R01sW65C02S *c = (R01sW65C02S *)e;

    if (!r01s_level_is_high(r01s_entity_sense(e, "BE"))) {
        cpu_drive_bus(e, c);
        return;
    }
    if (r01s_level_is_low(r01s_entity_sense(e, "RDY"))) {
        cpu_drive_bus(e, c);
        return;
    }
    if (r01s_level_is_low(r01s_entity_sense(e, "RESB"))) {
        c->phase = R01S_CPU_RES_HOLD;
        c->res_cycles = 7;
        c->sync = 0;
        c->rwb = 1;
        c->ab = 0xFFFC;
        cpu_drive_bus(e, c);
        return;
    }

    switch (c->phase) {
    case R01S_CPU_RES_HOLD:
        c->phase = R01S_CPU_RES_WAIT;
        c->res_cycles = 7;
        c->ab = 0xFFFC;
        c->rwb = 1;
        c->sync = 0;
        break;
    case R01S_CPU_RES_WAIT:
        c->res_cycles--;
        if (c->res_cycles <= 0) {
            c->phase = R01S_CPU_VEC_PCL;
            c->ab = 0xFFFC;
            c->rwb = 1;
            c->sync = 0;
        }
        break;
    case R01S_CPU_VEC_PCL:
        c->pc = cpu_sample_d(e);
        c->phase = R01S_CPU_VEC_PCH;
        c->ab = 0xFFFD;
        c->rwb = 1;
        c->sync = 0;
        break;
    case R01S_CPU_VEC_PCH:
        c->pc |= (uint16_t)cpu_sample_d(e) << 8;
        cpu_begin_fetch(c);
        break;
    case R01S_CPU_FETCH:
        c->ir = cpu_sample_d(e);
        c->pc++;
        c->sync = 0;
        switch (c->ir) {
        case 0xEA: /* NOP — 1-cycle stub (real chip is 2) */
            cpu_begin_fetch(c);
            break;
        case 0xCA: /* DEX — 1-cycle stub (real chip is 2) */
            c->x--;
            cpu_set_zn(c, c->x);
            cpu_begin_fetch(c);
            break;
        case 0xA9: /* LDA #imm */
        case 0xA2: /* LDX #imm */
        case 0xD0: /* BNE rel */
            c->phase = R01S_CPU_OP_IMM;
            c->ab = c->pc;
            c->rwb = 1;
            break;
        case 0x4C: /* JMP abs */
        case 0x8D: /* STA abs */
        case 0xAD: /* LDA abs */
            c->phase = R01S_CPU_OP_ADL;
            c->ab = c->pc;
            c->rwb = 1;
            break;
        default: /* unknown: skip like NOP */
            cpu_begin_fetch(c);
            break;
        }
        break;
    case R01S_CPU_OP_IMM:
        if (c->ir == 0xA2) {
            c->x = cpu_sample_d(e);
            cpu_set_zn(c, c->x);
            c->pc++;
        } else if (c->ir == 0xD0) {
            int8_t off = (int8_t)cpu_sample_d(e);
            c->pc++;
            if ((c->p & 0x02u) == 0) {
                c->pc = (uint16_t)(c->pc + (int16_t)off);
            }
        } else {
            c->a = cpu_sample_d(e);
            cpu_set_zn(c, c->a);
            c->pc++;
        }
        cpu_begin_fetch(c);
        break;
    case R01S_CPU_OP_ADL:
        c->ea = cpu_sample_d(e);
        c->pc++;
        c->phase = R01S_CPU_OP_ADH;
        c->ab = c->pc;
        c->rwb = 1;
        break;
    case R01S_CPU_OP_ADH:
        c->ea |= (uint16_t)cpu_sample_d(e) << 8;
        c->pc++;
        if (c->ir == 0x4C) {
            c->pc = c->ea;
            cpu_begin_fetch(c);
        } else {
            c->phase = R01S_CPU_OP_DATA;
            c->ab = c->ea;
            c->rwb = (c->ir != 0x8D);
            c->sync = 0;
        }
        break;
    case R01S_CPU_OP_DATA:
        if (c->rwb) {
            c->a = cpu_sample_d(e);
            cpu_set_zn(c, c->a);
        }
        cpu_begin_fetch(c);
        break;
    }

    cpu_drive_bus(e, c);
}

static void cpu_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable CPU_VT = {cpu_reset, cpu_eval, cpu_tick, cpu_destroy};

static const char *const CPU_A_NAMES[16] = {"A0",  "A1",  "A2",  "A3",  "A4",  "A5",  "A6",  "A7",
                                           "A8",  "A9",  "A10", "A11", "A12", "A13", "A14", "A15"};

void r01s_w65c02s_init(R01sW65C02S *chip, const char *refdes) {
    int i;
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &CPU_VT, "W65C02S", refdes ? refdes : "U1");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "VPB", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 2, "RDY", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 3, "PHI1O", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "IRQB", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 5, "MLB", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 6, "NMIB", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 7, "SYNC", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 8, "VDD", R01S_PIN_PWR);
    for (i = 0; i <= 11; i++) {
        r01s_entity_add_pin(&chip->base, 9 + i, CPU_A_NAMES[i], R01S_PIN_OUT);
    }
    r01s_entity_add_pin(&chip->base, 21, "VSS", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 22, CPU_A_NAMES[12], R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 23, CPU_A_NAMES[13], R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 24, CPU_A_NAMES[14], R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 25, CPU_A_NAMES[15], R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 26, "D7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 27, "D6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 28, "D5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 29, "D4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 30, "D3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 31, "D2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 32, "D1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 33, "D0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 34, "RWB", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 35, "NC", R01S_PIN_NC);
    r01s_entity_add_pin(&chip->base, 36, "BE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 37, "PHI2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 38, "SOB", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 39, "PHI2O", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 40, "RESB", R01S_PIN_IN);
    r01s_entity_set_dip(&chip->base, 40, 64);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_w65c02s_entity(R01sW65C02S *chip) {
    return chip ? &chip->base : NULL;
}

uint16_t r01s_w65c02s_pc(const R01sW65C02S *chip) {
    return chip ? chip->pc : 0;
}

uint8_t r01s_w65c02s_a(const R01sW65C02S *chip) {
    return chip ? chip->a : 0;
}

R01sCpuPhase r01s_w65c02s_phase(const R01sW65C02S *chip) {
    return chip ? chip->phase : R01S_CPU_RES_HOLD;
}

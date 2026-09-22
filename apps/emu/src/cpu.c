#include "retr01_emu/cpu.h"

#include "retr01_emu/machine.h"
#include "r01_hw_regs.h"

#include <string.h>

#define C_C 0x01
#define C_Z 0x02
#define C_I 0x04
#define C_D 0x08
#define C_B 0x10
#define C_U 0x20
#define C_V 0x40
#define C_N 0x80

static void set_zn(R01eCpu *cpu, uint8_t v) {
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_Z | C_N)) | (v == 0 ? C_Z : 0) | (v & 0x80u ? C_N : 0));
}

static void do_adc(R01eCpu *cpu, uint8_t imm) {
    uint16_t t = (uint16_t)cpu->a + imm + (cpu->p & C_C ? 1 : 0);
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t > 0xFF ? C_C : 0) |
                       (((~(cpu->a ^ imm) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
    cpu->a = (uint8_t)t;
    set_zn(cpu, cpu->a);
}

static void do_sbc(R01eCpu *cpu, uint8_t imm) {
    uint16_t t = (uint16_t)cpu->a - imm - (cpu->p & C_C ? 0 : 1);
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t < 0x100 ? C_C : 0) |
                       ((((cpu->a ^ imm) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
    cpu->a = (uint8_t)t;
    set_zn(cpu, cpu->a);
}

static void do_cmp(R01eCpu *cpu, uint8_t r, uint8_t imm) {
    uint16_t t = (uint16_t)r - imm;
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N)) | (r >= imm ? C_C : 0) |
                       ((t & 0xFF) == 0 ? C_Z : 0) | (t & 0x80 ? C_N : 0));
}

static uint8_t do_asl(R01eCpu *cpu, uint8_t v) {
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (v & 0x80 ? C_C : 0));
    v = (uint8_t)(v << 1);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_lsr(R01eCpu *cpu, uint8_t v) {
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (v & 0x01 ? C_C : 0));
    v = (uint8_t)(v >> 1);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_rol(R01eCpu *cpu, uint8_t v) {
    uint8_t cin = (uint8_t)(cpu->p & C_C);
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (v & 0x80 ? C_C : 0));
    v = (uint8_t)((v << 1) | (cin ? 1u : 0u));
    set_zn(cpu, v);
    return v;
}

static uint8_t do_ror(R01eCpu *cpu, uint8_t v) {
    uint8_t cin = (uint8_t)(cpu->p & C_C);
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (v & 0x01 ? C_C : 0));
    v = (uint8_t)((v >> 1) | (cin ? 0x80u : 0u));
    set_zn(cpu, v);
    return v;
}

static void do_bit(R01eCpu *cpu, uint8_t v) {
    cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_Z | C_N | C_V)) | ((cpu->a & v) == 0 ? C_Z : 0) | (v & 0xC0u));
}

static uint8_t rd(R01eMachine *m, uint16_t a) {
    return r01e_mem_read(m, a);
}

static void wr(R01eMachine *m, uint16_t a, uint8_t v) {
    r01e_mem_write(m, a, v);
}

static uint16_t rd16(R01eMachine *m, uint16_t a) {
    return (uint16_t)rd(m, a) | ((uint16_t)rd(m, (uint16_t)(a + 1)) << 8);
}

static void push(R01eCpu *cpu, R01eMachine *m, uint8_t v) {
    wr(m, (uint16_t)(0x0100u | cpu->s), v);
    cpu->s--;
}

static uint8_t pull(R01eCpu *cpu, R01eMachine *m) {
    cpu->s++;
    return rd(m, (uint16_t)(0x0100u | cpu->s));
}

void r01e_cpu_reset(R01eCpu *cpu, R01eMachine *m) {
    if (!cpu) {
        return;
    }
    memset(cpu, 0, sizeof(*cpu));
    cpu->a = cpu->x = cpu->y = 0;
    cpu->s = 0xFD;
    cpu->p = (uint8_t)(C_U | C_I);
    cpu->pc = rd16(m, 0xFFFC);
    cpu->cycles = 0;
    cpu->stalled = 0;
}

void r01e_cpu_rdy_hold(R01eCpu *cpu, uint32_t holds) {
    if (!cpu) {
        return;
    }
    cpu->stalled = holds;
}

void r01e_cpu_rdy_hold_ee(R01eCpu *cpu) {
    r01e_cpu_rdy_hold(cpu, R01_RDY_EE_HOLDS);
}

int r01e_cpu_rdy_is_held(const R01eCpu *cpu) {
    return cpu && cpu->stalled > 0;
}

uint32_t r01e_cpu_rdy_remaining(const R01eCpu *cpu) {
    return cpu ? cpu->stalled : 0;
}

/*
 * Compact 65C02-ish core. Enough for Studio stub + typical game code.
 * Cycle counts are approximate (useful for frame pacing, not silicon QA).
 */
int r01e_cpu_step(R01eCpu *cpu, R01eMachine *m) {
    uint8_t op;
    uint16_t addr;
    uint8_t v;
    int cyc = 2;

    if (!cpu || !m) {
        return 0;
    }

    /* RDY low: stretch PHI2 (no fetch / no NMI entry until released). */
    if (cpu->stalled > 0) {
        cpu->stalled--;
        cpu->cycles++;
        return 1;
    }

    /* NMI edge */
    if (m->nmi_pending) {
        m->nmi_pending = 0;
        push(cpu, m, (uint8_t)(cpu->pc >> 8));
        push(cpu, m, (uint8_t)(cpu->pc & 0xFF));
        push(cpu, m, (uint8_t)(cpu->p & (uint8_t)~C_B));
        cpu->p |= C_I;
        cpu->pc = rd16(m, 0xFFFA);
        cpu->cycles += 7;
        return 7;
    }

    op = rd(m, cpu->pc++);
    switch (op) {
    case 0x14: { /* TRB zp -- 65C02 */
        addr = rd(m, cpu->pc++);
        v = (uint8_t)(rd(m, addr) & (uint8_t)~cpu->a);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 5;
        break;
    }
    case 0x80: { /* BRA rel -- 65C02 */
        int8_t off = (int8_t)rd(m, cpu->pc++);
        cpu->pc = (uint16_t)(cpu->pc + off);
        cyc = 3;
        break;
    }
    case 0x5A: /* PHY -- 65C02 */
        push(cpu, m, cpu->y);
        cyc = 3;
        break;
    case 0x7A: /* PLY -- 65C02 */
        cpu->y = pull(cpu, m);
        set_zn(cpu, cpu->y);
        cyc = 4;
        break;
    case 0xDA: /* PHX -- 65C02 */
        push(cpu, m, cpu->x);
        cyc = 3;
        break;
    case 0xFA: /* PLX -- 65C02 */
        cpu->x = pull(cpu, m);
        set_zn(cpu, cpu->x);
        cyc = 4;
        break;
    case 0x1A: /* INC A -- 65C02 */
        cpu->a++;
        set_zn(cpu, cpu->a);
        cyc = 2;
        break;
    case 0x3A: /* DEC A -- 65C02 */
        cpu->a--;
        set_zn(cpu, cpu->a);
        cyc = 2;
        break;
    case 0x64: /* STZ zp -- 65C02 */
        addr = rd(m, cpu->pc++);
        wr(m, addr, 0);
        cyc = 3;
        break;
    case 0x74: /* STZ zp,X -- 65C02 */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, 0);
        cyc = 4;
        break;
    case 0x9C: /* STZ abs -- 65C02 */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, 0);
        cyc = 4;
        break;
    case 0x9E: /* STZ abs,X -- 65C02 */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, 0);
        cyc = 5;
        break;
    case 0x92: /* STA (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        wr(m, rd16(m, addr), cpu->a);
        cyc = 5;
        break;
    case 0xB2: /* LDA (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        cpu->a = rd(m, rd16(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x12: /* ORA (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        cpu->a |= rd(m, rd16(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x32: /* AND (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        cpu->a &= rd(m, rd16(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x52: /* EOR (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        cpu->a ^= rd(m, rd16(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0xD2: /* CMP (zp) -- 65C02 */
        addr = rd(m, cpu->pc++);
        v = rd(m, rd16(m, addr));
        {
            uint16_t d = (uint16_t)cpu->a - v;
            if (cpu->a >= v) {
                cpu->p |= C_C;
            } else {
                cpu->p = (uint8_t)(cpu->p & (uint8_t)~C_C);
            }
            set_zn(cpu, (uint8_t)d);
        }
        cyc = 5;
        break;
    case 0x72: /* ADC (zp) -- 65C02 */
    case 0xF2: /* SBC (zp) -- 65C02 */ {
        uint16_t t;
        addr = rd(m, cpu->pc++);
        v = rd(m, rd16(m, addr));
        if (op == 0x72) {
            t = (uint16_t)cpu->a + v + (cpu->p & C_C ? 1 : 0);
            cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t > 0xFF ? C_C : 0) |
                               (((~(cpu->a ^ v) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
            cpu->a = (uint8_t)t;
        } else {
            t = (uint16_t)cpu->a - v - (cpu->p & C_C ? 0 : 1);
            cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t < 0x100 ? C_C : 0) |
                               ((((cpu->a ^ v) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
            cpu->a = (uint8_t)t;
        }
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    }
    case 0x04: /* TSB zp -- 65C02 */
        addr = rd(m, cpu->pc++);
        v = rd(m, addr);
        set_zn(cpu, (uint8_t)(v & cpu->a));
        wr(m, addr, (uint8_t)(v | cpu->a));
        cyc = 5;
        break;
    case 0x0C: /* TSB abs -- 65C02 */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = rd(m, addr);
        set_zn(cpu, (uint8_t)(v & cpu->a));
        wr(m, addr, (uint8_t)(v | cpu->a));
        cyc = 6;
        break;
    case 0x1C: /* TRB abs -- 65C02 */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = rd(m, addr);
        set_zn(cpu, (uint8_t)(v & cpu->a));
        wr(m, addr, (uint8_t)(v & (uint8_t)~cpu->a));
        cyc = 6;
        break;
    case 0x34: /* BIT zp,X -- 65C02 */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        v = rd(m, addr);
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_Z | C_N | C_V)) | ((cpu->a & v) == 0 ? C_Z : 0) |
                           (v & 0xC0u));
        cyc = 4;
        break;
    case 0x3C: /* BIT abs,X -- 65C02 */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = rd(m, addr);
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_Z | C_N | C_V)) | ((cpu->a & v) == 0 ? C_Z : 0) |
                           (v & 0xC0u));
        cyc = 4;
        break;
    case 0xEA: /* NOP */
        cyc = 2;
        break;
    case 0x78: /* SEI */
        cpu->p |= C_I;
        break;
    case 0x58: /* CLI */
        cpu->p = (uint8_t)(cpu->p & (uint8_t)~C_I);
        break;
    case 0xF8: /* SED */
        cpu->p |= C_D;
        break;
    case 0xD8: /* CLD */
        cpu->p = (uint8_t)(cpu->p & (uint8_t)~C_D);
        break;
    case 0x18: /* CLC */
        cpu->p = (uint8_t)(cpu->p & (uint8_t)~C_C);
        break;
    case 0x38: /* SEC */
        cpu->p |= C_C;
        break;
    case 0xB8: /* CLV */
        cpu->p = (uint8_t)(cpu->p & (uint8_t)~C_V);
        break;
    case 0xA9: /* LDA #imm */
        cpu->a = rd(m, cpu->pc++);
        set_zn(cpu, cpu->a);
        cyc = 2;
        break;
    case 0xA2: /* LDX #imm */
        cpu->x = rd(m, cpu->pc++);
        set_zn(cpu, cpu->x);
        break;
    case 0xA0: /* LDY #imm */
        cpu->y = rd(m, cpu->pc++);
        set_zn(cpu, cpu->y);
        break;
    case 0x9A: /* TXS */
        cpu->s = cpu->x;
        break;
    case 0xBA: /* TSX */
        cpu->x = cpu->s;
        set_zn(cpu, cpu->x);
        break;
    case 0xAA: /* TAX */
        cpu->x = cpu->a;
        set_zn(cpu, cpu->x);
        break;
    case 0x8A: /* TXA */
        cpu->a = cpu->x;
        set_zn(cpu, cpu->a);
        break;
    case 0xA8: /* TAY */
        cpu->y = cpu->a;
        set_zn(cpu, cpu->y);
        break;
    case 0x98: /* TYA */
        cpu->a = cpu->y;
        set_zn(cpu, cpu->a);
        break;
    case 0x4C: /* JMP abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = addr;
        cyc = 3;
        break;
    case 0x6C: { /* JMP (abs) -- 65C02 page-safe */
        uint16_t ind = rd16(m, cpu->pc);
        cpu->pc = rd16(m, ind);
        cyc = 6;
        break;
    }
    case 0x20: /* JSR abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        push(cpu, m, (uint8_t)((cpu->pc - 1) >> 8));
        push(cpu, m, (uint8_t)((cpu->pc - 1) & 0xFF));
        cpu->pc = addr;
        cyc = 6;
        break;
    case 0x60: /* RTS */
        v = pull(cpu, m);
        addr = (uint16_t)v | ((uint16_t)pull(cpu, m) << 8);
        cpu->pc = (uint16_t)(addr + 1);
        cyc = 6;
        break;
    case 0x40: /* RTI */
        cpu->p = (uint8_t)(pull(cpu, m) | C_U);
        v = pull(cpu, m);
        cpu->pc = (uint16_t)v | ((uint16_t)pull(cpu, m) << 8);
        cyc = 6;
        break;
    case 0x8D: /* STA abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, cpu->a);
        cyc = 4;
        break;
    case 0xAD: /* LDA abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0xAE: /* LDX abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->x = rd(m, addr);
        set_zn(cpu, cpu->x);
        cyc = 4;
        break;
    case 0xAC: /* LDY abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->y = rd(m, addr);
        set_zn(cpu, cpu->y);
        cyc = 4;
        break;
    case 0x8E: /* STX abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, cpu->x);
        cyc = 4;
        break;
    case 0x8C: /* STY abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, cpu->y);
        cyc = 4;
        break;
    case 0x85: /* STA zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, cpu->a);
        cyc = 3;
        break;
    case 0xA5: /* LDA zp */
        addr = rd(m, cpu->pc++);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 3;
        break;
    case 0x86: /* STX zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, cpu->x);
        cyc = 3;
        break;
    case 0x84: /* STY zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, cpu->y);
        cyc = 3;
        break;
    case 0xA6: /* LDX zp */
        addr = rd(m, cpu->pc++);
        cpu->x = rd(m, addr);
        set_zn(cpu, cpu->x);
        cyc = 3;
        break;
    case 0xA4: /* LDY zp */
        addr = rd(m, cpu->pc++);
        cpu->y = rd(m, addr);
        set_zn(cpu, cpu->y);
        cyc = 3;
        break;
    case 0xE8: /* INX */
        cpu->x++;
        set_zn(cpu, cpu->x);
        break;
    case 0xC8: /* INY */
        cpu->y++;
        set_zn(cpu, cpu->y);
        break;
    case 0xCA: /* DEX */
        cpu->x--;
        set_zn(cpu, cpu->x);
        break;
    case 0x88: /* DEY */
        cpu->y--;
        set_zn(cpu, cpu->y);
        break;
    case 0xE6: /* INC zp */
        addr = rd(m, cpu->pc++);
        v = (uint8_t)(rd(m, addr) + 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 5;
        break;
    case 0xC6: /* DEC zp */
        addr = rd(m, cpu->pc++);
        v = (uint8_t)(rd(m, addr) - 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 5;
        break;
    case 0xEE: /* INC abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = (uint8_t)(rd(m, addr) + 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 6;
        break;
    case 0xCE: /* DEC abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = (uint8_t)(rd(m, addr) - 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 6;
        break;
    case 0x69: /* ADC #imm */
    case 0xE9: /* SBC #imm */
    case 0x29: /* AND #imm */
    case 0x09: /* ORA #imm */
    case 0x49: /* EOR #imm */
    case 0xC9: /* CMP #imm */
    case 0xE0: /* CPX #imm */
    case 0xC0: /* CPY #imm */ {
        uint8_t imm = rd(m, cpu->pc++);
        if (op == 0x29) {
            cpu->a &= imm;
            set_zn(cpu, cpu->a);
        } else if (op == 0x09) {
            cpu->a |= imm;
            set_zn(cpu, cpu->a);
        } else if (op == 0x49) {
            cpu->a ^= imm;
            set_zn(cpu, cpu->a);
        } else if (op == 0xC9 || op == 0xE0 || op == 0xC0) {
            uint8_t r = (op == 0xC9) ? cpu->a : (op == 0xE0 ? cpu->x : cpu->y);
            uint16_t t = (uint16_t)r - imm;
            cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N)) | (r >= imm ? C_C : 0) |
                               ((t & 0xFF) == 0 ? C_Z : 0) | (t & 0x80 ? C_N : 0));
        } else {
            /* ADC/SBC binary */
            uint16_t t;
            if (op == 0x69) {
                t = (uint16_t)cpu->a + imm + (cpu->p & C_C ? 1 : 0);
                cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t > 0xFF ? C_C : 0) |
                                   (((~(cpu->a ^ imm) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
                cpu->a = (uint8_t)t;
            } else {
                t = (uint16_t)cpu->a - imm - (cpu->p & C_C ? 0 : 1);
                cpu->p = (uint8_t)((cpu->p & (uint8_t)~(C_C | C_Z | C_N | C_V)) | (t < 0x100 ? C_C : 0) |
                                   ((((cpu->a ^ imm) & (cpu->a ^ t)) & 0x80) ? C_V : 0));
                cpu->a = (uint8_t)t;
            }
            set_zn(cpu, cpu->a);
        }
        break;
    }
    case 0x0A: /* ASL A */
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (cpu->a & 0x80 ? C_C : 0));
        cpu->a = (uint8_t)(cpu->a << 1);
        set_zn(cpu, cpu->a);
        break;
    case 0x4A: /* LSR A */
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (cpu->a & 0x01 ? C_C : 0));
        cpu->a = (uint8_t)(cpu->a >> 1);
        set_zn(cpu, cpu->a);
        break;
    case 0x2A: /* ROL A */ {
        uint8_t c = (uint8_t)(cpu->p & C_C);
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (cpu->a & 0x80 ? C_C : 0));
        cpu->a = (uint8_t)((cpu->a << 1) | (c ? 1 : 0));
        set_zn(cpu, cpu->a);
        break;
    }
    case 0x6A: /* ROR A */ {
        uint8_t c = (uint8_t)(cpu->p & C_C);
        cpu->p = (uint8_t)((cpu->p & (uint8_t)~C_C) | (cpu->a & 0x01 ? C_C : 0));
        cpu->a = (uint8_t)((cpu->a >> 1) | (c ? 0x80 : 0));
        set_zn(cpu, cpu->a);
        break;
    }
    case 0x48: /* PHA */
        push(cpu, m, cpu->a);
        cyc = 3;
        break;
    case 0x68: /* PLA */
        cpu->a = pull(cpu, m);
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x08: /* PHP */
        push(cpu, m, (uint8_t)(cpu->p | C_B | C_U));
        cyc = 3;
        break;
    case 0x28: /* PLP */
        cpu->p = (uint8_t)(pull(cpu, m) | C_U);
        cyc = 4;
        break;
    case 0x90: /* BCC */
    case 0xB0: /* BCS */
    case 0xF0: /* BEQ */
    case 0xD0: /* BNE */
    case 0x30: /* BMI */
    case 0x10: /* BPL */
    case 0x50: /* BVC */
    case 0x70: /* BVS */ {
        int8_t off = (int8_t)rd(m, cpu->pc++);
        int take = 0;
        switch (op) {
        case 0x90:
            take = !(cpu->p & C_C);
            break;
        case 0xB0:
            take = (cpu->p & C_C) != 0;
            break;
        case 0xF0:
            take = (cpu->p & C_Z) != 0;
            break;
        case 0xD0:
            take = !(cpu->p & C_Z);
            break;
        case 0x30:
            take = (cpu->p & C_N) != 0;
            break;
        case 0x10:
            take = !(cpu->p & C_N);
            break;
        case 0x50:
            take = !(cpu->p & C_V);
            break;
        case 0x70:
            take = (cpu->p & C_V) != 0;
            break;
        }
        cyc = 2;
        if (take) {
            uint16_t npc = (uint16_t)(cpu->pc + off);
            cyc += ((npc ^ cpu->pc) & 0xFF00) ? 2 : 1;
            cpu->pc = npc;
        }
        break;
    }
    case 0x9D: /* STA abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, cpu->a);
        cyc = 5;
        break;
    case 0x99: /* STA abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, cpu->a);
        cyc = 5;
        break;
    case 0xBD: /* LDA abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0xB9: /* LDA abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x95: /* STA zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, cpu->a);
        cyc = 4;
        break;
    case 0xB5: /* LDA zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x65: /* ADC zp */
        do_adc(cpu, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0x75: /* ADC zp,X */
        do_adc(cpu, rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        cyc = 4;
        break;
    case 0x6D: /* ADC abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_adc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0x7D: /* ADC abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_adc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0x79: /* ADC abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_adc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0x71: /* ADC (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        do_adc(cpu, rd(m, addr));
        cyc = 5;
        break;
    case 0x61: /* ADC (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        do_adc(cpu, rd(m, addr));
        cyc = 6;
        break;
    case 0xE5: /* SBC zp */
        do_sbc(cpu, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0xF5: /* SBC zp,X */
        do_sbc(cpu, rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        cyc = 4;
        break;
    case 0xED: /* SBC abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_sbc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0xFD: /* SBC abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_sbc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0xF9: /* SBC abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_sbc(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0xF1: /* SBC (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        do_sbc(cpu, rd(m, addr));
        cyc = 5;
        break;
    case 0xE1: /* SBC (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        do_sbc(cpu, rd(m, addr));
        cyc = 6;
        break;
    case 0xC5: /* CMP zp */
        do_cmp(cpu, cpu->a, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0xE4: /* CPX zp */
        do_cmp(cpu, cpu->x, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0xC4: /* CPY zp */
        do_cmp(cpu, cpu->y, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0x91: /* STA (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        wr(m, addr, cpu->a);
        cyc = 6;
        break;
    case 0x81: /* STA (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        wr(m, addr, cpu->a);
        cyc = 6;
        break;
    case 0xB1: /* LDA (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0xA1: /* LDA (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        cpu->a = rd(m, addr);
        set_zn(cpu, cpu->a);
        cyc = 6;
        break;
    case 0x06: /* ASL zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, do_asl(cpu, rd(m, addr)));
        cyc = 5;
        break;
    case 0x16: /* ASL zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, do_asl(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x0E: /* ASL abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_asl(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x1E: /* ASL abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_asl(cpu, rd(m, addr)));
        cyc = 7;
        break;
    case 0x24: /* BIT zp */
        do_bit(cpu, rd(m, rd(m, cpu->pc++)));
        cyc = 3;
        break;
    case 0x2C: /* BIT abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_bit(cpu, rd(m, addr));
        cyc = 4;
        break;
    case 0x05: /* ORA zp */
        cpu->a = (uint8_t)(cpu->a | rd(m, rd(m, cpu->pc++)));
        set_zn(cpu, cpu->a);
        cyc = 3;
        break;
    case 0x15: /* ORA zp,X */
        cpu->a = (uint8_t)(cpu->a | rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x0D: /* ORA abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a | rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x1D: /* ORA abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a | rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x19: /* ORA abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a | rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x11: /* ORA (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        cpu->a = (uint8_t)(cpu->a | rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x01: /* ORA (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        cpu->a = (uint8_t)(cpu->a | rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 6;
        break;
    case 0x25: /* AND zp */
        cpu->a = (uint8_t)(cpu->a & rd(m, rd(m, cpu->pc++)));
        set_zn(cpu, cpu->a);
        cyc = 3;
        break;
    case 0x35: /* AND zp,X */
        cpu->a = (uint8_t)(cpu->a & rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x2D: /* AND abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a & rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x3D: /* AND abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a & rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x39: /* AND abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a & rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x31: /* AND (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        cpu->a = (uint8_t)(cpu->a & rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x21: /* AND (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        cpu->a = (uint8_t)(cpu->a & rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 6;
        break;
    case 0x45: /* EOR zp */
        cpu->a = (uint8_t)(cpu->a ^ rd(m, rd(m, cpu->pc++)));
        set_zn(cpu, cpu->a);
        cyc = 3;
        break;
    case 0x55: /* EOR zp,X */
        cpu->a = (uint8_t)(cpu->a ^ rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x4D: /* EOR abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a ^ rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x5D: /* EOR abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a ^ rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x59: /* EOR abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->a = (uint8_t)(cpu->a ^ rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 4;
        break;
    case 0x51: /* EOR (zp),Y */
        addr = (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y);
        cpu->a = (uint8_t)(cpu->a ^ rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 5;
        break;
    case 0x41: /* EOR (zp,X) */
        addr = rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        cpu->a = (uint8_t)(cpu->a ^ rd(m, addr));
        set_zn(cpu, cpu->a);
        cyc = 6;
        break;
    case 0x46: /* LSR zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, do_lsr(cpu, rd(m, addr)));
        cyc = 5;
        break;
    case 0x56: /* LSR zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, do_lsr(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x4E: /* LSR abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_lsr(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x5E: /* LSR abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_lsr(cpu, rd(m, addr)));
        cyc = 7;
        break;
    case 0x26: /* ROL zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, do_rol(cpu, rd(m, addr)));
        cyc = 5;
        break;
    case 0x36: /* ROL zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, do_rol(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x2E: /* ROL abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_rol(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x3E: /* ROL abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_rol(cpu, rd(m, addr)));
        cyc = 7;
        break;
    case 0x66: /* ROR zp */
        addr = rd(m, cpu->pc++);
        wr(m, addr, do_ror(cpu, rd(m, addr)));
        cyc = 5;
        break;
    case 0x76: /* ROR zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        wr(m, addr, do_ror(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x6E: /* ROR abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_ror(cpu, rd(m, addr)));
        cyc = 6;
        break;
    case 0x7E: /* ROR abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        wr(m, addr, do_ror(cpu, rd(m, addr)));
        cyc = 7;
        break;
    case 0xC1: /* CMP (zp,X) */
        do_cmp(cpu, cpu->a, rd(m, rd16(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x))));
        cyc = 6;
        break;
    case 0xD1: /* CMP (zp),Y */
        do_cmp(cpu, cpu->a, rd(m, (uint16_t)(rd16(m, rd(m, cpu->pc++)) + cpu->y)));
        cyc = 5;
        break;
    case 0xD5: /* CMP zp,X */
        do_cmp(cpu, cpu->a, rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x)));
        cyc = 4;
        break;
    case 0xCD: /* CMP abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_cmp(cpu, cpu->a, rd(m, addr));
        cyc = 4;
        break;
    case 0xDD: /* CMP abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_cmp(cpu, cpu->a, rd(m, addr));
        cyc = 4;
        break;
    case 0xD9: /* CMP abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_cmp(cpu, cpu->a, rd(m, addr));
        cyc = 4;
        break;
    case 0xCC: /* CPY abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_cmp(cpu, cpu->y, rd(m, addr));
        cyc = 4;
        break;
    case 0xEC: /* CPX abs */
        addr = rd16(m, cpu->pc);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        do_cmp(cpu, cpu->x, rd(m, addr));
        cyc = 4;
        break;
    case 0xD6: /* DEC zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        v = (uint8_t)(rd(m, addr) - 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 6;
        break;
    case 0xDE: /* DEC abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = (uint8_t)(rd(m, addr) - 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 7;
        break;
    case 0xF6: /* INC zp,X */
        addr = (uint8_t)(rd(m, cpu->pc++) + cpu->x);
        v = (uint8_t)(rd(m, addr) + 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 6;
        break;
    case 0xFE: /* INC abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        v = (uint8_t)(rd(m, addr) + 1);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 7;
        break;
    case 0x94: /* STY zp,X */
        wr(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x), cpu->y);
        cyc = 4;
        break;
    case 0x96: /* STX zp,Y */
        wr(m, (uint8_t)(rd(m, cpu->pc++) + cpu->y), cpu->x);
        cyc = 4;
        break;
    case 0xB4: /* LDY zp,X */
        cpu->y = rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->x));
        set_zn(cpu, cpu->y);
        cyc = 4;
        break;
    case 0xB6: /* LDX zp,Y */
        cpu->x = rd(m, (uint8_t)(rd(m, cpu->pc++) + cpu->y));
        set_zn(cpu, cpu->x);
        cyc = 4;
        break;
    case 0xBC: /* LDY abs,X */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->y = rd(m, addr);
        set_zn(cpu, cpu->y);
        cyc = 4;
        break;
    case 0xBE: /* LDX abs,Y */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->y);
        cpu->pc = (uint16_t)(cpu->pc + 2);
        cpu->x = rd(m, addr);
        set_zn(cpu, cpu->x);
        cyc = 4;
        break;
    case 0x7C: /* JMP (abs,X) -- 65C02 */
        addr = (uint16_t)(rd16(m, cpu->pc) + cpu->x);
        cpu->pc = rd16(m, addr);
        cyc = 6;
        break;
    case 0x00: /* BRK */
        cpu->pc++;
        push(cpu, m, (uint8_t)(cpu->pc >> 8));
        push(cpu, m, (uint8_t)(cpu->pc & 0xFF));
        push(cpu, m, (uint8_t)(cpu->p | C_B | C_U));
        cpu->p |= C_I;
        cpu->pc = rd16(m, 0xFFFE);
        cyc = 7;
        break;
    default:
        /* Unknown: treat as 2-cycle NOP so bad carts don't wedge hard. */
        cyc = 2;
        break;
    }

    cpu->cycles += (uint64_t)cyc;
    return cyc;
}

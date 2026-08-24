#include "retr01_emu/cpu.h"

#include "retr01_emu/machine.h"

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
    case 0x14: { /* TRB zp — 65C02 */
        addr = rd(m, cpu->pc++);
        v = (uint8_t)(rd(m, addr) & (uint8_t)~cpu->a);
        wr(m, addr, v);
        set_zn(cpu, v);
        cyc = 5;
        break;
    }
    case 0x80: { /* BRA rel — 65C02 */
        int8_t off = (int8_t)rd(m, cpu->pc++);
        cpu->pc = (uint16_t)(cpu->pc + off);
        cyc = 3;
        break;
    }
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
    case 0x6C: { /* JMP (abs) — 65C02 page-safe */
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

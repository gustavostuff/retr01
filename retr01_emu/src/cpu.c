#include "retr01_emu/emu.h"

#include <stdint.h>

#define RD(a) retr01_bus_read(e, (uint16_t)(a))
#define WR(a, v) retr01_bus_write(e, (uint16_t)(a), (uint8_t)(v))

static uint8_t fetch(retr01_emu_t *e)
{
    return RD(e->cpu.pc++);
}

static uint16_t fetch16(retr01_emu_t *e)
{
    uint8_t lo = fetch(e);
    uint8_t hi = fetch(e);
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static uint16_t rd16(retr01_emu_t *e, uint16_t addr)
{
    uint8_t lo = RD(addr);
    uint8_t hi = RD((uint16_t)(addr + 1));
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static uint16_t rd16_zp(retr01_emu_t *e, uint8_t zp)
{
    uint8_t lo = RD(zp);
    uint8_t hi = RD((uint8_t)(zp + 1));
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static void set_zn(retr01_cpu_t *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~(RETR01_N_FLAG | RETR01_Z_FLAG)) | (v & RETR01_N_FLAG) |
                     (v == 0 ? RETR01_Z_FLAG : 0));
}

static void push(retr01_emu_t *e, uint8_t v)
{
    WR((uint16_t)(0x0100u | e->cpu.sp), v);
    e->cpu.sp--;
}

static uint8_t pull(retr01_emu_t *e)
{
    e->cpu.sp++;
    return RD((uint16_t)(0x0100u | e->cpu.sp));
}

static void push16(retr01_emu_t *e, uint16_t v)
{
    push(e, (uint8_t)(v >> 8));
    push(e, (uint8_t)v);
}

static uint16_t pull16(retr01_emu_t *e)
{
    uint8_t lo = pull(e);
    uint8_t hi = pull(e);
    return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static int branch(retr01_emu_t *e, int taken)
{
    int8_t off = (int8_t)fetch(e);
    if (!taken) {
        return 2;
    }
    uint16_t old = e->cpu.pc;
    e->cpu.pc = (uint16_t)(e->cpu.pc + off);
    return ((old ^ e->cpu.pc) & 0xFF00) ? 4 : 3;
}

static uint16_t addr_zpx(retr01_emu_t *e)
{
    return (uint8_t)(fetch(e) + e->cpu.x);
}

static uint16_t addr_zpy(retr01_emu_t *e)
{
    return (uint8_t)(fetch(e) + e->cpu.y);
}

static uint16_t addr_absx(retr01_emu_t *e)
{
    return (uint16_t)(fetch16(e) + e->cpu.x);
}

static uint16_t addr_absy(retr01_emu_t *e)
{
    return (uint16_t)(fetch16(e) + e->cpu.y);
}

static uint16_t addr_indx(retr01_emu_t *e)
{
    return rd16_zp(e, (uint8_t)(fetch(e) + e->cpu.x));
}

static uint16_t addr_indy(retr01_emu_t *e)
{
    return (uint16_t)(rd16_zp(e, fetch(e)) + e->cpu.y);
}

static uint16_t addr_izp(retr01_emu_t *e)
{
    return rd16_zp(e, fetch(e));
}

static void adc(retr01_emu_t *e, uint8_t v)
{
    retr01_cpu_t *c = &e->cpu;
    unsigned cin = c->p & RETR01_C_FLAG;
    unsigned sum = (unsigned)c->a + v + cin;
    uint8_t vflag = (uint8_t)((~(c->a ^ v) & (c->a ^ (uint8_t)sum) & 0x80) ? RETR01_V_FLAG : 0);

    if (c->p & RETR01_D_FLAG) {
        unsigned al = (c->a & 0x0Fu) + (v & 0x0Fu) + cin;
        unsigned ah = (c->a >> 4) + (v >> 4);
        if (al > 9) {
            al -= 10;
            ah++;
        }
        if (ah > 9) {
            ah -= 10;
            sum = 0x100;
        } else {
            sum = (ah << 4) | (al & 0x0F);
        }
        c->a = (uint8_t)((ah << 4) | (al & 0x0F));
        c->p = (uint8_t)((c->p & ~(RETR01_C_FLAG | RETR01_V_FLAG | RETR01_N_FLAG | RETR01_Z_FLAG)) |
                         (sum > 0xFF ? RETR01_C_FLAG : 0) | vflag);
        set_zn(c, c->a);
        return;
    }

    c->p = (uint8_t)((c->p & ~(RETR01_C_FLAG | RETR01_V_FLAG)) | (sum > 0xFF ? RETR01_C_FLAG : 0) |
                     vflag);
    c->a = (uint8_t)sum;
    set_zn(c, c->a);
}

static void sbc(retr01_emu_t *e, uint8_t v)
{
    adc(e, (uint8_t)~v);
}

static void cmp(retr01_cpu_t *c, uint8_t r, uint8_t v)
{
    uint16_t t = (uint16_t)r - v;
    c->p = (uint8_t)((c->p & ~RETR01_C_FLAG) | (r >= v ? RETR01_C_FLAG : 0));
    set_zn(c, (uint8_t)t);
}

static uint8_t asl(retr01_cpu_t *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~RETR01_C_FLAG) | ((v & 0x80) ? RETR01_C_FLAG : 0));
    v <<= 1;
    set_zn(c, v);
    return v;
}

static uint8_t lsr(retr01_cpu_t *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~RETR01_C_FLAG) | (v & RETR01_C_FLAG));
    v >>= 1;
    set_zn(c, v);
    return v;
}

static uint8_t rol(retr01_cpu_t *c, uint8_t v)
{
    uint8_t cin = (uint8_t)(c->p & RETR01_C_FLAG);
    c->p = (uint8_t)((c->p & ~RETR01_C_FLAG) | ((v & 0x80) ? RETR01_C_FLAG : 0));
    v = (uint8_t)((v << 1) | cin);
    set_zn(c, v);
    return v;
}

static uint8_t ror(retr01_cpu_t *c, uint8_t v)
{
    uint8_t cin = (uint8_t)((c->p & RETR01_C_FLAG) ? 0x80 : 0);
    c->p = (uint8_t)((c->p & ~RETR01_C_FLAG) | (v & RETR01_C_FLAG));
    v = (uint8_t)((v >> 1) | cin);
    set_zn(c, v);
    return v;
}

static void bit_op(retr01_cpu_t *c, uint8_t v)
{
    c->p = (uint8_t)((c->p & ~(RETR01_N_FLAG | RETR01_V_FLAG | RETR01_Z_FLAG)) | (v & (RETR01_N_FLAG | RETR01_V_FLAG)) |
                     ((c->a & v) == 0 ? RETR01_Z_FLAG : 0));
}

static void tsb(retr01_emu_t *e, uint16_t addr)
{
    uint8_t v = RD(addr);
    e->cpu.p = (uint8_t)((e->cpu.p & ~RETR01_Z_FLAG) | ((e->cpu.a & v) == 0 ? RETR01_Z_FLAG : 0));
    WR(addr, (uint8_t)(v | e->cpu.a));
}

static void trb(retr01_emu_t *e, uint16_t addr)
{
    uint8_t v = RD(addr);
    e->cpu.p = (uint8_t)((e->cpu.p & ~RETR01_Z_FLAG) | ((e->cpu.a & v) == 0 ? RETR01_Z_FLAG : 0));
    WR(addr, (uint8_t)(v & (uint8_t)~e->cpu.a));
}

void retr01_cpu_reset(retr01_emu_t *e)
{
    e->cpu.a = 0;
    e->cpu.x = 0;
    e->cpu.y = 0;
    e->cpu.sp = 0xFD;
    e->cpu.p = (uint8_t)(RETR01_I_FLAG | RETR01_U_FLAG);
    e->cpu.stopped = 0;
    e->cpu.waiting = 0;
    e->cpu.pc = rd16(e, 0xFFFC);
    e->cpu.cycles += 7;
}

void retr01_cpu_nmi(retr01_emu_t *e)
{
    e->cpu.waiting = 0;
    push16(e, e->cpu.pc);
    push(e, (uint8_t)((e->cpu.p & ~RETR01_B_FLAG) | RETR01_U_FLAG));
    e->cpu.p |= RETR01_I_FLAG;
    e->cpu.pc = rd16(e, 0xFFFA);
    e->cpu.cycles += 7;
}

void retr01_cpu_irq(retr01_emu_t *e)
{
    if (e->cpu.p & RETR01_I_FLAG) {
        return;
    }
    e->cpu.waiting = 0;
    push16(e, e->cpu.pc);
    push(e, (uint8_t)((e->cpu.p & ~RETR01_B_FLAG) | RETR01_U_FLAG));
    e->cpu.p |= RETR01_I_FLAG;
    e->cpu.pc = rd16(e, 0xFFFE);
    e->cpu.cycles += 7;
}

int retr01_cpu_step(retr01_emu_t *e)
{
    retr01_cpu_t *c = &e->cpu;
    uint8_t op;
    uint16_t a;
    uint8_t v;
    int cycles = 2;

    if (c->stopped) {
        c->cycles++;
        return 1;
    }
    if (c->waiting) {
        c->cycles++;
        return 1;
    }

    op = fetch(e);

    switch (op) {
    case 0x00: /* BRK */
        fetch(e);
        push16(e, c->pc);
        push(e, (uint8_t)(c->p | RETR01_B_FLAG | RETR01_U_FLAG));
        c->p |= RETR01_I_FLAG;
        c->pc = rd16(e, 0xFFFE);
        cycles = 7;
        break;
    case 0x01:
        c->a |= RD(addr_indx(e));
        set_zn(c, c->a);
        cycles = 6;
        break;
    case 0x04:
        tsb(e, fetch(e));
        cycles = 5;
        break;
    case 0x05:
        c->a |= RD(fetch(e));
        set_zn(c, c->a);
        cycles = 3;
        break;
    case 0x06:
        a = fetch(e);
        WR(a, asl(c, RD(a)));
        cycles = 5;
        break;
    case 0x08:
        push(e, (uint8_t)(c->p | RETR01_B_FLAG | RETR01_U_FLAG));
        cycles = 3;
        break;
    case 0x09:
        c->a |= fetch(e);
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x0A:
        c->a = asl(c, c->a);
        cycles = 2;
        break;
    case 0x0C:
        tsb(e, fetch16(e));
        cycles = 6;
        break;
    case 0x0D:
        c->a |= RD(fetch16(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x0E:
        a = fetch16(e);
        WR(a, asl(c, RD(a)));
        cycles = 6;
        break;
    case 0x10:
        cycles = branch(e, !(c->p & RETR01_N_FLAG));
        break;
    case 0x11:
        c->a |= RD(addr_indy(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x12:
        c->a |= RD(addr_izp(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x14:
        trb(e, fetch(e));
        cycles = 5;
        break;
    case 0x15:
        c->a |= RD(addr_zpx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x16:
        a = addr_zpx(e);
        WR(a, asl(c, RD(a)));
        cycles = 6;
        break;
    case 0x18:
        c->p &= (uint8_t)~RETR01_C_FLAG;
        cycles = 2;
        break;
    case 0x19:
        c->a |= RD(addr_absy(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x1A:
        c->a++;
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x1C:
        trb(e, fetch16(e));
        cycles = 6;
        break;
    case 0x1D:
        c->a |= RD(addr_absx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x1E:
        a = addr_absx(e);
        WR(a, asl(c, RD(a)));
        cycles = 7;
        break;
    case 0x20:
        a = fetch16(e);
        push16(e, (uint16_t)(c->pc - 1));
        c->pc = a;
        cycles = 6;
        break;
    case 0x21:
        c->a &= RD(addr_indx(e));
        set_zn(c, c->a);
        cycles = 6;
        break;
    case 0x24:
        bit_op(c, RD(fetch(e)));
        cycles = 3;
        break;
    case 0x25:
        c->a &= RD(fetch(e));
        set_zn(c, c->a);
        cycles = 3;
        break;
    case 0x26:
        a = fetch(e);
        WR(a, rol(c, RD(a)));
        cycles = 5;
        break;
    case 0x28:
        c->p = (uint8_t)(pull(e) | RETR01_U_FLAG);
        cycles = 4;
        break;
    case 0x29:
        c->a &= fetch(e);
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x2A:
        c->a = rol(c, c->a);
        cycles = 2;
        break;
    case 0x2C:
        bit_op(c, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0x2D:
        c->a &= RD(fetch16(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x2E:
        a = fetch16(e);
        WR(a, rol(c, RD(a)));
        cycles = 6;
        break;
    case 0x30:
        cycles = branch(e, c->p & RETR01_N_FLAG);
        break;
    case 0x31:
        c->a &= RD(addr_indy(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x32:
        c->a &= RD(addr_izp(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x34:
        bit_op(c, RD(addr_zpx(e)));
        cycles = 4;
        break;
    case 0x35:
        c->a &= RD(addr_zpx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x36:
        a = addr_zpx(e);
        WR(a, rol(c, RD(a)));
        cycles = 6;
        break;
    case 0x38:
        c->p |= RETR01_C_FLAG;
        cycles = 2;
        break;
    case 0x39:
        c->a &= RD(addr_absy(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x3A:
        c->a--;
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x3C:
        bit_op(c, RD(addr_absx(e)));
        cycles = 4;
        break;
    case 0x3D:
        c->a &= RD(addr_absx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x3E:
        a = addr_absx(e);
        WR(a, rol(c, RD(a)));
        cycles = 7;
        break;
    case 0x40:
        c->p = (uint8_t)(pull(e) | RETR01_U_FLAG);
        c->pc = pull16(e);
        cycles = 6;
        break;
    case 0x41:
        c->a ^= RD(addr_indx(e));
        set_zn(c, c->a);
        cycles = 6;
        break;
    case 0x45:
        c->a ^= RD(fetch(e));
        set_zn(c, c->a);
        cycles = 3;
        break;
    case 0x46:
        a = fetch(e);
        WR(a, lsr(c, RD(a)));
        cycles = 5;
        break;
    case 0x48:
        push(e, c->a);
        cycles = 3;
        break;
    case 0x49:
        c->a ^= fetch(e);
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x4A:
        c->a = lsr(c, c->a);
        cycles = 2;
        break;
    case 0x4C:
        c->pc = fetch16(e);
        cycles = 3;
        break;
    case 0x4D:
        c->a ^= RD(fetch16(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x4E:
        a = fetch16(e);
        WR(a, lsr(c, RD(a)));
        cycles = 6;
        break;
    case 0x50:
        cycles = branch(e, !(c->p & RETR01_V_FLAG));
        break;
    case 0x51:
        c->a ^= RD(addr_indy(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x52:
        c->a ^= RD(addr_izp(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0x55:
        c->a ^= RD(addr_zpx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x56:
        a = addr_zpx(e);
        WR(a, lsr(c, RD(a)));
        cycles = 6;
        break;
    case 0x58:
        c->p &= (uint8_t)~RETR01_I_FLAG;
        cycles = 2;
        break;
    case 0x59:
        c->a ^= RD(addr_absy(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x5A:
        push(e, c->y);
        cycles = 3;
        break;
    case 0x5D:
        c->a ^= RD(addr_absx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x5E:
        a = addr_absx(e);
        WR(a, lsr(c, RD(a)));
        cycles = 7;
        break;
    case 0x60:
        c->pc = (uint16_t)(pull16(e) + 1);
        cycles = 6;
        break;
    case 0x61:
        adc(e, RD(addr_indx(e)));
        cycles = 6;
        break;
    case 0x64:
        WR(fetch(e), 0);
        cycles = 3;
        break;
    case 0x65:
        adc(e, RD(fetch(e)));
        cycles = 3;
        break;
    case 0x66:
        a = fetch(e);
        WR(a, ror(c, RD(a)));
        cycles = 5;
        break;
    case 0x68:
        c->a = pull(e);
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0x69:
        adc(e, fetch(e));
        cycles = 2;
        break;
    case 0x6A:
        c->a = ror(c, c->a);
        cycles = 2;
        break;
    case 0x6C:
        a = fetch16(e);
        c->pc = rd16(e, a);
        cycles = 6;
        break;
    case 0x6D:
        adc(e, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0x6E:
        a = fetch16(e);
        WR(a, ror(c, RD(a)));
        cycles = 6;
        break;
    case 0x70:
        cycles = branch(e, c->p & RETR01_V_FLAG);
        break;
    case 0x71:
        adc(e, RD(addr_indy(e)));
        cycles = 5;
        break;
    case 0x72:
        adc(e, RD(addr_izp(e)));
        cycles = 5;
        break;
    case 0x74:
        WR(addr_zpx(e), 0);
        cycles = 4;
        break;
    case 0x75:
        adc(e, RD(addr_zpx(e)));
        cycles = 4;
        break;
    case 0x76:
        a = addr_zpx(e);
        WR(a, ror(c, RD(a)));
        cycles = 6;
        break;
    case 0x78:
        c->p |= RETR01_I_FLAG;
        cycles = 2;
        break;
    case 0x79:
        adc(e, RD(addr_absy(e)));
        cycles = 4;
        break;
    case 0x7A:
        c->y = pull(e);
        set_zn(c, c->y);
        cycles = 4;
        break;
    case 0x7C:
        a = fetch16(e);
        c->pc = rd16(e, (uint16_t)(a + c->x));
        cycles = 6;
        break;
    case 0x7D:
        adc(e, RD(addr_absx(e)));
        cycles = 4;
        break;
    case 0x7E:
        a = addr_absx(e);
        WR(a, ror(c, RD(a)));
        cycles = 7;
        break;
    case 0x80:
        cycles = branch(e, 1);
        break;
    case 0x81:
        WR(addr_indx(e), c->a);
        cycles = 6;
        break;
    case 0x84:
        WR(fetch(e), c->y);
        cycles = 3;
        break;
    case 0x85:
        WR(fetch(e), c->a);
        cycles = 3;
        break;
    case 0x86:
        WR(fetch(e), c->x);
        cycles = 3;
        break;
    case 0x88:
        c->y--;
        set_zn(c, c->y);
        cycles = 2;
        break;
    case 0x89:
        c->p = (uint8_t)((c->p & ~RETR01_Z_FLAG) | ((c->a & fetch(e)) == 0 ? RETR01_Z_FLAG : 0));
        cycles = 2;
        break;
    case 0x8A:
        c->a = c->x;
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x8C:
        WR(fetch16(e), c->y);
        cycles = 4;
        break;
    case 0x8D:
        WR(fetch16(e), c->a);
        cycles = 4;
        break;
    case 0x8E:
        WR(fetch16(e), c->x);
        cycles = 4;
        break;
    case 0x90:
        cycles = branch(e, !(c->p & RETR01_C_FLAG));
        break;
    case 0x91:
        WR(addr_indy(e), c->a);
        cycles = 6;
        break;
    case 0x92:
        WR(addr_izp(e), c->a);
        cycles = 5;
        break;
    case 0x94:
        WR(addr_zpx(e), c->y);
        cycles = 4;
        break;
    case 0x95:
        WR(addr_zpx(e), c->a);
        cycles = 4;
        break;
    case 0x96:
        WR(addr_zpy(e), c->x);
        cycles = 4;
        break;
    case 0x98:
        c->a = c->y;
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0x99:
        WR(addr_absy(e), c->a);
        cycles = 5;
        break;
    case 0x9A:
        c->sp = c->x;
        cycles = 2;
        break;
    case 0x9C:
        WR(fetch16(e), 0);
        cycles = 4;
        break;
    case 0x9D:
        WR(addr_absx(e), c->a);
        cycles = 5;
        break;
    case 0x9E:
        WR(addr_absx(e), 0);
        cycles = 5;
        break;
    case 0xA0:
        c->y = fetch(e);
        set_zn(c, c->y);
        cycles = 2;
        break;
    case 0xA1:
        c->a = RD(addr_indx(e));
        set_zn(c, c->a);
        cycles = 6;
        break;
    case 0xA2:
        c->x = fetch(e);
        set_zn(c, c->x);
        cycles = 2;
        break;
    case 0xA4:
        c->y = RD(fetch(e));
        set_zn(c, c->y);
        cycles = 3;
        break;
    case 0xA5:
        c->a = RD(fetch(e));
        set_zn(c, c->a);
        cycles = 3;
        break;
    case 0xA6:
        c->x = RD(fetch(e));
        set_zn(c, c->x);
        cycles = 3;
        break;
    case 0xA8:
        c->y = c->a;
        set_zn(c, c->y);
        cycles = 2;
        break;
    case 0xA9:
        c->a = fetch(e);
        set_zn(c, c->a);
        cycles = 2;
        break;
    case 0xAA:
        c->x = c->a;
        set_zn(c, c->x);
        cycles = 2;
        break;
    case 0xAC:
        c->y = RD(fetch16(e));
        set_zn(c, c->y);
        cycles = 4;
        break;
    case 0xAD:
        c->a = RD(fetch16(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0xAE:
        c->x = RD(fetch16(e));
        set_zn(c, c->x);
        cycles = 4;
        break;
    case 0xB0:
        cycles = branch(e, c->p & RETR01_C_FLAG);
        break;
    case 0xB1:
        c->a = RD(addr_indy(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0xB2:
        c->a = RD(addr_izp(e));
        set_zn(c, c->a);
        cycles = 5;
        break;
    case 0xB4:
        c->y = RD(addr_zpx(e));
        set_zn(c, c->y);
        cycles = 4;
        break;
    case 0xB5:
        c->a = RD(addr_zpx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0xB6:
        c->x = RD(addr_zpy(e));
        set_zn(c, c->x);
        cycles = 4;
        break;
    case 0xB8:
        c->p &= (uint8_t)~RETR01_V_FLAG;
        cycles = 2;
        break;
    case 0xB9:
        c->a = RD(addr_absy(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0xBA:
        c->x = c->sp;
        set_zn(c, c->x);
        cycles = 2;
        break;
    case 0xBC:
        c->y = RD(addr_absx(e));
        set_zn(c, c->y);
        cycles = 4;
        break;
    case 0xBD:
        c->a = RD(addr_absx(e));
        set_zn(c, c->a);
        cycles = 4;
        break;
    case 0xBE:
        c->x = RD(addr_absy(e));
        set_zn(c, c->x);
        cycles = 4;
        break;
    case 0xC0:
        cmp(c, c->y, fetch(e));
        cycles = 2;
        break;
    case 0xC1:
        cmp(c, c->a, RD(addr_indx(e)));
        cycles = 6;
        break;
    case 0xC4:
        cmp(c, c->y, RD(fetch(e)));
        cycles = 3;
        break;
    case 0xC5:
        cmp(c, c->a, RD(fetch(e)));
        cycles = 3;
        break;
    case 0xC6:
        a = fetch(e);
        v = (uint8_t)(RD(a) - 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 5;
        break;
    case 0xC8:
        c->y++;
        set_zn(c, c->y);
        cycles = 2;
        break;
    case 0xC9:
        cmp(c, c->a, fetch(e));
        cycles = 2;
        break;
    case 0xCA:
        c->x--;
        set_zn(c, c->x);
        cycles = 2;
        break;
    case 0xCB:
        c->waiting = 1;
        cycles = 3;
        break;
    case 0xCC:
        cmp(c, c->y, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0xCD:
        cmp(c, c->a, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0xCE:
        a = fetch16(e);
        v = (uint8_t)(RD(a) - 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 6;
        break;
    case 0xD0:
        cycles = branch(e, !(c->p & RETR01_Z_FLAG));
        break;
    case 0xD1:
        cmp(c, c->a, RD(addr_indy(e)));
        cycles = 5;
        break;
    case 0xD2:
        cmp(c, c->a, RD(addr_izp(e)));
        cycles = 5;
        break;
    case 0xD5:
        cmp(c, c->a, RD(addr_zpx(e)));
        cycles = 4;
        break;
    case 0xD6:
        a = addr_zpx(e);
        v = (uint8_t)(RD(a) - 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 6;
        break;
    case 0xD8:
        c->p &= (uint8_t)~RETR01_D_FLAG;
        cycles = 2;
        break;
    case 0xD9:
        cmp(c, c->a, RD(addr_absy(e)));
        cycles = 4;
        break;
    case 0xDA:
        push(e, c->x);
        cycles = 3;
        break;
    case 0xDB:
        c->stopped = 1;
        cycles = 3;
        break;
    case 0xDD:
        cmp(c, c->a, RD(addr_absx(e)));
        cycles = 4;
        break;
    case 0xDE:
        a = addr_absx(e);
        v = (uint8_t)(RD(a) - 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 7;
        break;
    case 0xE0:
        cmp(c, c->x, fetch(e));
        cycles = 2;
        break;
    case 0xE1:
        sbc(e, RD(addr_indx(e)));
        cycles = 6;
        break;
    case 0xE4:
        cmp(c, c->x, RD(fetch(e)));
        cycles = 3;
        break;
    case 0xE5:
        sbc(e, RD(fetch(e)));
        cycles = 3;
        break;
    case 0xE6:
        a = fetch(e);
        v = (uint8_t)(RD(a) + 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 5;
        break;
    case 0xE8:
        c->x++;
        set_zn(c, c->x);
        cycles = 2;
        break;
    case 0xE9:
        sbc(e, fetch(e));
        cycles = 2;
        break;
    case 0xEA:
        cycles = 2;
        break;
    case 0xEC:
        cmp(c, c->x, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0xED:
        sbc(e, RD(fetch16(e)));
        cycles = 4;
        break;
    case 0xEE:
        a = fetch16(e);
        v = (uint8_t)(RD(a) + 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 6;
        break;
    case 0xF0:
        cycles = branch(e, c->p & RETR01_Z_FLAG);
        break;
    case 0xF1:
        sbc(e, RD(addr_indy(e)));
        cycles = 5;
        break;
    case 0xF2:
        sbc(e, RD(addr_izp(e)));
        cycles = 5;
        break;
    case 0xF5:
        sbc(e, RD(addr_zpx(e)));
        cycles = 4;
        break;
    case 0xF6:
        a = addr_zpx(e);
        v = (uint8_t)(RD(a) + 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 6;
        break;
    case 0xF8:
        c->p |= RETR01_D_FLAG;
        cycles = 2;
        break;
    case 0xF9:
        sbc(e, RD(addr_absy(e)));
        cycles = 4;
        break;
    case 0xFA:
        c->x = pull(e);
        set_zn(c, c->x);
        cycles = 4;
        break;
    case 0xFD:
        sbc(e, RD(addr_absx(e)));
        cycles = 4;
        break;
    case 0xFE:
        a = addr_absx(e);
        v = (uint8_t)(RD(a) + 1);
        WR(a, v);
        set_zn(c, v);
        cycles = 7;
        break;
    default:
        cycles = 2;
        break;
    }

    c->cycles += (uint64_t)cycles;
    return cycles;
}

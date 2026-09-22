#include "retr01_emu/machine.h"
#include "stub_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

int main(void) {
    R01eMachine m;
    char err[256];
    uint8_t *stub;
    size_t stub_len = (size_t)R01E_STUB_CART_LEN;

    stub = (uint8_t *)malloc(stub_len);
    if (!stub) {
        return fail("oom stub");
    }
    if (r01e_test_stub_cart(stub, stub_len) != 0) {
        free(stub);
        return fail("build stub");
    }
    if (r01e_machine_init_mem(&m, stub, stub_len, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        free(stub);
        return 1;
    }
    free(stub);

    /* Tiny program in system RAM. */
    m.ram[0x0000] = 0xA9; /* LDA #$42 */
    m.ram[0x0001] = 0x42;
    m.ram[0x0002] = 0x85; /* STA $10 */
    m.ram[0x0003] = 0x10;
    m.ram[0x0004] = 0xA5; /* LDA $10 */
    m.ram[0x0005] = 0x10;
    m.ram[0x0006] = 0x4C; /* JMP $0006 */
    m.ram[0x0007] = 0x06;
    m.ram[0x0008] = 0x00;

    m.cpu.pc = 0x0000;
    m.cpu.a = 0;
    (void)r01e_cpu_step(&m.cpu, &m); /* LDA #$42 */
    if (m.cpu.a != 0x42) {
        r01e_machine_shutdown(&m);
        return fail("LDA imm");
    }
    (void)r01e_cpu_step(&m.cpu, &m); /* STA $10 */
    if (m.ram[0x10] != 0x42) {
        r01e_machine_shutdown(&m);
        return fail("STA zp");
    }
    m.cpu.a = 0;
    (void)r01e_cpu_step(&m.cpu, &m); /* LDA $10 */
    if (m.cpu.a != 0x42) {
        r01e_machine_shutdown(&m);
        return fail("LDA zp");
    }
    (void)r01e_cpu_step(&m.cpu, &m); /* JMP */
    if (m.cpu.pc != 0x0006) {
        fprintf(stderr, "FAIL JMP pc=$%04x\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }

    m.ram[0x40] = 0xFF;
    m.ram[0x0100] = 0x64; /* STZ $40 */
    m.ram[0x0101] = 0x40;
    m.cpu.pc = 0x0100;
    (void)r01e_cpu_step(&m.cpu, &m);
    if (m.ram[0x40] != 0) {
        r01e_machine_shutdown(&m);
        return fail("STZ zp");
    }

    m.ram[0x20] = 0x30;
    m.ram[0x21] = 0x00;
    m.ram[0x30] = 0;
    m.ram[0x0102] = 0xA9; /* LDA #$99 */
    m.ram[0x0103] = 0x99;
    m.ram[0x0104] = 0x92; /* STA ($20) */
    m.ram[0x0105] = 0x20;
    m.cpu.pc = 0x0102;
    (void)r01e_cpu_step(&m.cpu, &m);
    (void)r01e_cpu_step(&m.cpu, &m);
    if (m.ram[0x30] != 0x99) {
        r01e_machine_shutdown(&m);
        return fail("STA (zp)");
    }

    m.cpu.x = 0xAB;
    m.cpu.s = 0xFD;
    m.ram[0x0106] = 0xDA; /* PHX */
    m.cpu.pc = 0x0106;
    (void)r01e_cpu_step(&m.cpu, &m);
    if (m.ram[0x01FD] != 0xAB) {
        r01e_machine_shutdown(&m);
        return fail("PHX");
    }

    m.nmi_pending = 0;
    m.cpu.stalled = 0;
    m.ram[0x50] = 0x01;
    m.ram[0x60] = 0x46; /* LSR $50 */
    m.ram[0x61] = 0x50;
    m.cpu.pc = 0x0060;
    m.cpu.p = 0;
    (void)r01e_cpu_step(&m.cpu, &m);
    if (m.ram[0x50] != 0x00 || (m.cpu.p & 0x03) != 0x03) {
        r01e_machine_shutdown(&m);
        return fail("LSR zp");
    }

    m.ram[0x0110] = 0x80; /* BRA +2 */
    m.ram[0x0111] = 0x02;
    m.ram[0x0112] = 0xEA;
    m.ram[0x0113] = 0xEA;
    m.cpu.pc = 0x0110;
    (void)r01e_cpu_step(&m.cpu, &m);
    if (m.cpu.pc != 0x0114) {
        fprintf(stderr, "FAIL BRA pc=$%04x\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }

    /* Reset vector path: machine reset should land in PRG. */
    r01e_machine_reset(&m);
    if (m.cpu.pc < 0x8000u) {
        fprintf(stderr, "FAIL reset pc=$%04x\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok cpu lda/sta/jmp reset_pc=$%04x\n", m.cpu.pc);
    r01e_machine_shutdown(&m);
    return 0;
}

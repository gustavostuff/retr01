#include "retr01_emu/machine.h"

#include <stdio.h>
#include <string.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../../output/test.retr01"
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eMachine m;
    char err[256];

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }

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

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

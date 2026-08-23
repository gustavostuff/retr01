#include "retr01_emu/machine.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "../studio/project.retr01";
    R01eMachine m;
    char err[256];
    int i;
    int nonzero = 0;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }

    /* Run enough instructions for Studio stub to reach hang JMP. */
    for (i = 0; i < 64; i++) {
        (void)r01e_machine_step_insn(&m);
    }
    if (m.cpu.pc < 0x8000u || m.cpu.pc > 0x8020u) {
        fprintf(stderr, "FAIL pc=$%04x (expected hang near $8012)\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (!m.ppu.chr_loaded) {
        fprintf(stderr, "FAIL CHR not loaded\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    (void)r01e_machine_frame(&m);
    for (i = 0; i < (int)sizeof(m.ppu.fb); i++) {
        if (m.ppu.fb[i] != 0) {
            nonzero++;
        }
    }
    if (nonzero < 100) {
        fprintf(stderr, "FAIL framebuffer mostly empty (%d nonzero)\n", nonzero);
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok boot pc=$%04x world=%u fb_nonzero=%d\n", m.cpu.pc, (unsigned)m.ppu.world, nonzero);
    r01e_machine_shutdown(&m);
    return 0;
}

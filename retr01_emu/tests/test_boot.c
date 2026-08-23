#include "retr01_emu/machine.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "../retr01_studio/project.retr01";
    R01eMachine m;
    char err[256];
    R01eWorldView wv;
    int i;
    int nonzero = 0;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }

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

    /* World 0 holds the 12-screen atlas; stub may have selected another world. */
    if (r01e_cart_world(&m.cart, 0, &wv) != 0 || wv.screen_count < 12) {
        fprintf(stderr, "FAIL world0 screens=%u (expected >=12)\n",
                wv.present ? (unsigned)wv.screen_count : 0);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (r01e_ppu_boot_world(&m, 0) != 0) {
        fprintf(stderr, "FAIL boot world0\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (m.ppu.cam_max_x < 4 * 128) {
        fprintf(stderr, "FAIL cam_max_x=%d (expected span of cols 0..4)\n", m.ppu.cam_max_x);
        r01e_machine_shutdown(&m);
        return 1;
    }
    for (i = 0; i < 200; i++) {
        (void)r01e_ppu_host_pan(&m, 1, 0);
    }
    if (m.ppu.cam_origin_col < 1) {
        fprintf(stderr, "FAIL origin_col=%d after pan (expected shifted)\n", m.ppu.cam_origin_col);
        r01e_machine_shutdown(&m);
        return 1;
    }

    r01e_ppu_render_frame(&m);
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

    printf("ok world0 screens=%u cam=%d,%d origin=%d,%d fb_nonzero=%d\n", (unsigned)wv.screen_count,
           m.ppu.cam_x, m.ppu.cam_y, m.ppu.cam_origin_col, m.ppu.cam_origin_row, nonzero);
    r01e_machine_shutdown(&m);
    return 0;
}

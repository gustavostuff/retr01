#include "retr01_emu/machine.h"

#include <stdio.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../../output/test.retr01"
#endif

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
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
    if (m.cpu.pc < 0x8000u || m.cpu.pc >= 0x8100u) {
        fprintf(stderr, "FAIL pc=$%04x (expected stub loop in $8000-$80FF)\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (!m.video.chr_loaded) {
        fprintf(stderr, "FAIL CHR not loaded\n");
        r01e_machine_shutdown(&m);
        return 1;
    }

    if (r01e_cart_world(&m.cart, 0, &wv) != 0 || wv.screen_count < 1) {
        fprintf(stderr, "FAIL world0 screens=%u\n", wv.present ? (unsigned)wv.screen_count : 0);
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (r01e_video_boot_world(&m, 0) != 0) {
        fprintf(stderr, "FAIL boot world0\n");
        r01e_machine_shutdown(&m);
        return 1;
    }
    if (m.video.cam_max_x < R01E_SCREEN_PX_W) {
        fprintf(stderr, "FAIL cam_max_x=%d\n", m.video.cam_max_x);
        r01e_machine_shutdown(&m);
        return 1;
    }

    for (i = 0; i < 200; i++) {
        (void)r01e_video_host_pan(&m, 1, 0);
    }
    if (m.video.cam_origin_col < 1 && m.video.cam_max_x >= 2 * R01E_SCREEN_PX_W) {
        fprintf(stderr, "FAIL origin_col=%d after pan\n", m.video.cam_origin_col);
        r01e_machine_shutdown(&m);
        return 1;
    }

    r01e_video_render_frame(&m);
    for (i = 0; i < (int)sizeof(m.video.fb); i++) {
        if (m.video.fb[i] != 0) {
            nonzero++;
        }
    }
    if (nonzero < 100) {
        fprintf(stderr, "FAIL framebuffer mostly empty (%d nonzero bytes)\n", nonzero);
        r01e_machine_shutdown(&m);
        return 1;
    }

    printf("ok world0 screens=%u cam=%d,%d origin=%d,%d fb_nonzero=%d\n", (unsigned)wv.screen_count,
           m.video.cam_x, m.video.cam_y, m.video.cam_origin_col, m.video.cam_origin_row, nonzero);
    r01e_machine_shutdown(&m);
    return 0;
}

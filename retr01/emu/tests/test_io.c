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
    uint8_t v;
    uint32_t map0;

    if (r01e_machine_init(&m, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        return 1;
    }

    r01e_mem_write(&m, 0xFE02, 0x55);
    if (m.io.scroll_x != 0x55) {
        r01e_machine_shutdown(&m);
        return fail("scroll_x latch");
    }
    r01e_mem_write(&m, 0xFE02, 0xFF);
    if (m.io.scroll_x != 0x7F) {
        r01e_machine_shutdown(&m);
        return fail("scroll_x clamp");
    }
    r01e_mem_write(&m, 0xFE03, 0x80);
    if (m.io.scroll_y != 119) {
        r01e_machine_shutdown(&m);
        return fail("scroll_y clamp");
    }

    r01e_mem_write(&m, 0xFE10, 0x00);
    r01e_mem_write(&m, 0xFE11, 0x00);
    r01e_mem_write(&m, 0xFE12, 0xAA);
    r01e_mem_write(&m, 0xFE12, 0xBB);
    if (m.video.vram[0] != 0xAA || m.video.vram[1] != 0xBB) {
        r01e_machine_shutdown(&m);
        return fail("VRAM auto-inc write");
    }
    r01e_mem_write(&m, 0xFE10, 0x00);
    r01e_mem_write(&m, 0xFE11, 0x00);
    v = r01e_mem_read(&m, 0xFE12);
    if (v != 0xAA) {
        r01e_machine_shutdown(&m);
        return fail("VRAM auto-inc read");
    }

    map0 = m.cart.off_prg; /* known absolute in image; any readable byte */
    r01e_mem_write(&m, 0xFE90, (uint8_t)(map0 & 0xFF));
    r01e_mem_write(&m, 0xFE91, (uint8_t)((map0 >> 8) & 0xFF));
    r01e_mem_write(&m, 0xFE92, (uint8_t)((map0 >> 16) & 0xFF));
    v = r01e_mem_read(&m, 0xFE93);
    if (v != r01e_cart_read(&m.cart, map0)) {
        r01e_machine_shutdown(&m);
        return fail("MAP $FE93 read");
    }
    if (m.io.map_addr != ((map0 + 1) & 0xFFFFFFu)) {
        r01e_machine_shutdown(&m);
        return fail("MAP auto-inc");
    }

    /* Unused $FE80: open as 0 today. */
    v = r01e_mem_read(&m, 0xFE80);
    if (v != 0) {
        r01e_machine_shutdown(&m);
        return fail("$FE80 unused read");
    }
    r01e_mem_write(&m, 0xFE80, 0x03); /* must not bank PRG */
    if (r01e_mem_read(&m, 0xFE80) != 0) {
        r01e_machine_shutdown(&m);
        return fail("$FE80 write ignored");
    }

    printf("ok io scroll/vram/map/fe80\n");
    r01e_machine_shutdown(&m);
    return 0;
}

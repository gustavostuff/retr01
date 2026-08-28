#include "test_harness.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CART_HDR_SIZE 16u
#define CART_PTR_SIZE 24u
#define CART_PAL_PLANE_BYTES 128u
#define CART_PRG_OFF (CART_HDR_SIZE + CART_PTR_SIZE + 2u * CART_PAL_PLANE_BYTES)
#define PRG_PLAY_SPAWN_C 0x0108u
#define PRG_PLAY_SPAWN_R 0x0109u

TEST_MAIN() {
    R01Project *p = (R01Project *)calloc(1, sizeof(R01Project));
    char err[128];
    int i;

    EXPECT(p != NULL, "alloc project");
    if (!p) {
        return 1;
    }

    r01_project_init(p, "cart");
    for (i = 0; i < p->worlds[0].screen_count; i++) {
        p->worlds[0].screens[i].present = 1;
    }
    p->worlds[0].default_screen = 2;

    EXPECT(r01_cart_write(p, "test_cart.retr01", err, sizeof(err)) == 0, "cart write");
    {
        FILE *f = fopen("test_cart.retr01", "rb");
        char magic[6];
        uint8_t prg_spawn[2];
        long prg_off = (long)CART_PRG_OFF;

        EXPECT(f != NULL, "open cart");
        if (f) {
            EXPECT(fread(magic, 1, 6, f) == 6, "read cart magic");
            EXPECT(memcmp(magic, "retr01", 6) == 0, "cart magic");
            EXPECT(fseek(f, prg_off + (long)PRG_PLAY_SPAWN_C, SEEK_SET) == 0, "seek prg spawn");
            EXPECT(fread(prg_spawn, 1, 2, f) == 2, "read prg spawn");
            EXPECT(prg_spawn[0] == 2, "prg spawn col matches default screen");
            EXPECT(prg_spawn[1] == 0, "prg spawn row matches default screen");
            fclose(f);
        }
    }

    EXPECT(r01_prom_write("test_prom.bin", err, sizeof(err)) == 0, "prom write");
    {
        FILE *f = fopen("test_prom.bin", "rb");
        uint8_t prom[R01_MASTER_COLORS];
        EXPECT(f != NULL, "open prom");
        if (f) {
            EXPECT(fread(prom, 1, sizeof(prom), f) == sizeof(prom), "prom size");
            fclose(f);
        }
    }

    free(p);
    TEST_EXIT();
}

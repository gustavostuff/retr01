#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/palette.h"
#include "retr01/screen.h"
#include "retr01_emu/ppu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s --cart path.retr01 [--world N] [--col C] [--row R] [--dump-fb out.raw]\n",
            argv0);
}

static int load_palette(retr01_master_palette_t *pal)
{
    const char *paths[] = {
        "retr01_world_studio/retr01_palette_v_01.txt",
        "../retr01_world_studio/retr01_palette_v_01.txt",
        RETR01_PALETTE_V01_PATH,
        NULL,
    };
    size_t i;
    for (i = 0; paths[i]; i++) {
        if (retr01_palette_load_v01(paths[i], pal) == 0) {
            return 0;
        }
    }
    retr01_palette_set_defaults(pal);
    return -1;
}

int main(int argc, char **argv)
{
    const char *cart_path = NULL;
    const char *dump_path = NULL;
    int world = 0;
    int col = 0;
    int row = 0;
    retr01_cart_t cart;
    retr01_ppu_t ppu;
    retr01_screen_t screen;
    uint8_t rgba[RETR01_FB_W * RETR01_FB_H * 4];
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cart") == 0 && i + 1 < argc) {
            cart_path = argv[++i];
        } else if (strcmp(argv[i], "--world") == 0 && i + 1 < argc) {
            world = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--col") == 0 && i + 1 < argc) {
            col = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--row") == 0 && i + 1 < argc) {
            row = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dump-fb") == 0 && i + 1 < argc) {
            dump_path = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (!cart_path) {
        usage(argv[0]);
        return 1;
    }

    retr01_cart_init(&cart);
    if (retr01_cart_load_file(cart_path, &cart) != 0) {
        fprintf(stderr, "failed to load cart: %s\n", cart_path);
        return 1;
    }

    if (retr01_map_load_screen(&cart, world, (uint8_t)col, (uint8_t)row, &screen) != 0) {
        fprintf(stderr, "load_screen failed world=%d col=%d row=%d\n", world, col, row);
        retr01_cart_free(&cart);
        return 1;
    }

    retr01_ppu_init(&ppu);
    ppu.screen = screen;
    ppu.chr = cart.chr;
    ppu.chr_size = cart.chr_size;
    load_palette(&ppu.palette);

    retr01_ppu_render_bg(&ppu, rgba);

    if (dump_path) {
        FILE *f = fopen(dump_path, "wb");
        if (!f || fwrite(rgba, 1, sizeof(rgba), f) != sizeof(rgba)) {
            fprintf(stderr, "failed to write %s\n", dump_path);
            retr01_cart_free(&cart);
            return 1;
        }
        fclose(f);
        printf("wrote %s (%zu bytes)\n", dump_path, sizeof(rgba));
    } else {
        printf("rendered %dx%d (use --dump-fb to save RGBA)\n", RETR01_FB_W, RETR01_FB_H);
    }

    retr01_cart_free(&cart);
    return 0;
}

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *out_path = "tests/fixtures/one_screen.retr01";
    retr01_map_build_screen_t screens[1];
    retr01_map_build_world_t worlds[1];
    retr01_cart_t cart;
    uint8_t *map = NULL;
    size_t map_len = 0;

    if (argc > 1) {
        out_path = argv[1];
    }

    retr01_screen_clear(&screens[0].screen);
    screens[0].screen.col = 0;
    screens[0].screen.row = 0;
    memset(screens[0].screen.tiles, 1, RETR01_SCREEN_TILE_BYTES);

    worlds[0].desc.grid_w = 1;
    worlds[0].desc.grid_h = 1;
    worlds[0].desc.empty_off = 0;
    worlds[0].screens = screens;
    worlds[0].screen_count = 1;

    if (retr01_map_build(worlds, 1, &map, &map_len) != 0) {
        fprintf(stderr, "map build failed\n");
        return 1;
    }

    retr01_cart_init(&cart);
    cart.prg = (uint8_t *)malloc(4);
    if (!cart.prg) {
        free(map);
        return 1;
    }
    memset(cart.prg, 0xEA, 4);
    cart.prg_size = 4;

    cart.chr = (uint8_t *)calloc(512, 1);
    if (!cart.chr) {
        retr01_cart_free(&cart);
        free(map);
        return 1;
    }
    cart.chr_size = 512;
    for (int y = 0; y < 8; y++) {
        cart.chr[16 + y] = 0xFF;
    }

    cart.map = map;
    cart.map_size = map_len;
    cart.world_count = 1;

    if (retr01_cart_write_file(out_path, &cart) != 0) {
        fprintf(stderr, "write failed\n");
        retr01_cart_free(&cart);
        return 1;
    }

    printf("wrote %s\n", out_path);
    retr01_cart_free(&cart);
    return 0;
}

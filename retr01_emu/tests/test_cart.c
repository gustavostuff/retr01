#include "retr01_emu/cart.h"

#include <stdio.h>
#include <string.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../rom/test.retr01"
#endif

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eCart cart;
    R01eWorldView wv;
    char err[256];
    const uint8_t *prg;

    if (r01e_cart_load_path(&cart, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL load: %s\n", err);
        return 1;
    }
    if (memcmp(cart.data, "retr01", 6) != 0) {
        fprintf(stderr, "FAIL magic\n");
        return 1;
    }
    prg = r01e_cart_prg(&cart);
    if (!prg || cart.len_prg < 16) {
        fprintf(stderr, "FAIL prg\n");
        return 1;
    }
    if (prg[0] != 0x78) {
        fprintf(stderr, "FAIL stub opcode %02x\n", prg[0]);
        return 1;
    }
    if (r01e_cart_world(&cart, 0, &wv) != 0 || !wv.present) {
        fprintf(stderr, "FAIL world0\n");
        return 1;
    }
    printf("ok cart %zu B worlds=%u prg@$%06x world0 screens=%u\n", cart.len,
           (unsigned)cart.world_count, cart.off_prg, (unsigned)wv.screen_count);
    r01e_cart_free(&cart);
    return 0;
}

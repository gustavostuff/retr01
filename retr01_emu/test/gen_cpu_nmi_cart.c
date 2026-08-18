#include "nmi_prg.h"
#include "retr01/cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *out_path = "cpu_nmi.retr01";
    retr01_cart_t cart;
    uint8_t *prg;

    if (argc > 1) {
        out_path = argv[1];
    }

    prg = (uint8_t *)malloc(0x8000);
    if (!prg) {
        return 1;
    }
    retr01_test_fill_nmi_prg(prg, 0x8000);

    retr01_cart_init(&cart);
    cart.prg = prg;
    cart.prg_size = 0x8000;
    cart.world_count = 0;

    if (retr01_cart_write_file(out_path, &cart) != 0) {
        fprintf(stderr, "write failed: %s\n", out_path);
        retr01_cart_free(&cart);
        return 1;
    }
    printf("wrote %s\n", out_path);
    retr01_cart_free(&cart);
    return 0;
}

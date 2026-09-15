#include "retr01_emu/cart.h"
#include "stub_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

int main(void) {
    R01eCart cart;
    char err[256];
    const uint8_t *prg;
    uint8_t bad[64];
    R01eCart junk;
    uint8_t *stub;
    size_t stub_len = (size_t)R01E_STUB_CART_LEN;

    /* --- reject bad format (no external ROM) --- */
    memset(bad, 0, sizeof(bad));
    memcpy(bad, "retr01", 6);
    bad[6] = 1; /* legacy format_ver */
    bad[7] = 1;
    if (r01e_cart_load_mem(&junk, bad, sizeof(bad), err, sizeof(err)) == 0) {
        r01e_cart_free(&junk);
        return fail("accepted format_ver 1");
    }
    bad[6] = R01E_CART_FORMAT_VER;
    if (r01e_cart_load_mem(&junk, bad, 20, err, sizeof(err)) == 0) {
        r01e_cart_free(&junk);
        return fail("accepted truncated cart");
    }
    memcpy(bad, "xxxxxx", 6);
    bad[6] = R01E_CART_FORMAT_VER;
    if (r01e_cart_load_mem(&junk, bad, sizeof(bad), err, sizeof(err)) == 0) {
        r01e_cart_free(&junk);
        return fail("accepted bad magic");
    }

    stub = (uint8_t *)malloc(stub_len);
    if (!stub) {
        return fail("oom stub");
    }
    if (r01e_test_stub_cart(stub, stub_len) != 0) {
        free(stub);
        return fail("build stub");
    }
    if (r01e_cart_load_mem(&cart, stub, stub_len, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL load stub: %s\n", err);
        free(stub);
        return 1;
    }
    free(stub);

    if (cart.format_ver != R01E_CART_FORMAT_VER) {
        r01e_cart_free(&cart);
        return fail("format_ver");
    }
    if (memcmp(cart.data, "retr01", 6) != 0) {
        r01e_cart_free(&cart);
        return fail("magic");
    }
    prg = r01e_cart_prg(&cart);
    if (!prg || cart.len_prg != R01E_PRG_BYTES) {
        r01e_cart_free(&cart);
        return fail("prg size");
    }
    if (prg[0] != 0x78) {
        fprintf(stderr, "FAIL stub opcode %02x\n", prg[0]);
        r01e_cart_free(&cart);
        return 1;
    }

    printf("ok cart stub %zu B prg=%u\n", cart.len, (unsigned)cart.len_prg);
    r01e_cart_free(&cart);
    return 0;
}

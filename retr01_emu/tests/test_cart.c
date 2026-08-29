#include "retr01_emu/cart.h"

#include <stdio.h>
#include <string.h>

#ifdef R01_DEFAULT_CART
#define R01E_TEST_CART R01_DEFAULT_CART
#else
#define R01E_TEST_CART "../rom/test.retr01"
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01E_TEST_CART;
    R01eCart cart;
    R01eWorldView wv;
    char err[256];
    const uint8_t *prg;
    const uint8_t *credits;
    const uint8_t *other;
    size_t credits_len = 0;
    uint8_t bad[64];
    R01eCart junk;

    /* --- reject bad format --- */
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

    if (r01e_cart_load_path(&cart, path, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL load: %s\n", err);
        return 1;
    }
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
    if (r01e_cart_world(&cart, 0, &wv) != 0 || !wv.present) {
        r01e_cart_free(&cart);
        return fail("world0");
    }

    other = r01e_cart_other_payload(&cart, R01E_CART_OTHER_TITLE);
    /* Title may be empty payload offset 0 -- still OK if len_other > 0 */
    if (cart.len_other > 0 && other == NULL) {
        /* empty other blob still parses; missing id is ok */
    }
    credits = r01e_cart_credits(&cart, &credits_len);
    (void)credits;
    (void)credits_len;

    /* Collision helpers are callable on packed cart. */
    (void)r01e_cart_solid_at(&cart, 0, wv.start_col * R01E_SCREEN_PX_W + 4,
                             wv.start_row * R01E_SCREEN_PX_H + 4);
    (void)r01e_cart_player_aabb_ok(&cart, 0, wv.start_col * R01E_SCREEN_PX_W + 60,
                                   wv.start_row * R01E_SCREEN_PX_H + 56);

    printf("ok cart %zu B worlds=%u prg=%u screens=%u other=%u credits=%zu\n", cart.len,
           (unsigned)cart.world_count, (unsigned)cart.len_prg, (unsigned)wv.screen_count,
           (unsigned)cart.len_other, credits_len);
    r01e_cart_free(&cart);
    return 0;
}

#include "retr01_emu/machine.h"
#include "stub_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int load_prg(const char *path, uint8_t *dst, size_t dst_len) {
    FILE *f;
    size_t n;
    if (!path || !dst || dst_len < R01E_PRG_BYTES) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    n = fread(dst, 1, R01E_PRG_BYTES, f);
    fclose(f);
    return n == R01E_PRG_BYTES ? 0 : -1;
}

int main(int argc, char **argv) {
    R01eMachine m;
    char err[256];
    uint8_t *stub;
    uint8_t prg[R01E_PRG_BYTES];
    const char *prg_path;
    int i;

    prg_path = (argc > 1 && argv[1] && argv[1][0]) ? argv[1] : NULL;
    if (!prg_path) {
        return fail("need llvm-mos PRG path");
    }
    if (load_prg(prg_path, prg, sizeof(prg)) != 0) {
        return fail("read PRG");
    }
    if (prg[0] != 0x78) {
        return fail("PRG reset is not SEI");
    }
    if (prg[0x7FFC] != 0x00 || prg[0x7FFD] != 0x80) {
        fprintf(stderr, "FAIL RESET vector $%02x%02x\n", prg[0x7FFD], prg[0x7FFC]);
        return 1;
    }

    uint16_t nmi;
    uint16_t irq;
    char listing_path[1024];
    FILE *listing;

    nmi = (uint16_t)prg[0x7FFA] | ((uint16_t)prg[0x7FFB] << 8);
    irq = (uint16_t)prg[0x7FFE] | ((uint16_t)prg[0x7FFF] << 8);
    if (nmi < 0x8000u || irq < 0x8000u) {
        fprintf(stderr, "FAIL vectors nmi=$%04x irq=$%04x\n", nmi, irq);
        return 1;
    }

    {
        const char *slash = strrchr(prg_path, '/');
        size_t dir_len = slash ? (size_t)(slash - prg_path) : 1u;
        if (dir_len + 13u >= sizeof(listing_path)) {
            return fail("listing path");
        }
        if (slash) {
            memcpy(listing_path, prg_path, dir_len);
            listing_path[dir_len] = '\0';
        } else {
            listing_path[0] = '.';
            listing_path[1] = '\0';
        }
        if (strlen(listing_path) + 13u >= sizeof(listing_path)) {
            return fail("listing path");
        }
        memcpy(listing_path + strlen(listing_path), "/listing.txt", 13u);
    }
    listing = fopen(listing_path, "r");
    if (!listing) {
        return fail("listing.txt");
    }
    {
        char line[512];
        int saw_init = 0;
        int saw_tick = 0;
        int saw_wait = 0;
        while (fgets(line, (int)sizeof(line), listing)) {
            if (strstr(line, "r01_game_on_init")) {
                saw_init = 1;
            }
            if (strstr(line, "r01_game_on_tick")) {
                saw_tick = 1;
            }
            if (strstr(line, "r01_ppu_wait_vblank")) {
                saw_wait = 1;
            }
        }
        fclose(listing);
        if (!saw_init || !saw_tick || !saw_wait) {
            return fail("listing missing hook symbols");
        }
    }

    stub = (uint8_t *)malloc((size_t)R01E_STUB_CART_LEN);
    if (!stub) {
        return fail("oom stub");
    }
    if (r01e_test_stub_cart(stub, (size_t)R01E_STUB_CART_LEN) != 0) {
        free(stub);
        return fail("build stub");
    }
    memcpy(stub + R01E_STUB_PRG_OFF, prg, R01E_PRG_BYTES);

    if (r01e_machine_init_mem(&m, stub, (size_t)R01E_STUB_CART_LEN, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        free(stub);
        return 1;
    }
    free(stub);

    if (m.cpu.pc < 0x8000u) {
        fprintf(stderr, "FAIL reset pc=$%04x\n", m.cpu.pc);
        r01e_machine_shutdown(&m);
        return 1;
    }

    for (i = 0; i < 20000; i++) {
        (void)r01e_machine_step_insn(&m);
        if (m.cpu.pc < 0x8000u) {
            fprintf(stderr, "FAIL left PRG pc=$%04x\n", m.cpu.pc);
            r01e_machine_shutdown(&m);
            return 1;
        }
    }

    printf("ok sdk prg pc=$%04x nmi=$%04x\n", m.cpu.pc, nmi);
    r01e_machine_shutdown(&m);
    return 0;
}

#include "retr01/cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    char magic[6];
    uint16_t format_version;
    uint8_t flags;
    uint8_t world_count;
    uint32_t prg_size;
    uint32_t chr_size;
    uint32_t map_size;
    uint32_t prg_load_addr;
    uint8_t reserved[22];
} retr01_cart_hdr_t;

_Static_assert(sizeof(retr01_cart_hdr_t) == RETR01_CART_HEADER_SIZE,
               "cart header must be 0x30 bytes");

void retr01_cart_init(retr01_cart_t *cart)
{
    memset(cart, 0, sizeof(*cart));
}

void retr01_cart_free(retr01_cart_t *cart)
{
    free(cart->prg);
    free(cart->chr);
    free(cart->map);
    retr01_cart_init(cart);
}

int retr01_cart_load_file(const char *path, retr01_cart_t *out)
{
    FILE *f = fopen(path, "rb");
    retr01_cart_hdr_t hdr;
    long file_size;
    size_t n;

    if (!f || !out) {
        if (f) {
            fclose(f);
        }
        return -1;
    }

    retr01_cart_free(out);

    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return -1;
    }

    if (memcmp(hdr.magic, RETR01_CART_MAGIC, 6) != 0) {
        fclose(f);
        return -1;
    }

    if (hdr.format_version != 1) {
        fclose(f);
        return -1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    file_size = ftell(f);
    if (file_size < 0) {
        fclose(f);
        return -1;
    }
    if ((size_t)file_size <
        RETR01_CART_HEADER_SIZE + hdr.prg_size + hdr.chr_size + hdr.map_size) {
        fclose(f);
        return -1;
    }

    if (fseek(f, (long)RETR01_CART_HEADER_SIZE, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    out->prg_size = hdr.prg_size;
    out->chr_size = hdr.chr_size;
    out->map_size = hdr.map_size;
    out->world_count = hdr.world_count;

    if (hdr.prg_size > 0) {
        out->prg = (uint8_t *)malloc(hdr.prg_size);
        if (!out->prg) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
        n = fread(out->prg, 1, hdr.prg_size, f);
        if (n != hdr.prg_size) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
    }

    if (hdr.chr_size > 0) {
        out->chr = (uint8_t *)malloc(hdr.chr_size);
        if (!out->chr) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
        n = fread(out->chr, 1, hdr.chr_size, f);
        if (n != hdr.chr_size) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
    }

    if (hdr.map_size > 0) {
        out->map = (uint8_t *)malloc(hdr.map_size);
        if (!out->map) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
        n = fread(out->map, 1, hdr.map_size, f);
        if (n != hdr.map_size) {
            retr01_cart_free(out);
            fclose(f);
            return -1;
        }
    }

    fclose(f);
    return 0;
}

int retr01_cart_write_file(const char *path, const retr01_cart_t *cart)
{
    FILE *f = fopen(path, "wb");
    retr01_cart_hdr_t hdr;

    if (!f || !cart) {
        if (f) {
            fclose(f);
        }
        return -1;
    }

    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, RETR01_CART_MAGIC, 6);
    hdr.format_version = 1;
    hdr.world_count = cart->world_count;
    hdr.prg_size = (uint32_t)cart->prg_size;
    hdr.chr_size = (uint32_t)cart->chr_size;
    hdr.map_size = (uint32_t)cart->map_size;

    if (fwrite(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return -1;
    }

    if (cart->prg_size > 0 &&
        fwrite(cart->prg, 1, cart->prg_size, f) != cart->prg_size) {
        fclose(f);
        return -1;
    }
    if (cart->chr_size > 0 &&
        fwrite(cart->chr, 1, cart->chr_size, f) != cart->chr_size) {
        fclose(f);
        return -1;
    }
    if (cart->map_size > 0 &&
        fwrite(cart->map, 1, cart->map_size, f) != cart->map_size) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

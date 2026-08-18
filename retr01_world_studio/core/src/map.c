#include "retr01/map.h"

#include "retr01/rle.h"

#include <stdlib.h>
#include <string.h>

#define MAP_HDR_SIZE 30

static void write_u24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
}

static uint32_t read_u24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} buf_t;

static int buf_reserve(buf_t *b, size_t need)
{
    if (b->len + need <= b->cap) {
        return 0;
    }
    size_t ncap = b->cap ? b->cap : 256;
    while (ncap < b->len + need) {
        ncap *= 2;
    }
    uint8_t *p = (uint8_t *)realloc(b->data, ncap);
    if (!p) {
        return -1;
    }
    b->data = p;
    b->cap = ncap;
    return 0;
}

static int buf_append(buf_t *b, const void *src, size_t n)
{
    if (buf_reserve(b, n) != 0) {
        return -1;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}

static int buf_append_u8(buf_t *b, uint8_t v)
{
    return buf_append(b, &v, 1);
}

static int buf_append_u24(buf_t *b, uint32_t v)
{
    uint8_t tmp[3];
    write_u24(tmp, v);
    return buf_append(b, tmp, 3);
}

static int build_world_blob(const retr01_map_build_world_t *world, buf_t *out)
{
    size_t i;
    size_t dir_count = world->screen_count;
    uint8_t *rle_blobs[RETR01_MAX_SCREENS_PER_WORLD];
    size_t rle_lens[RETR01_MAX_SCREENS_PER_WORLD];
    uint32_t payload_region;

    if (dir_count > RETR01_MAX_SCREENS_PER_WORLD) {
        return -1;
    }

    memset(rle_blobs, 0, sizeof(rle_blobs));
    memset(rle_lens, 0, sizeof(rle_lens));

    payload_region = 6u + (uint32_t)(dir_count * 6u);

    for (i = 0; i < dir_count; i++) {
        const retr01_screen_t *sc = &world->screens[i].screen;
        size_t cap = RETR01_SCREEN_BYTES * 2;
        rle_blobs[i] = (uint8_t *)malloc(cap);
        if (!rle_blobs[i]) {
            goto fail;
        }
        if (retr01_screen_rle_encode(sc->tiles, sc->attrs, rle_blobs[i], cap, &rle_lens[i]) != 0) {
            goto fail;
        }
    }

    if (buf_append_u8(out, world->desc.grid_w) != 0 ||
        buf_append_u8(out, world->desc.grid_h) != 0 ||
        buf_append_u8(out, (uint8_t)dir_count) != 0 ||
        buf_append_u24(out, world->desc.empty_off) != 0) {
        goto fail;
    }

    {
        uint32_t running = payload_region;
        for (i = 0; i < dir_count; i++) {
            const retr01_screen_t *sc = &world->screens[i].screen;
            if (buf_append_u8(out, sc->col) != 0 || buf_append_u8(out, sc->row) != 0 ||
                buf_append_u8(out, sc->flags) != 0 || buf_append_u24(out, running) != 0) {
                goto fail;
            }
            running += (uint32_t)rle_lens[i];
        }
    }

    for (i = 0; i < dir_count; i++) {
        if (buf_append(out, rle_blobs[i], rle_lens[i]) != 0) {
            goto fail;
        }
        free(rle_blobs[i]);
        rle_blobs[i] = NULL;
    }

    return 0;

fail:
    for (i = 0; i < RETR01_MAX_SCREENS_PER_WORLD; i++) {
        free(rle_blobs[i]);
    }
    return -1;
}

int retr01_map_build(const retr01_map_build_world_t *worlds, size_t world_count,
                     uint8_t **out, size_t *out_len)
{
    buf_t map = {0};
    size_t i;
    uint32_t world_base[RETR01_MAX_WORLDS];
    uint8_t map_hdr[MAP_HDR_SIZE];

    if (!worlds || !out || !out_len || world_count == 0 ||
        world_count > RETR01_MAX_WORLDS) {
        return -1;
    }

    memset(world_base, 0, sizeof(world_base));

    memset(map_hdr, 0, sizeof(map_hdr));
    map_hdr[0] = RETR01_MAP_MAGIC_0;
    map_hdr[1] = RETR01_MAP_MAGIC_1;
    map_hdr[2] = RETR01_MAP_MAGIC_2;
    map_hdr[3] = RETR01_MAP_MAGIC_3;
    map_hdr[4] = 1; /* version */

    if (buf_append(&map, map_hdr, 6) != 0) {
        free(map.data);
        return -1;
    }
    /* reserve world_base slots (8 x 3 bytes) */
    size_t base_pos = map.len;
    if (buf_reserve(&map, 24) != 0) {
        free(map.data);
        return -1;
    }
    map.len += 24;

    {
        uint8_t present = 0;
        for (i = 0; i < world_count && i < RETR01_MAX_WORLDS; i++) {
            if (worlds[i].screen_count == 0) {
                world_base[i] = 0;
                continue;
            }
            world_base[i] = (uint32_t)map.len;
            if (build_world_blob(&worlds[i], &map) != 0) {
                free(map.data);
                return -1;
            }
            present++;
        }
        if (present == 0) {
            free(map.data);
            return -1;
        }
        map.data[5] = present;
    }

    for (i = 0; i < RETR01_MAX_WORLDS; i++) {
        write_u24(map.data + base_pos + i * 3, world_base[i]);
    }

    *out = map.data;
    *out_len = map.len;
    return 0;
}

static const uint8_t *map_world_ptr(const uint8_t *map, size_t map_len, int world_index,
                                    uint8_t *grid_w, uint8_t *grid_h, uint8_t *screen_count,
                                    uint32_t *empty_off, const uint8_t **dir_ptr)
{
    uint32_t world_base[RETR01_MAX_WORLDS];
    size_t i;
    const uint8_t *w;

    if (!map || map_len < MAP_HDR_SIZE || world_index < 0 ||
        world_index >= RETR01_MAX_WORLDS) {
        return NULL;
    }
    if (map[0] != RETR01_MAP_MAGIC_0 || map[1] != RETR01_MAP_MAGIC_1 ||
        map[2] != RETR01_MAP_MAGIC_2 || map[3] != RETR01_MAP_MAGIC_3) {
        return NULL;
    }

    for (i = 0; i < RETR01_MAX_WORLDS; i++) {
        world_base[i] = read_u24(map + 6 + i * 3);
    }

    if (world_base[world_index] == 0 || world_base[world_index] >= map_len) {
        return NULL;
    }

    w = map + world_base[world_index];
    if ((size_t)(w - map) + 6 > map_len) {
        return NULL;
    }

    *grid_w = w[0];
    *grid_h = w[1];
    *screen_count = w[2];
    *empty_off = read_u24(w + 3);
    *dir_ptr = w + 6;

    if (*screen_count > RETR01_MAX_SCREENS_PER_WORLD) {
        return NULL;
    }
    if ((size_t)(*dir_ptr - map) + (size_t)(*screen_count) * 6 > map_len) {
        return NULL;
    }

    return w;
}

int retr01_map_load_screen(const retr01_cart_t *cart, int world_index,
                           uint8_t col, uint8_t row, retr01_screen_t *screen_out)
{
    uint8_t grid_w, grid_h, screen_count;
    uint32_t empty_off;
    const uint8_t *dir;
    const uint8_t *world;
    size_t i;
    uint32_t world_this;

    if (!cart || !screen_out || !cart->map || cart->map_size == 0) {
        return -1;
    }

    world = map_world_ptr(cart->map, cart->map_size, world_index, &grid_w, &grid_h,
                          &screen_count, &empty_off, &dir);
    if (!world) {
        return -1;
    }

    world_this = read_u24(cart->map + 6 + (size_t)world_index * 3);

    (void)grid_w;
    (void)grid_h;
    (void)empty_off;

    for (i = 0; i < screen_count; i++) {
        const uint8_t *e = dir + i * 6;
        if (e[0] == col && e[1] == row) {
            uint32_t data_off = read_u24(e + 3);
            const uint8_t *payload = world + data_off;
            size_t payload_len;
            size_t world_end = cart->map_size;
            size_t wi;

            if (payload < world || payload >= cart->map + cart->map_size) {
                return -1;
            }

            for (wi = 0; wi < RETR01_MAX_WORLDS; wi++) {
                uint32_t base = read_u24(cart->map + 6 + wi * 3);
                if (base > world_this && base < world_end) {
                    world_end = base;
                }
            }

            if (i + 1 < screen_count) {
                payload_len = (size_t)(read_u24(dir + (i + 1) * 6 + 3) - data_off);
            } else {
                payload_len = world_end - (size_t)(payload - cart->map);
            }

            if (payload_len == 0 || (size_t)(payload - cart->map) + payload_len > cart->map_size) {
                return -1;
            }

            memset(screen_out, 0, sizeof(*screen_out));
            screen_out->col = col;
            screen_out->row = row;
            screen_out->flags = e[2];

            if (retr01_screen_rle_decode(payload, payload_len, screen_out->tiles,
                                         screen_out->attrs) != 0) {
                return -1;
            }
            return 0;
        }
    }

    return -1;
}

int retr01_map_world_count(const retr01_cart_t *cart)
{
    if (!cart || !cart->map || cart->map_size < MAP_HDR_SIZE) {
        return -1;
    }
    if (cart->map[0] != RETR01_MAP_MAGIC_0 || cart->map[1] != RETR01_MAP_MAGIC_1 ||
        cart->map[2] != RETR01_MAP_MAGIC_2 || cart->map[3] != RETR01_MAP_MAGIC_3) {
        return -1;
    }
    return (int)cart->map[5];
}

int retr01_map_list_cells(const retr01_cart_t *cart, int world_index, retr01_map_cell_t *out,
                          int max_out, int *out_count)
{
    uint8_t grid_w, grid_h, screen_count;
    uint32_t empty_off;
    const uint8_t *dir;
    const uint8_t *world;
    int i;
    int n;

    if (!cart || !out_count) {
        return -1;
    }

    world = map_world_ptr(cart->map, cart->map_size, world_index, &grid_w, &grid_h, &screen_count,
                          &empty_off, &dir);
    if (!world) {
        *out_count = 0;
        return -1;
    }

    n = (int)screen_count;
    if (n > max_out) {
        n = max_out;
    }
    if (out) {
        for (i = 0; i < n; i++) {
            const uint8_t *e = dir + i * 6;
            out[i].col = e[0];
            out[i].row = e[1];
            out[i].flags = e[2];
        }
    }
    *out_count = (int)screen_count;
    return 0;
}

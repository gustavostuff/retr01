#include "retr01/project.h"

#include "retr01/cart.h"
#include "retr01/map.h"
#include "retr01/palette.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_nibble(int c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int decode_hex(const char *hex, uint8_t *out, size_t out_len)
{
    size_t i;
    size_t pos = 0;

    if (!hex || !out) {
        return -1;
    }

    while (hex[pos] && isspace((unsigned char)hex[pos])) {
        pos++;
    }

    for (i = 0; i < out_len; i++) {
        int hi;
        int lo;
        while (hex[pos] && isspace((unsigned char)hex[pos])) {
            pos++;
        }
        hi = hex_nibble(hex[pos]);
        if (hi < 0) {
            return -1;
        }
        pos++;
        lo = hex_nibble(hex[pos]);
        if (lo < 0) {
            return -1;
        }
        pos++;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static void encode_hex(FILE *f, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        fprintf(f, "%02x", data[i]);
    }
}

static const char *find_key(const char *json, const char *key)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(json, pattern);
}

static int read_int_after_key(const char *json, const char *key, int *out)
{
    const char *p = find_key(json, key);
    if (!p) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    *out = (int)strtol(p, NULL, 10);
    return 0;
}

static int read_quoted_after_key(const char *json, const char *key, char *out, size_t out_len)
{
    const char *p = find_key(json, key);
    const char *start;
    const char *end;
    size_t n;

    if (!p || out_len == 0) {
        return -1;
    }
    p = strchr(p, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    start = p + 1;
    end = strchr(start, '"');
    if (!end) {
        return -1;
    }
    n = (size_t)(end - start);
    if (n >= out_len) {
        return -1;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return 0;
}

static const char *find_hex_field(const char *json, const char *key)
{
    const char *p = find_key(json, key);
    if (!p) {
        return NULL;
    }
    p = strchr(p, ':');
    if (!p) {
        return NULL;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '"') {
        return NULL;
    }
    return p + 1;
}

static const char *next_screen_object(const char *json, const char *cursor, char *buf,
                                      size_t buf_len)
{
    const char *start;
    const char *end;
    size_t depth;
    size_t n;

    if (!cursor) {
        cursor = find_key(json, "screens");
        if (!cursor) {
            return NULL;
        }
        cursor = strchr(cursor, '[');
        if (!cursor) {
            return NULL;
        }
        cursor++;
    }

    while (*cursor && *cursor != '{') {
        if (*cursor == ']') {
            return NULL;
        }
        cursor++;
    }
    if (*cursor != '{') {
        return NULL;
    }

    start = cursor;
    depth = 0;
    for (end = start; *end; end++) {
        if (*end == '{') {
            depth++;
        } else if (*end == '}') {
            depth--;
            if (depth == 0) {
                end++;
                break;
            }
        }
    }
    if (depth != 0) {
        return NULL;
    }

    n = (size_t)(end - start);
    if (n >= buf_len) {
        return NULL;
    }
    memcpy(buf, start, n);
    buf[n] = '\0';
    return end;
}

void retr01_project_init_default(retr01_project_t *proj, const char *palette_v01_path)
{
    retr01_project_screen_t *sc;

    memset(proj, 0, sizeof(*proj));
    proj->format_version = 1;
    snprintf(proj->title, sizeof(proj->title), "Untitled");
    snprintf(proj->master_palette_source, sizeof(proj->master_palette_source),
             "retr01_palette_v_01");
    proj->active_world = 0;
    proj->active_bank = 0;
    proj->active_bg_palette = 0;
    proj->active_chr_tile = 1;
    proj->build_start_world = 0;
    proj->build_start_col = 0;
    proj->build_start_row = 0;
    snprintf(proj->build_output_name, sizeof(proj->build_output_name), "untitled");

    if (palette_v01_path &&
        retr01_palette_load_v01(palette_v01_path, &proj->palette) != 0) {
        retr01_palette_set_defaults(&proj->palette);
    }

    proj->screen_count = 1;
    proj->active_screen = 0;
    sc = &proj->screens[0];
    snprintf(sc->id, sizeof(sc->id), "w0_c0_r0");
    retr01_screen_clear(&sc->screen);

    /* Default solid tile 1 (ci=2) for painting. */
    for (int y = 0; y < 8; y++) {
        proj->chr_banks[0][16 + y] = 0xFF;
    }
    proj->chr_used[0] = 2;
    proj->active_chr_tile = 1;
}

retr01_project_screen_t *retr01_project_active_screen(retr01_project_t *proj)
{
    if (!proj || proj->screen_count <= 0 || proj->active_screen < 0 ||
        proj->active_screen >= proj->screen_count) {
        return NULL;
    }
    return &proj->screens[proj->active_screen];
}

retr01_project_screen_t *retr01_project_ensure_screen(retr01_project_t *proj, int world,
                                                      uint8_t col, uint8_t row)
{
    int i;

    (void)world;
    for (i = 0; i < proj->screen_count; i++) {
        retr01_screen_t *s = &proj->screens[i].screen;
        if (s->col == col && s->row == row) {
            proj->active_screen = i;
            return &proj->screens[i];
        }
    }

    if (proj->screen_count >= RETR01_PROJECT_MAX_SCREENS) {
        return NULL;
    }

    i = proj->screen_count++;
    snprintf(proj->screens[i].id, sizeof(proj->screens[i].id), "w%d_c%d_r%d", world, col, row);
    retr01_screen_clear(&proj->screens[i].screen);
    proj->screens[i].screen.col = col;
    proj->screens[i].screen.row = row;
    proj->active_screen = i;
    return &proj->screens[i];
}

int retr01_project_save(const retr01_project_t *proj, const char *path)
{
    FILE *f;
    int i;
    int b;

    if (!proj || !path) {
        return -1;
    }

    f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"format_version\": %d,\n", proj->format_version);
    fprintf(f, "  \"title\": \"%s\",\n", proj->title);
    fprintf(f, "  \"master_palette_source\": \"%s\",\n", proj->master_palette_source);
    fprintf(f, "  \"active_world\": %d,\n", proj->active_world);
    fprintf(f, "  \"active_bank\": %d,\n", proj->active_bank);
    fprintf(f, "  \"active_bg_palette\": %d,\n", proj->active_bg_palette);
    fprintf(f, "  \"build\": {\n");
    fprintf(f, "    \"start_world\": %d,\n", proj->build_start_world);
    fprintf(f, "    \"start_col\": %d,\n", proj->build_start_col);
    fprintf(f, "    \"start_row\": %d,\n", proj->build_start_row);
    fprintf(f, "    \"output_name\": \"%s\"\n", proj->build_output_name);
    fprintf(f, "  },\n");
    fprintf(f, "  \"screens\": [\n");

    for (i = 0; i < proj->screen_count; i++) {
        const retr01_screen_t *s = &proj->screens[i].screen;
        fprintf(f, "    {\n");
        fprintf(f, "      \"world\": %d,\n", proj->active_world);
        fprintf(f, "      \"col\": %u,\n", s->col);
        fprintf(f, "      \"row\": %u,\n", s->row);
        fprintf(f, "      \"flags\": %u,\n", s->flags);
        fprintf(f, "      \"authored_bank\": %u,\n", s->authored_bank);
        fprintf(f, "      \"tiles_hex\": \"");
        encode_hex(f, s->tiles, sizeof(s->tiles));
        fprintf(f, "\",\n");
        fprintf(f, "      \"attrs_hex\": \"");
        encode_hex(f, s->attrs, sizeof(s->attrs));
        fprintf(f, "\"\n");
        fprintf(f, "    }%s\n", (i + 1 < proj->screen_count) ? "," : "");
    }

    fprintf(f, "  ],\n");
    fprintf(f, "  \"chr_banks\": [\n");
    for (b = 0; b < 4; b++) {
        fprintf(f, "    { \"bank\": %d, \"used\": %d, \"hex\": \"", b, proj->chr_used[b]);
        encode_hex(f, proj->chr_banks[b], RETR01_CHR_BANK_BYTES);
        fprintf(f, "\" }%s\n", (b + 1 < 4) ? "," : "");
    }
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

int retr01_project_load(retr01_project_t *proj, const char *path)
{
    FILE *f;
    long sz;
    char *json = NULL;
    char screen_obj[8192];
    const char *cursor = NULL;
    int screen_idx = 0;
    int bank;

    if (!proj || !path) {
        return -1;
    }

    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz <= 0 || sz > 32 * 1024 * 1024) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    json = (char *)malloc((size_t)sz + 1);
    if (!json) {
        fclose(f);
        return -1;
    }
    if (fread(json, 1, (size_t)sz, f) != (size_t)sz) {
        free(json);
        fclose(f);
        return -1;
    }
    json[sz] = '\0';
    fclose(f);

    retr01_project_init_default(proj, NULL);

    read_int_after_key(json, "format_version", &proj->format_version);
    read_quoted_after_key(json, "title", proj->title, sizeof(proj->title));
    read_quoted_after_key(json, "master_palette_source", proj->master_palette_source,
                          sizeof(proj->master_palette_source));
    read_int_after_key(json, "active_world", &proj->active_world);
    read_int_after_key(json, "active_bank", &proj->active_bank);
    read_int_after_key(json, "active_bg_palette", &proj->active_bg_palette);

    {
        const char *build = find_key(json, "build");
        if (build) {
            read_int_after_key(build, "start_world", &proj->build_start_world);
            read_int_after_key(build, "start_col", &proj->build_start_col);
            read_int_after_key(build, "start_row", &proj->build_start_row);
            read_quoted_after_key(build, "output_name", proj->build_output_name,
                                  sizeof(proj->build_output_name));
        }
    }

    proj->screen_count = 0;
    while ((cursor = next_screen_object(json, cursor, screen_obj, sizeof(screen_obj))) != NULL) {
        retr01_project_screen_t *ps;
        const char *tiles_hex;
        const char *attrs_hex;
        int col = 0;
        int row = 0;

        if (screen_idx >= RETR01_PROJECT_MAX_SCREENS) {
            free(json);
            return -1;
        }

        ps = &proj->screens[screen_idx];
        read_int_after_key(screen_obj, "col", &col);
        read_int_after_key(screen_obj, "row", &row);
        ps->screen.col = (uint8_t)col;
        ps->screen.row = (uint8_t)row;
        read_int_after_key(screen_obj, "flags", (int *)&ps->screen.flags);
        read_int_after_key(screen_obj, "authored_bank", (int *)&ps->screen.authored_bank);
        snprintf(ps->id, sizeof(ps->id), "w%d_c%d_r%d", proj->active_world, col, row);

        tiles_hex = find_hex_field(screen_obj, "tiles_hex");
        attrs_hex = find_hex_field(screen_obj, "attrs_hex");
        if (!tiles_hex || !attrs_hex ||
            decode_hex(tiles_hex, ps->screen.tiles, sizeof(ps->screen.tiles)) != 0 ||
            decode_hex(attrs_hex, ps->screen.attrs, sizeof(ps->screen.attrs)) != 0) {
            free(json);
            return -1;
        }

        screen_idx++;
    }
    proj->screen_count = screen_idx;
    if (proj->screen_count == 0) {
        retr01_project_init_default(proj, NULL);
    }
    proj->active_screen = 0;

    for (bank = 0; bank < 4; bank++) {
        char bank_key[32];
        const char *banks = find_key(json, "chr_banks");
        const char *bank_obj;
        char bank_buf[32];
        int used = 0;

        if (!banks) {
            break;
        }

        snprintf(bank_key, sizeof(bank_key), "\"bank\": %d", bank);
        bank_obj = strstr(banks, bank_key);
        if (!bank_obj) {
            continue;
        }

        snprintf(bank_buf, sizeof(bank_buf), "w%d", bank);
        (void)bank_buf;

        read_int_after_key(bank_obj, "used", &used);
        proj->chr_used[bank] = used;

        {
            const char *hex = find_hex_field(bank_obj, "hex");
            if (hex &&
                decode_hex(hex, proj->chr_banks[bank], RETR01_CHR_BANK_BYTES) != 0) {
                free(json);
                return -1;
            }
        }
    }

    snprintf(proj->path, sizeof(proj->path), "%s", path);
    free(json);
    return 0;
}

int retr01_project_export_retr01(const retr01_project_t *proj, const char *out_path)
{
    retr01_map_build_screen_t map_screens[RETR01_PROJECT_MAX_SCREENS];
    retr01_map_build_world_t world;
    retr01_cart_t cart;
    uint8_t *map_blob = NULL;
    size_t map_len = 0;
    size_t chr_size;
    int i;
    uint8_t max_tile = 0;
    int bank;

    if (!proj || !out_path || proj->screen_count <= 0) {
        return -1;
    }

    bank = proj->active_bank;
    if (bank < 0 || bank > 3) {
        bank = 0;
    }

    for (i = 0; i < proj->screen_count; i++) {
        const uint8_t *tiles = proj->screens[i].screen.tiles;
        size_t t;
        map_screens[i].screen = proj->screens[i].screen;
        map_screens[i].screen.authored_bank = (uint8_t)bank;
        for (t = 0; t < RETR01_SCREEN_TILE_BYTES; t++) {
            if (tiles[t] > max_tile) {
                max_tile = tiles[t];
            }
        }
    }

    chr_size = (size_t)(max_tile + 1) * 16;
    if (chr_size < 16) {
        chr_size = 16;
    }
    if ((size_t)proj->chr_used[bank] * 16 > chr_size) {
        chr_size = (size_t)proj->chr_used[bank] * 16;
    }

    world.desc.grid_w = 1;
    world.desc.grid_h = 1;
    world.desc.empty_off = 0;
    world.screens = map_screens;
    world.screen_count = (size_t)proj->screen_count;

    if (retr01_map_build(&world, 1, &map_blob, &map_len) != 0) {
        return -1;
    }

    retr01_cart_init(&cart);
    cart.prg = (uint8_t *)malloc(4);
    if (!cart.prg) {
        free(map_blob);
        return -1;
    }
    memset(cart.prg, 0xEA, 4);
    cart.prg_size = 4;

    cart.chr = (uint8_t *)malloc(chr_size);
    if (!cart.chr) {
        free(map_blob);
        retr01_cart_free(&cart);
        return -1;
    }
    memset(cart.chr, 0, chr_size);
    memcpy(cart.chr, proj->chr_banks[bank], chr_size > RETR01_CHR_BANK_BYTES ? RETR01_CHR_BANK_BYTES
                                                                              : chr_size);
    cart.chr_size = chr_size;

    cart.map = map_blob;
    cart.map_size = map_len;
    cart.world_count = 1;

    i = retr01_cart_write_file(out_path, &cart);
    retr01_cart_free(&cart);
    return i;
}

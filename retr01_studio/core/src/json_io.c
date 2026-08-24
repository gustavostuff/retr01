#include "retr01_studio/json_io.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + c - 'A';
    }
    return -1;
}

static int decode_hex(const char *hex, uint8_t *out, size_t out_len) {
    size_t i;
    size_t n = strlen(hex);
    if (n != out_len * 2u) {
        return -1;
    }
    for (i = 0; i < out_len; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            return -1;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static char *encode_hex(const uint8_t *in, size_t in_len) {
    static const char *HEX = "0123456789ABCDEF";
    char *out = (char *)malloc(in_len * 2u + 1u);
    size_t i;
    if (!out) {
        return NULL;
    }
    for (i = 0; i < in_len; i++) {
        out[i * 2] = HEX[in[i] >> 4];
        out[i * 2 + 1] = HEX[in[i] & 0x0Fu];
    }
    out[in_len * 2] = '\0';
    return out;
}

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

int r01_project_save_json(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    FILE *f;
    const R01World *w;
    int i;
    if (!p || !path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    w = &p->worlds[0];
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write json");
        return -1;
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": %d,\n", R01_JSON_VER);
    fprintf(f, "  \"name\": \"%s\",\n", p->name);
    fprintf(f, "  \"active_screen\": %d,\n", p->active_screen);
    fprintf(f, "  \"grid_cols\": %d,\n", w->grid_cols);
    fprintf(f, "  \"grid_rows\": %d,\n", w->grid_rows);
    fprintf(f, "  \"screens\": [\n");
    for (i = 0; i < w->screen_count; i++) {
        const R01Screen *s = &w->screens[i];
        char *px = encode_hex(s->pixels, sizeof(s->pixels));
        char *tl = encode_hex(s->tiles, sizeof(s->tiles));
        char *at = encode_hex(s->attrs, sizeof(s->attrs));
        if (!px || !tl || !at) {
            free(px);
            free(tl);
            free(at);
            fclose(f);
            set_err(err_buf, err_cap, "oom");
            return -1;
        }
        fprintf(f, "    {\"col\": %d, \"row\": %d,\n", s->col, s->row);
        fprintf(f, "     \"pixels_hex\": \"%s\",\n", px);
        fprintf(f, "     \"tiles_hex\": \"%s\",\n", tl);
        fprintf(f, "     \"attrs_hex\": \"%s\"}%s\n", at, i + 1 < w->screen_count ? "," : "");
        free(px);
        free(tl);
        free(at);
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"bg_bank0_tiles\": %d\n", w->bg_banks[0].tile_count);
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

static const char *json_find(const char *hay, const char *needle) {
    return hay ? strstr(hay, needle) : NULL;
}

static int json_int_after(const char *p, const char *key, int *out) {
    const char *k = json_find(p, key);
    char *end;
    long v;
    if (!k || !out) {
        return 0;
    }
    k += strlen(key);
    while (*k == ' ' || *k == '\t' || *k == ':' || *k == '\"') {
        k++;
    }
    v = strtol(k, &end, 10);
    if (end == k) {
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int json_string_field(const char *obj, const char *key, char *buf, size_t buf_len) {
    const char *k = json_find(obj, key);
    size_t n = 0;
    if (!k || !buf || buf_len == 0) {
        return 0;
    }
    k += strlen(key);
    while (*k && *k != '\"') {
        k++;
    }
    if (*k != '\"') {
        return 0;
    }
    k++;
    while (*k && *k != '\"' && n + 1 < buf_len) {
        buf[n++] = *k++;
    }
    buf[n] = '\0';
    return n > 0;
}

int r01_project_load_json(R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    FILE *f;
    long sz;
    char *buf = NULL;
    const char *section;
    const char *obj;
    char name[R01_NAME_MAX];
    int active = R01_START_ROW * R01_DEFAULT_GRID + R01_START_COL;
    int grid_cols = R01_DEFAULT_GRID;
    int grid_rows = R01_DEFAULT_GRID;

    if (!p || !path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot open json");
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        set_err(err_buf, err_cap, "read failed");
        return -1;
    }
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf || fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        set_err(err_buf, err_cap, "read failed");
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    r01_project_init(p, "untitled");
    name[0] = '\0';
    json_string_field(buf, "\"name\"", name, sizeof(name));
    if (name[0]) {
        strncpy(p->name, name, R01_NAME_MAX - 1);
    }
    json_int_after(buf, "\"active_screen\"", &active);
    json_int_after(buf, "\"grid_cols\"", &grid_cols);
    json_int_after(buf, "\"grid_rows\"", &grid_rows);
    if (grid_cols >= 1 && grid_cols <= R01_GRID_MAX && grid_rows >= 1 && grid_rows <= R01_GRID_MAX) {
        r01_world_set_grid(r01_project_world0(p), grid_cols, grid_rows);
    }
    if (active >= 0 && active < r01_project_world0(p)->screen_count) {
        p->active_screen = active;
    }

    section = json_find(buf, "\"screens\"");
    if (section) {
        R01World *w = r01_project_world0(p);
        obj = strchr(section, '{');
        while (obj && obj < buf + sz) {
            const char *end = strchr(obj, '}');
            size_t olen;
            char *slice;
            char px[sizeof(((R01Screen *)0)->pixels) * 2 + 4];
            char tl[sizeof(((R01Screen *)0)->tiles) * 2 + 4];
            char at[sizeof(((R01Screen *)0)->attrs) * 2 + 4];
            int col = 0, row = 0, si;
            R01Screen *s;
            if (!end) {
                break;
            }
            olen = (size_t)(end - obj + 1);
            slice = (char *)malloc(olen + 1u);
            if (!slice) {
                break;
            }
            memcpy(slice, obj, olen);
            slice[olen] = '\0';
            json_int_after(slice, "\"col\"", &col);
            json_int_after(slice, "\"row\"", &row);
            si = r01_world_screen_index(w, col, row);
            if (si < 0) {
                free(slice);
                obj = strchr(end + 1, '{');
                continue;
            }
            s = &w->screens[si];
            px[0] = tl[0] = at[0] = '\0';
            if (json_string_field(slice, "\"pixels_hex\"", px, sizeof(px))) {
                decode_hex(px, s->pixels, sizeof(s->pixels));
            }
            if (json_string_field(slice, "\"tiles_hex\"", tl, sizeof(tl))) {
                decode_hex(tl, s->tiles, sizeof(s->tiles));
            }
            if (json_string_field(slice, "\"attrs_hex\"", at, sizeof(at))) {
                decode_hex(at, s->attrs, sizeof(s->attrs));
            }
            free(slice);
            obj = strchr(end + 1, '{');
        }
    }

    free(buf);
    r01_chr_pack_world_bank0(r01_project_world0(p));
    return 0;
}

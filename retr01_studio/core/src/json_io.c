#include "retr01_studio/json_io.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char B64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    if (c == '=') {
        return -2;
    }
    return -1;
}

static char *encode_b64(const uint8_t *in, size_t in_len) {
    size_t out_len = 4u * ((in_len + 2u) / 3u);
    char *out;
    size_t i;
    size_t j = 0;
    if (!in && in_len > 0) {
        return NULL;
    }
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        return NULL;
    }
    for (i = 0; i < in_len; i += 3u) {
        uint32_t v = (uint32_t)in[i] << 16;
        if (i + 1u < in_len) {
            v |= (uint32_t)in[i + 1u] << 8;
        }
        if (i + 2u < in_len) {
            v |= (uint32_t)in[i + 2u];
        }
        out[j++] = B64_TABLE[(v >> 18) & 63u];
        out[j++] = B64_TABLE[(v >> 12) & 63u];
        out[j++] = (i + 1u < in_len) ? B64_TABLE[(v >> 6) & 63u] : '=';
        out[j++] = (i + 2u < in_len) ? B64_TABLE[v & 63u] : '=';
    }
    out[j] = '\0';
    return out;
}

static uint8_t *decode_b64(const char *in, size_t *out_len) {
    size_t o = 0;
    size_t cap;
    uint8_t *out;
    int val = 0;
    int valb = -8;
    size_t i;
    if (!in || !out_len) {
        return NULL;
    }
    cap = strlen(in) * 3u / 4u + 4u;
    out = (uint8_t *)malloc(cap);
    if (!out) {
        return NULL;
    }
    for (i = 0; in[i]; i++) {
        int d = b64_value(in[i]);
        if (d == -1) {
            continue;
        }
        if (d == -2) {
            break;
        }
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out[o++] = (uint8_t)((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    *out_len = o;
    return out;
}

static int decode_hex(const char *hex, uint8_t *out, size_t out_len) {
    size_t i;
    size_t n;
    if (!hex || !out) {
        return -1;
    }
    n = strlen(hex);
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

static uint8_t *decode_hex_alloc(const char *hex, size_t *out_len) {
    size_t n;
    uint8_t *out;
    size_t i;
    if (!hex || !out_len) {
        return NULL;
    }
    n = strlen(hex);
    if (n == 0 || (n & 1u) != 0) {
        return NULL;
    }
    *out_len = n / 2u;
    out = (uint8_t *)malloc(*out_len);
    if (!out) {
        return NULL;
    }
    for (i = 0; i < *out_len; i++) {
        int hi = hex_nibble(hex[i * 2]);
        int lo = hex_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            free(out);
            return NULL;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return out;
}

static int r01_rle_decode(const uint8_t *in, size_t in_len, uint8_t *out, size_t out_len) {
    size_t i = 0;
    size_t o = 0;
    if (!in || !out) {
        return -1;
    }
    while (i < in_len) {
        uint8_t tag = in[i++];
        if (tag & 0x80u) {
            uint8_t val;
            size_t count = tag & 0x7Fu;
            if (i >= in_len || count == 0) {
                return -1;
            }
            val = in[i++];
            if (o + count > out_len) {
                return -1;
            }
            memset(out + o, val, count);
            o += count;
        } else {
            size_t count = tag;
            if (count == 0 || i + count > in_len || o + count > out_len) {
                return -1;
            }
            memcpy(out + o, in + i, count);
            i += count;
            o += count;
        }
    }
    if (o != out_len) {
        return -1;
    }
    return 0;
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
    int wrote = 0;
    if (!p || !path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    w = &p->worlds[0];
    if (r01_chr_pack_world_bank0(w) != R01_CHR_OK) {
        set_err(err_buf, err_cap, "chr pack failed");
        return -1;
    }
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
    fprintf(f, "  \"global_pal_bg\": [");
    {
        int row, pal, j, first = 1;
        for (row = 0; row < R01_PAL_ROWS; row++) {
            for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
                fprintf(f, "%s[", first ? "" : ", ");
                first = 0;
                for (j = 0; j < R01_PAL_COLORS; j++) {
                    fprintf(f, "%s%d", j ? ", " : "", p->global_pal_bg[row][pal].idx[j]);
                }
                fprintf(f, "]");
            }
        }
    }
    fprintf(f, "],\n");
    fprintf(f, "  \"global_pal_spr\": [");
    {
        int row, pal, j, first = 1;
        for (row = 0; row < R01_PAL_ROWS; row++) {
            for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
                fprintf(f, "%s[", first ? "" : ", ");
                first = 0;
                for (j = 0; j < R01_PAL_COLORS; j++) {
                    fprintf(f, "%s%d", j ? ", " : "", p->global_pal_spr[row][pal].idx[j]);
                }
                fprintf(f, "]");
            }
        }
    }
    fprintf(f, "],\n");
    {
        size_t chr_bytes = (size_t)w->bg_banks[0].tile_count * R01_TILE_BYTES;
        char *bank_b64 = encode_b64(w->bg_banks[0].chr, chr_bytes);
        if (!bank_b64) {
            fclose(f);
            set_err(err_buf, err_cap, "oom");
            return -1;
        }
        fprintf(f, "  \"bg_bank0_tiles\": %d,\n", w->bg_banks[0].tile_count);
        fprintf(f, "  \"bg_bank0_b64\": \"%s\",\n", bank_b64);
        free(bank_b64);
    }
    fprintf(f, "  \"screens\": [\n");
    for (i = 0; i < w->screen_count; i++) {
        const R01Screen *s = &w->screens[i];
        char *tl;
        char *at;
        if (!s->present) {
            continue;
        }
        tl = encode_b64(s->tiles, sizeof(s->tiles));
        at = encode_b64(s->attrs, sizeof(s->attrs));
        if (!tl || !at) {
            free(tl);
            free(at);
            fclose(f);
            set_err(err_buf, err_cap, "oom");
            return -1;
        }
        fprintf(f, "%s    {\"col\": %d, \"row\": %d,\n", wrote ? ",\n" : "", s->col, s->row);
        fprintf(f, "     \"tiles_b64\": \"%s\",\n", tl);
        fprintf(f, "     \"attrs_b64\": \"%s\"}", at);
        wrote = 1;
        free(tl);
        free(at);
    }
    fprintf(f, "\n  ]\n");
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

static char *json_string_field_dup(const char *obj, const char *key) {
    const char *k = json_find(obj, key);
    const char *start;
    const char *end;
    size_t n;
    char *out;
    if (!k) {
        return NULL;
    }
    k += strlen(key);
    while (*k && *k != '\"') {
        k++;
    }
    if (*k != '\"') {
        return NULL;
    }
    start = k + 1;
    end = start;
    while (*end && *end != '\"') {
        end++;
    }
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1u);
    if (!out) {
        return NULL;
    }
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

static int load_screen_field(const char *slice, const char *b64_key, const char *rle_key, const char *hex_key,
                             uint8_t *out, size_t out_len) {
    char *text = json_string_field_dup(slice, b64_key);
    if (text) {
        size_t bin_len = 0;
        uint8_t *bin = decode_b64(text, &bin_len);
        int rc = -1;
        free(text);
        if (!bin) {
            return -1;
        }
        if (bin_len == out_len) {
            memcpy(out, bin, out_len);
            rc = 0;
        }
        free(bin);
        return rc;
    }
    text = json_string_field_dup(slice, rle_key);
    if (text) {
        size_t bin_len = 0;
        uint8_t *bin = decode_hex_alloc(text, &bin_len);
        int rc = -1;
        free(text);
        if (!bin) {
            return -1;
        }
        rc = r01_rle_decode(bin, bin_len, out, out_len);
        free(bin);
        return rc;
    }
    text = json_string_field_dup(slice, hex_key);
    if (!text) {
        return 0;
    }
    {
        int rc = decode_hex(text, out, out_len);
        free(text);
        return rc;
    }
}

static int parse_bracket_row(const char *start, uint8_t out[R01_PAL_COLORS]) {
    const char *k = start;
    int i = 0;
    if (!start || *start != '[') {
        return 0;
    }
    k++;
    while (*k && i < R01_PAL_COLORS) {
        char *end;
        long v;
        while (*k == ' ' || *k == '\t' || *k == ',') {
            k++;
        }
        if (*k == ']') {
            break;
        }
        v = strtol(k, &end, 10);
        if (end == k) {
            break;
        }
        out[i++] = (uint8_t)v;
        k = end;
    }
    return i == R01_PAL_COLORS;
}

static int load_palette_plane(const char *buf, const char *key, R01PalRow plane[R01_PAL_ROWS][R01_PALS_PER_ROW]) {
    const char *section = json_find(buf, key);
    const char *row;
    R01PalRow flat[R01_PAL_COUNT];
    int i = 0;
    int n;
    if (!section) {
        return 0;
    }
    row = strchr(section, '[');
    if (row) {
        row = strchr(row + 1, '[');
    }
    while (row && i < R01_PAL_COUNT) {
        if (parse_bracket_row(row, flat[i].idx)) {
            i++;
        }
        row = strchr(row + 1, '[');
    }
    n = i;
    if (n == R01_PAL_COUNT) {
        for (i = 0; i < R01_PAL_COUNT; i++) {
            plane[i / R01_PALS_PER_ROW][i % R01_PALS_PER_ROW] = flat[i];
        }
        return n;
    }
    /* Legacy v3: 4 pals = one row. Copy into row 0; caller keeps other rows from init. */
    if (n == R01_PALS_PER_ROW) {
        for (i = 0; i < R01_PALS_PER_ROW; i++) {
            plane[0][i] = flat[i];
        }
        return n;
    }
    return n;
}

int r01_project_load_json(R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    FILE *f;
    long sz;
    char *buf = NULL;
    const char *section;
    const char *obj;
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
    {
        char *name_str = json_string_field_dup(buf, "\"name\"");
        if (name_str && name_str[0]) {
            strncpy(p->name, name_str, R01_NAME_MAX - 1);
            p->name[R01_NAME_MAX - 1] = '\0';
        }
        free(name_str);
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
    {
        int nbg = load_palette_plane(buf, "\"global_pal_bg\"", p->global_pal_bg);
        int nspr = load_palette_plane(buf, "\"global_pal_spr\"", p->global_pal_spr);
        (void)nbg;
        (void)nspr;
        /* Full plane (32) or legacy row-0 (4) already applied; other rows keep phase1 init. */
    }

    section = json_find(buf, "\"screens\"");
    if (section) {
        R01World *w = r01_project_world0(p);
        obj = strchr(section, '{');
        while (obj && obj < buf + sz) {
            const char *end = strchr(obj, '}');
            size_t olen;
            char *slice;
            int col = 0, row = 0, present = 1, si;
            int has_present = 0;
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
            has_present = json_int_after(slice, "\"present\"", &present);
            si = r01_world_screen_index(w, col, row);
            if (si < 0) {
                free(slice);
                obj = strchr(end + 1, '{');
                continue;
            }
            s = &w->screens[si];
            s->present = 1;
            if (has_present && !present) {
                s->present = 0;
            }
            if (load_screen_field(slice, "\"pixels_b64\"", "\"pixels_rle_hex\"", "\"pixels_hex\"", s->pixels,
                                  sizeof(s->pixels)) != 0) {
                free(slice);
                free(buf);
                set_err(err_buf, err_cap, "screen decode failed");
                return -1;
            }
            if (load_screen_field(slice, "\"tiles_b64\"", "\"tiles_rle_hex\"", "\"tiles_hex\"", s->tiles,
                                  sizeof(s->tiles)) != 0 ||
                load_screen_field(slice, "\"attrs_b64\"", "\"attrs_rle_hex\"", "\"attrs_hex\"", s->attrs,
                                  sizeof(s->attrs)) != 0) {
                free(slice);
                free(buf);
                set_err(err_buf, err_cap, "screen decode failed");
                return -1;
            }
            free(slice);
            obj = strchr(end + 1, '{');
        }
    }

    {
        R01World *w = r01_project_world0(p);
        int bank_tiles = 0;
        int bank_loaded = 0;
        char *bank_b64 = json_string_field_dup(buf, "\"bg_bank0_b64\"");
        json_int_after(buf, "\"bg_bank0_tiles\"", &bank_tiles);
        if (bank_b64 && bank_tiles > 0 && bank_tiles <= R01_TILES_PER_BANK) {
            size_t bin_len = 0;
            size_t expect = (size_t)bank_tiles * R01_TILE_BYTES;
            uint8_t *bin = decode_b64(bank_b64, &bin_len);
            if (bin && bin_len == expect) {
                memset(w->bg_banks[0].chr, 0, R01_BANK_CHR_BYTES);
                memcpy(w->bg_banks[0].chr, bin, expect);
                w->bg_banks[0].tile_count = bank_tiles;
                bank_loaded = 1;
            }
            free(bin);
        }
        free(bank_b64);
        if (bank_loaded) {
            int si;
            for (si = 0; si < w->screen_count; si++) {
                R01Screen *s = &w->screens[si];
                if (s->present) {
                    r01_screen_fill_pixels_from_bank(w, s);
                }
            }
        } else {
            r01_chr_pack_world_bank0(w);
        }
    }

    r01_project_select_start_screen(p);
    free(buf);
    return 0;
}

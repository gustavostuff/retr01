#include "retr01_studio/json_io.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/project.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/sprites.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/metasprites.h"

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

static const char *json_array_end(const char *section_key) {
    const char *p;
    int depth;
    int in_str;
    if (!section_key) {
        return NULL;
    }
    p = strchr(section_key, '[');
    if (!p) {
        return NULL;
    }
    depth = 0;
    in_str = 0;
    for (; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) {
                p++;
                continue;
            }
            if (*p == '\"') {
                in_str = 0;
            }
            continue;
        }
        if (*p == '\"') {
            in_str = 1;
            continue;
        }
        if (*p == '[') {
            depth++;
        } else if (*p == ']') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

/* obj_start must point at '{'. Returns matching '}' or NULL. */
static const char *json_object_end(const char *obj_start) {
    const char *p;
    int depth;
    int in_str;
    if (!obj_start || *obj_start != '{') {
        return NULL;
    }
    depth = 0;
    in_str = 0;
    for (p = obj_start; *p; p++) {
        if (in_str) {
            if (*p == '\\' && p[1]) {
                p++;
                continue;
            }
            if (*p == '\"') {
                in_str = 0;
            }
            continue;
        }
        if (*p == '\"') {
            in_str = 1;
            continue;
        }
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                return p;
            }
        }
    }
    return NULL;
}

static char *json_string_field_dup(const char *obj, const char *key);

static int json_string_after(const char *p, const char *key, char *out, size_t out_cap) {
    char *dup;
    if (!out || out_cap < 1) {
        return 0;
    }
    out[0] = '\0';
    dup = json_string_field_dup(p, key);
    if (!dup) {
        return 0;
    }
    strncpy(out, dup, out_cap - 1u);
    out[out_cap - 1u] = '\0';
    free(dup);
    return 1;
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
    /* Persist world 0 only — load always applies the file into worlds[0]. */
    w = r01_project_world0_const(p);
    if (!w) {
        set_err(err_buf, err_cap, "bad project");
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
    fprintf(f, "  \"default_world\": %d,\n", p->default_world);
    fprintf(f, "  \"active_world\": %d,\n", p->active_world);
    fprintf(f, "  \"active_screen\": %d,\n", p->active_screen);
    fprintf(f, "  \"default_screen\": %d,\n", w->default_screen);
    fprintf(f, "  \"default_pal_row\": %d,\n", w->default_pal_row);
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
    fprintf(f, "  \"spr_banks\": [\n");
    {
        int bi;
        for (bi = 0; bi < R01_SPR_BANKS; bi++) {
            size_t chr_bytes = (size_t)w->spr_banks[bi].tile_count * R01_TILE_BYTES;
            char *bank_b64 = encode_b64(w->spr_banks[bi].chr, chr_bytes);
            if (!bank_b64) {
                fclose(f);
                set_err(err_buf, err_cap, "oom");
                return -1;
            }
            fprintf(f, "    {\"tiles\": %d, \"b64\": \"%s\"}%s\n", w->spr_banks[bi].tile_count, bank_b64,
                    bi + 1 < R01_SPR_BANKS ? "," : "");
            free(bank_b64);
        }
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"sprites\": [\n");
    {
        int si;
        for (si = 0; si < w->sprite_count; si++) {
            const R01SpriteDef *sp = &w->sprites[si];
            fprintf(f, "    {\"bank\": %d, \"tile\": %d, \"pal\": %d}%s\n", sp->bank, sp->tile_id, sp->pal,
                    si + 1 < w->sprite_count ? "," : "");
        }
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"metasprites\": [\n");
    {
        int mi;
        for (mi = 0; mi < w->metasprite_count; mi++) {
            const R01MetaspriteDef *ms = &w->metasprites[mi];
            const R01EntityFrame *fr = &ms->frame;
            int pi;
            fprintf(f, "    {\"name\": \"%s\", \"parts\": [", ms->name);
            for (pi = 0; pi < fr->part_count; pi++) {
                const R01EntityPart *pt = &fr->parts[pi];
                fprintf(f, "%s{\"bank\":%d,\"tile\":%d,\"pal\":%d,\"fh\":%d,\"fv\":%d,\"dx\":%d,\"dy\":%d}",
                        pi ? "," : "", pt->bank, pt->tile_id, pt->pal, pt->flip_h, pt->flip_v, pt->dx, pt->dy);
            }
            fprintf(f, "]}%s\n", mi + 1 < w->metasprite_count ? "," : "");
        }
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"entities\": [\n");
    {
        int ei;
        for (ei = 0; ei < w->entity_count; ei++) {
            const R01EntityType *ent = &w->entities[ei];
            int si;
            fprintf(f, "    {\n");
            fprintf(f, "      \"state_count\": %d,\n", ent->state_count);
            fprintf(f, "      \"states\": [\n");
            for (si = 0; si < ent->state_count; si++) {
                const R01EntityState *st = &ent->states[si];
                int fi;
                fprintf(f, "        {\n");
                fprintf(f, "          \"name\": \"%s\",\n", st->name);
                fprintf(f, "          \"origin_x\": %d, \"origin_y\": %d,\n", st->origin_x, st->origin_y);
                fprintf(f, "          \"hitbox_x\": %d, \"hitbox_y\": %d, \"hitbox_w\": %d, \"hitbox_h\": %d,\n",
                        st->hitbox_x, st->hitbox_y, st->hitbox_w, st->hitbox_h);
                fprintf(f, "          \"frame_count\": %d,\n", st->frame_count);
                fprintf(f, "          \"frames\": [\n");
                for (fi = 0; fi < st->frame_count; fi++) {
                    const R01EntityFrame *fr = &st->frames[fi];
                    int pi;
                    fprintf(f, "            {\"parts\": [");
                    for (pi = 0; pi < fr->part_count; pi++) {
                        const R01EntityPart *pt = &fr->parts[pi];
                        fprintf(f,
                                "%s{\"bank\":%d,\"tile\":%d,\"pal\":%d,\"fh\":%d,\"fv\":%d,\"dx\":%d,\"dy\":%d}",
                                pi ? "," : "", pt->bank, pt->tile_id, pt->pal, pt->flip_h, pt->flip_v, pt->dx,
                                pt->dy);
                    }
                    fprintf(f, "]}%s\n", fi + 1 < st->frame_count ? "," : "");
                }
                fprintf(f, "          ]\n");
                fprintf(f, "        }%s\n", si + 1 < ent->state_count ? "," : "");
            }
            fprintf(f, "      ]\n");
            fprintf(f, "    }%s\n", ei + 1 < w->entity_count ? "," : "");
        }
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"instances\": [\n");
    {
        int ii;
        for (ii = 0; ii < w->instance_count; ii++) {
            const R01EntityInstance *inst = &w->instances[ii];
            fprintf(f, "    {\"type\": %d, \"x\": %d, \"y\": %d}%s\n", inst->type_id, inst->world_x,
                    inst->world_y, ii + 1 < w->instance_count ? "," : "");
        }
    }
    fprintf(f, "  ],\n");
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
    int default_world = 0;
    int active_world = 0;
    int default_screen = -1;
    int default_pal_row = 0;
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
    json_int_after(buf, "\"default_world\"", &default_world);
    json_int_after(buf, "\"active_world\"", &active_world);
    json_int_after(buf, "\"active_screen\"", &active);
    json_int_after(buf, "\"default_screen\"", &default_screen);
    json_int_after(buf, "\"default_pal_row\"", &default_pal_row);
    json_int_after(buf, "\"grid_cols\"", &grid_cols);
    json_int_after(buf, "\"grid_rows\"", &grid_rows);
    if (grid_cols >= 1 && grid_cols <= R01_GRID_MAX && grid_rows >= 1 && grid_rows <= R01_GRID_MAX) {
        r01_world_set_grid(r01_project_world0(p), grid_cols, grid_rows);
    }
    if (default_world >= 0 && default_world < R01_MAX_WORLDS) {
        p->default_world = default_world;
    }
    if (active_world >= 0 && active_world < R01_MAX_WORLDS) {
        p->active_world = active_world;
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
    {
        int tilemaps_loaded = 0;
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
                if (json_find(slice, "\"tiles_b64\"")) {
                    tilemaps_loaded = 1;
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
            } else if (!tilemaps_loaded) {
                r01_chr_pack_world_bank0(w);
            }
        }

        /* v5+: SPR banks + sprite catalog; v6 adds metasprites (optional; older projects stay empty). */
        {
            R01World *w = r01_project_world0(p);
            const char *spr_section = json_find(buf, "\"spr_banks\":");
            const char *spr_end = json_array_end(spr_section);
            int bi = 0;
            w->sprite_count = 0;
            w->metasprite_count = 0;
            for (bi = 0; bi < R01_SPR_BANKS; bi++) {
                memset(w->spr_banks[bi].chr, 0, R01_BANK_CHR_BYTES);
                w->spr_banks[bi].tile_count = 0;
            }
            if (spr_section && spr_end) {
                const char *obj2 = strchr(spr_section, '{');
                bi = 0;
                while (obj2 && obj2 < spr_end && bi < R01_SPR_BANKS) {
                    const char *end = strchr(obj2, '}');
                    size_t olen;
                    char *slice;
                    int tiles = 0;
                    char *b64;
                    if (!end || end >= spr_end) {
                        break;
                    }
                    olen = (size_t)(end - obj2 + 1);
                    slice = (char *)malloc(olen + 1u);
                    if (!slice) {
                        break;
                    }
                    memcpy(slice, obj2, olen);
                    slice[olen] = '\0';
                    json_int_after(slice, "\"tiles\"", &tiles);
                    b64 = json_string_field_dup(slice, "\"b64\"");
                    if (b64 && tiles > 0 && tiles <= R01_TILES_PER_BANK) {
                        size_t bin_len = 0;
                        size_t expect = (size_t)tiles * R01_TILE_BYTES;
                        uint8_t *bin = decode_b64(b64, &bin_len);
                        if (bin && bin_len == expect) {
                            memcpy(w->spr_banks[bi].chr, bin, expect);
                            w->spr_banks[bi].tile_count = tiles;
                        }
                        free(bin);
                    }
                    free(b64);
                    free(slice);
                    bi++;
                    obj2 = strchr(end + 1, '{');
                }
            }
            {
                const char *cat = json_find(buf, "\"sprites\":");
                const char *cat_end = json_array_end(cat);
                if (cat && cat_end) {
                    const char *obj2 = strchr(cat, '{');
                    while (obj2 && obj2 < cat_end && w->sprite_count < R01_MAX_SPRITES) {
                        const char *end = strchr(obj2, '}');
                        size_t olen;
                        char *slice;
                        int bank = 0, tile = 0, pal = 0;
                        if (!end || end >= cat_end) {
                            break;
                        }
                        olen = (size_t)(end - obj2 + 1);
                        slice = (char *)malloc(olen + 1u);
                        if (!slice) {
                            break;
                        }
                        memcpy(slice, obj2, olen);
                        slice[olen] = '\0';
                        json_int_after(slice, "\"bank\"", &bank);
                        json_int_after(slice, "\"tile\"", &tile);
                        json_int_after(slice, "\"pal\"", &pal);
                        free(slice);
                        if (r01_world_sprite_add(w, bank, tile, pal) < 0) {
                            break;
                        }
                        obj2 = strchr(end + 1, '{');
                    }
                }
            }
            {
                const char *meta_sec = json_find(buf, "\"metasprites\":");
                const char *meta_end = json_array_end(meta_sec);
                if (meta_sec && meta_end) {
                    const char *obj2 = strchr(meta_sec, '{');
                    while (obj2 && obj2 < meta_end && w->metasprite_count < R01_MAX_METASPRITES) {
                        const char *end = json_object_end(obj2);
                        size_t olen;
                        char *slice;
                        char *name_str;
                        const char *parts_sec;
                        const char *parts_end;
                        const char *pt_obj;
                        int midx;
                        R01MetaspriteDef *ms;
                        if (!end || end >= meta_end) {
                            break;
                        }
                        olen = (size_t)(end - obj2 + 1);
                        slice = (char *)malloc(olen + 1u);
                        if (!slice) {
                            break;
                        }
                        memcpy(slice, obj2, olen);
                        slice[olen] = '\0';
                        midx = r01_world_metasprite_add(w);
                        if (midx < 0) {
                            free(slice);
                            break;
                        }
                        ms = &w->metasprites[midx];
                        name_str = json_string_field_dup(slice, "\"name\"");
                        if (name_str && name_str[0]) {
                            strncpy(ms->name, name_str, R01_ENTITY_NAME_MAX - 1);
                        }
                        free(name_str);
                        parts_sec = json_find(slice, "\"parts\"");
                        parts_end = json_array_end(parts_sec);
                        if (parts_sec && parts_end) {
                            pt_obj = strchr(parts_sec, '{');
                            while (pt_obj && pt_obj < parts_end &&
                                   ms->frame.part_count < R01_ENTITY_PARTS_MAX) {
                                const char *pt_end = strchr(pt_obj, '}');
                                size_t pt_len;
                                char *pt_slice;
                                R01EntityPart part;
                                if (!pt_end || pt_end >= parts_end) {
                                    break;
                                }
                                pt_len = (size_t)(pt_end - pt_obj + 1);
                                pt_slice = (char *)malloc(pt_len + 1u);
                                if (!pt_slice) {
                                    break;
                                }
                                memcpy(pt_slice, pt_obj, pt_len);
                                pt_slice[pt_len] = '\0';
                                memset(&part, 0, sizeof(part));
                                json_int_after(pt_slice, "\"bank\"", &part.bank);
                                json_int_after(pt_slice, "\"tile\"", &part.tile_id);
                                json_int_after(pt_slice, "\"pal\"", &part.pal);
                                json_int_after(pt_slice, "\"fh\"", &part.flip_h);
                                json_int_after(pt_slice, "\"fv\"", &part.flip_v);
                                json_int_after(pt_slice, "\"dx\"", &part.dx);
                                json_int_after(pt_slice, "\"dy\"", &part.dy);
                                free(pt_slice);
                                (void)r01_metasprite_add_part(ms, &part);
                                pt_obj = strchr(pt_end + 1, '{');
                            }
                        }
                        free(slice);
                        obj2 = strchr(end + 1, '{');
                    }
                }
            }
            {
                const char *ent_sec = json_find(buf, "\"entities\":");
                const char *ent_end = json_array_end(ent_sec);
                w->entity_count = 0;
                memset(w->entities, 0, sizeof(w->entities));
                if (ent_sec && ent_end) {
                    const char *obj2 = strchr(ent_sec, '{');
                    while (obj2 && obj2 < ent_end && w->entity_count < R01_MAX_ENTITY_TYPES) {
                        const char *end = json_object_end(obj2);
                        size_t olen;
                        char *slice;
                        int idx;
                        int state_count = 1;
                        const char *states_sec;
                        const char *states_end;
                        const char *st_obj;
                        int si = 0;
                        R01EntityType *ent;
                        if (!end || end >= ent_end) {
                            break;
                        }
                        olen = (size_t)(end - obj2 + 1);
                        slice = (char *)malloc(olen + 1u);
                        if (!slice) {
                            break;
                        }
                        memcpy(slice, obj2, olen);
                        slice[olen] = '\0';
                        idx = r01_world_entity_add(w);
                        if (idx < 0) {
                            free(slice);
                            break;
                        }
                        ent = &w->entities[idx];
                        json_int_after(slice, "\"state_count\"", &state_count);
                        if (state_count < 1) {
                            state_count = 1;
                        }
                        if (state_count > R01_ENTITY_STATES_MAX) {
                            state_count = R01_ENTITY_STATES_MAX;
                        }
                        ent->state_count = state_count;
                        states_sec = json_find(slice, "\"states\":");
                        states_end = json_array_end(states_sec);
                        st_obj = states_sec ? strchr(states_sec, '{') : NULL;
                        while (st_obj && states_end && st_obj < states_end && si < state_count) {
                            const char *st_end = json_object_end(st_obj);
                            size_t st_olen;
                            char *st_slice;
                            R01EntityState *st = &ent->states[si];
                            int frame_count = 1;
                            const char *frames_sec;
                            const char *frames_end;
                            const char *fr_obj;
                            int fi = 0;
                            if (!st_end || st_end >= states_end) {
                                break;
                            }
                            st_olen = (size_t)(st_end - st_obj + 1);
                            st_slice = (char *)malloc(st_olen + 1u);
                            if (!st_slice) {
                                break;
                            }
                            memcpy(st_slice, st_obj, st_olen);
                            st_slice[st_olen] = '\0';
                            r01_entity_state_init(st, "Idle");
                            json_string_after(st_slice, "\"name\"", st->name, sizeof(st->name));
                            json_int_after(st_slice, "\"origin_x\"", &st->origin_x);
                            json_int_after(st_slice, "\"origin_y\"", &st->origin_y);
                            json_int_after(st_slice, "\"hitbox_x\"", &st->hitbox_x);
                            json_int_after(st_slice, "\"hitbox_y\"", &st->hitbox_y);
                            json_int_after(st_slice, "\"hitbox_w\"", &st->hitbox_w);
                            json_int_after(st_slice, "\"hitbox_h\"", &st->hitbox_h);
                            json_int_after(st_slice, "\"frame_count\"", &frame_count);
                            if (frame_count < 1) {
                                frame_count = 1;
                            }
                            if (frame_count > R01_ENTITY_FRAMES_MAX) {
                                frame_count = R01_ENTITY_FRAMES_MAX;
                            }
                            st->frame_count = frame_count;
                            if (st->hitbox_w < 1) {
                                st->hitbox_w = R01_ENTITY_HITBOX_W;
                            }
                            if (st->hitbox_h < 1) {
                                st->hitbox_h = R01_ENTITY_HITBOX_H;
                            }
                            frames_sec = json_find(st_slice, "\"frames\":");
                            frames_end = json_array_end(frames_sec);
                            fr_obj = frames_sec ? strchr(frames_sec, '{') : NULL;
                            while (fr_obj && frames_end && fr_obj < frames_end && fi < frame_count) {
                                const char *fr_end = json_object_end(fr_obj);
                                size_t fr_olen;
                                char *fr_slice;
                                const char *parts_sec;
                                const char *parts_end;
                                const char *pt_obj;
                                R01EntityFrame *fr = &st->frames[fi];
                                if (!fr_end || fr_end >= frames_end) {
                                    break;
                                }
                                fr_olen = (size_t)(fr_end - fr_obj + 1);
                                fr_slice = (char *)malloc(fr_olen + 1u);
                                if (!fr_slice) {
                                    break;
                                }
                                memcpy(fr_slice, fr_obj, fr_olen);
                                fr_slice[fr_olen] = '\0';
                                memset(fr, 0, sizeof(*fr));
                                parts_sec = json_find(fr_slice, "\"parts\":");
                                parts_end = json_array_end(parts_sec);
                                pt_obj = parts_sec ? strchr(parts_sec, '{') : NULL;
                                while (pt_obj && parts_end && pt_obj < parts_end &&
                                       fr->part_count < R01_ENTITY_PARTS_MAX) {
                                    const char *pt_end = json_object_end(pt_obj);
                                    size_t pt_olen;
                                    char *pt_slice;
                                    R01EntityPart part;
                                    if (!pt_end || pt_end >= parts_end) {
                                        break;
                                    }
                                    pt_olen = (size_t)(pt_end - pt_obj + 1);
                                    pt_slice = (char *)malloc(pt_olen + 1u);
                                    if (!pt_slice) {
                                        break;
                                    }
                                    memcpy(pt_slice, pt_obj, pt_olen);
                                    pt_slice[pt_olen] = '\0';
                                    memset(&part, 0, sizeof(part));
                                    json_int_after(pt_slice, "\"bank\"", &part.bank);
                                    json_int_after(pt_slice, "\"tile\"", &part.tile_id);
                                    json_int_after(pt_slice, "\"pal\"", &part.pal);
                                    json_int_after(pt_slice, "\"fh\"", &part.flip_h);
                                    json_int_after(pt_slice, "\"fv\"", &part.flip_v);
                                    json_int_after(pt_slice, "\"dx\"", &part.dx);
                                    json_int_after(pt_slice, "\"dy\"", &part.dy);
                                    free(pt_slice);
                                    (void)r01_entity_frame_add_part(fr, &part);
                                    pt_obj = strchr(pt_end + 1, '{');
                                }
                                free(fr_slice);
                                fi++;
                                fr_obj = strchr(fr_end + 1, '{');
                            }
                            free(st_slice);
                            si++;
                            st_obj = strchr(st_end + 1, '{');
                        }
                        free(slice);
                        obj2 = strchr(end + 1, '{');
                    }
                }
            }
            {
                const char *inst_sec = json_find(buf, "\"instances\":");
                const char *inst_end = json_array_end(inst_sec);
                w->instance_count = 0;
                memset(w->instances, 0, sizeof(w->instances));
                if (inst_sec && inst_end) {
                    const char *obj2 = strchr(inst_sec, '{');
                    while (obj2 && obj2 < inst_end && w->instance_count < R01_MAX_ENTITY_INSTANCES) {
                        const char *end = json_object_end(obj2);
                        size_t olen;
                        char *slice;
                        int type_id = 0, wx = 0, wy = 0;
                        if (!end || end >= inst_end) {
                            break;
                        }
                        olen = (size_t)(end - obj2 + 1);
                        slice = (char *)malloc(olen + 1u);
                        if (!slice) {
                            break;
                        }
                        memcpy(slice, obj2, olen);
                        slice[olen] = '\0';
                        json_int_after(slice, "\"type\"", &type_id);
                        json_int_after(slice, "\"x\"", &wx);
                        json_int_after(slice, "\"y\"", &wy);
                        free(slice);
                        if (type_id >= 0 && type_id < w->entity_count) {
                            (void)r01_world_instance_add(w, type_id, wx, wy);
                        }
                        obj2 = strchr(end + 1, '{');
                    }
                }
            }
        }
    }

    {
        R01World *w0 = r01_project_world0(p);
        if (default_screen >= 0 && default_screen < w0->screen_count && w0->screens[default_screen].present) {
            w0->default_screen = default_screen;
        } else {
            r01_world_sync_default_screen(w0);
        }
        if (default_pal_row >= 0 && default_pal_row < R01_PAL_ROWS) {
            w0->default_pal_row = default_pal_row;
        }
    }

    if (active >= 0 && active < r01_project_world0(p)->screen_count &&
        r01_project_world0(p)->screens[active].present) {
        p->active_screen = active;
    } else {
        r01_project_select_start_screen(p);
    }
    /* File data lives in world 0; show that world after load. */
    p->active_world = 0;
    free(buf);
    return 0;
}

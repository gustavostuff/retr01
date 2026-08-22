#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

static int hex_nibble(char c) {
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

static int write_hex(FILE *f, const uint8_t *data, size_t n) {
    static const char *H = "0123456789abcdef";
    size_t i;
    for (i = 0; i < n; i++) {
        if (fputc(H[(data[i] >> 4) & 0xF], f) == EOF) {
            return -1;
        }
        if (fputc(H[data[i] & 0xF], f) == EOF) {
            return -1;
        }
    }
    return 0;
}

static int parse_hex(const char *hex, uint8_t *out, size_t out_len) {
    size_t i;
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) {
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

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    long sz;
    char *buf;
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sz] = 0;
    fclose(f);
    if (out_len) {
        *out_len = (size_t)sz;
    }
    return buf;
}

/* Find "key": and return pointer after colon whitespace. */
static const char *find_key(const char *json, const char *key) {
    char pattern[128];
    const char *p;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p) {
        return NULL;
    }
    p += strlen(pattern);
    while (*p && (isspace((unsigned char)*p) || *p == ':')) {
        p++;
    }
    return p;
}

static int parse_int_after(const char *p, int *out) {
    char *end;
    long v;
    if (!p) {
        return -1;
    }
    v = strtol(p, &end, 10);
    if (end == p) {
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_string_after(const char *p, char *out, size_t out_cap) {
    size_t n = 0;
    if (!p || *p != '"') {
        return -1;
    }
    p++;
    while (*p && *p != '"' && n + 1 < out_cap) {
        if (*p == '\\' && p[1]) {
            p++;
        }
        out[n++] = *p++;
    }
    if (*p != '"') {
        return -1;
    }
    out[n] = 0;
    return 0;
}

/* Parse [[a,b,c,d],...] into 4 R01PalRow. Starts at first '[' of outer array. */
static int parse_pal_rows(const char *p, R01PalRow rows[R01_PAL_ROWS]) {
    int ri, ci;
    if (!p || *p != '[') {
        return -1;
    }
    p++;
    for (ri = 0; ri < R01_PAL_ROWS; ri++) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (*p != '[') {
            return -1;
        }
        p++;
        for (ci = 0; ci < R01_PAL_COLORS; ci++) {
            char *end;
            long v;
            while (*p && isspace((unsigned char)*p)) {
                p++;
            }
            v = strtol(p, &end, 10);
            if (end == p) {
                return -1;
            }
            rows[ri].idx[ci] = (uint8_t)((int)v & 63);
            p = end;
            while (*p && (isspace((unsigned char)*p) || *p == ',')) {
                p++;
            }
        }
        if (*p != ']') {
            return -1;
        }
        p++;
        while (*p && (isspace((unsigned char)*p) || *p == ',')) {
            p++;
        }
    }
    return 0;
}

int r01_project_save_json(const R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    FILE *f;
    int wi, i, c;
    if (!p || !path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    f = fopen(path, "wb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot open for write");
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"format\": \"retr01_studio_project\",\n");
    fprintf(f, "  \"version\": 2,\n");
    fprintf(f, "  \"name\": \"%s\",\n", p->name);
    fprintf(f, "  \"active_world\": %d,\n", p->active_world);
    fprintf(f, "  \"active_screen\": %d,\n", p->active_screen);
    fprintf(f, "  \"generate_bank\": %d,\n", p->generate_bank);
    fprintf(f, "  \"paint_color\": %d,\n", p->paint_color);

    fprintf(f, "  \"global_pal_bg\": [");
    for (i = 0; i < R01_PAL_ROWS; i++) {
        fprintf(f, "[");
        for (c = 0; c < R01_PAL_COLORS; c++) {
            fprintf(f, "%d%s", p->global_pal_bg[i].idx[c], c + 1 < R01_PAL_COLORS ? "," : "");
        }
        fprintf(f, "]%s", i + 1 < R01_PAL_ROWS ? "," : "");
    }
    fprintf(f, "],\n");
    fprintf(f, "  \"global_pal_spr\": [");
    for (i = 0; i < R01_PAL_ROWS; i++) {
        fprintf(f, "[");
        for (c = 0; c < R01_PAL_COLORS; c++) {
            fprintf(f, "%d%s", p->global_pal_spr[i].idx[c], c + 1 < R01_PAL_COLORS ? "," : "");
        }
        fprintf(f, "]%s", i + 1 < R01_PAL_ROWS ? "," : "");
    }
    fprintf(f, "],\n");

    fprintf(f, "  \"worlds\": [\n");

    for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
        const R01World *w = &p->worlds[wi];
        int si, bi;
        fprintf(f, "    {\n");
        fprintf(f, "      \"present\": %d,\n", w->present ? 1 : 0);
        fprintf(f, "      \"default_bg_bank\": %d,\n", w->default_bg_bank);
        fprintf(f, "      \"default_pal_row\": %d,\n", w->default_pal_row);
        fprintf(f, "      \"use_world_pals\": %d,\n", w->use_world_pals ? 1 : 0);
        fprintf(f, "      \"pal_bg\": [");
        for (i = 0; i < R01_PAL_ROWS; i++) {
            fprintf(f, "[");
            for (c = 0; c < R01_PAL_COLORS; c++) {
                fprintf(f, "%d%s", w->pal_bg[i].idx[c], c + 1 < R01_PAL_COLORS ? "," : "");
            }
            fprintf(f, "]%s", i + 1 < R01_PAL_ROWS ? "," : "");
        }
        fprintf(f, "],\n");
        fprintf(f, "      \"pal_spr\": [");
        for (i = 0; i < R01_PAL_ROWS; i++) {
            fprintf(f, "[");
            for (c = 0; c < R01_PAL_COLORS; c++) {
                fprintf(f, "%d%s", w->pal_spr[i].idx[c], c + 1 < R01_PAL_COLORS ? "," : "");
            }
            fprintf(f, "]%s", i + 1 < R01_PAL_ROWS ? "," : "");
        }
        fprintf(f, "],\n");
        fprintf(f, "      \"screens\": [\n");
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            fprintf(f, "        {\n");
            fprintf(f, "          \"col\": %d,\n", s->col);
            fprintf(f, "          \"row\": %d,\n", s->row);
            fprintf(f, "          \"present\": %d,\n", s->present ? 1 : 0);
            fprintf(f, "          \"parallax\": %d,\n", s->parallax ? 1 : 0);
            fprintf(f, "          \"pixels_hex\": \"");
            write_hex(f, s->pixels, sizeof(s->pixels));
            fprintf(f, "\",\n");
            fprintf(f, "          \"tiles_hex\": \"");
            write_hex(f, s->tiles, sizeof(s->tiles));
            fprintf(f, "\",\n");
            fprintf(f, "          \"attrs_hex\": \"");
            write_hex(f, s->attrs, sizeof(s->attrs));
            fprintf(f, "\"\n");
            fprintf(f, "        }%s\n", si + 1 < w->screen_count ? "," : "");
        }
        fprintf(f, "      ],\n");
        fprintf(f, "      \"bg_banks\": [\n");
        for (bi = 0; bi < R01_BG_BANKS; bi++) {
            const R01BgBank *b = &w->bg_banks[bi];
            size_t chr_n = (size_t)b->tile_count * R01_TILE_BYTES;
            fprintf(f, "        {\n");
            fprintf(f, "          \"tile_count\": %d,\n", b->tile_count);
            fprintf(f, "          \"chr_hex\": \"");
            write_hex(f, b->chr, chr_n);
            fprintf(f, "\"\n");
            fprintf(f, "        }%s\n", bi + 1 < R01_BG_BANKS ? "," : "");
        }
        fprintf(f, "      ]\n");
        fprintf(f, "    }%s\n", wi + 1 < R01_MAX_WORLDS ? "," : "");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

/*
 * Phase-1 loader: not a full JSON parser. Expects files written by
 * r01_project_save_json (key order flexible within objects via sequential search
 * from each world/screen block).
 */
int r01_project_load_json(R01Project *p, const char *path, char *err_buf, size_t err_cap) {
    char *json;
    size_t len = 0;
    const char *pcur;
    int version = 0;
    int wi;

    if (!p || !path) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }

    json = read_file(path, &len);
    if (!json) {
        set_err(err_buf, err_cap, "cannot read file");
        return -1;
    }

    if (!strstr(json, "\"retr01_studio_project\"")) {
        free(json);
        set_err(err_buf, err_cap, "not a retr01_studio_project");
        return -1;
    }

    r01_project_init(p, "loaded");
    pcur = find_key(json, "version");
    if (parse_int_after(pcur, &version) != 0 || version < 1 || version > 2) {
        free(json);
        set_err(err_buf, err_cap, "unsupported version");
        return -1;
    }

    pcur = find_key(json, "name");
    if (pcur) {
        char name[R01_NAME_MAX];
        if (parse_string_after(pcur, name, sizeof(name)) == 0) {
            strncpy(p->name, name, R01_NAME_MAX - 1);
        }
    }
    pcur = find_key(json, "active_world");
    parse_int_after(pcur, &p->active_world);
    pcur = find_key(json, "active_screen");
    parse_int_after(pcur, &p->active_screen);
    pcur = find_key(json, "generate_bank");
    parse_int_after(pcur, &p->generate_bank);
    pcur = find_key(json, "paint_color");
    parse_int_after(pcur, &p->paint_color);

    pcur = find_key(json, "global_pal_bg");
    if (pcur) {
        parse_pal_rows(pcur, p->global_pal_bg);
    }
    pcur = find_key(json, "global_pal_spr");
    if (pcur) {
        parse_pal_rows(pcur, p->global_pal_spr);
    }

    /* Split worlds by walking "\"present\":" inside worlds array — brittle but ok for our writer. */
    {
        const char *worlds = strstr(json, "\"worlds\"");
        const char *cursor;
        if (!worlds) {
            free(json);
            set_err(err_buf, err_cap, "missing worlds");
            return -1;
        }
        cursor = strchr(worlds, '[');
        if (!cursor) {
            free(json);
            set_err(err_buf, err_cap, "bad worlds");
            return -1;
        }
        cursor++;

        for (wi = 0; wi < R01_MAX_WORLDS; wi++) {
            R01World *w = &p->worlds[wi];
            const char *wobj = strchr(cursor, '{');
            const char *wend;
            const char *screens;
            const char *scur;
            int si, bi;
            char *wblock;
            size_t wlen;

            if (!wobj) {
                break;
            }
            wend = wobj + 1;
            {
                int depth = 1;
                while (*wend && depth > 0) {
                    if (*wend == '{') {
                        depth++;
                    } else if (*wend == '}') {
                        depth--;
                    }
                    wend++;
                }
            }
            wlen = (size_t)(wend - wobj);
            wblock = (char *)malloc(wlen + 1);
            if (!wblock) {
                free(json);
                set_err(err_buf, err_cap, "oom");
                return -1;
            }
            memcpy(wblock, wobj, wlen);
            wblock[wlen] = 0;

            memset(w, 0, sizeof(*w));
            pcur = find_key(wblock, "present");
            {
                int pr = 0;
                parse_int_after(pcur, &pr);
                w->present = pr ? 1 : 0;
            }
            pcur = find_key(wblock, "default_bg_bank");
            parse_int_after(pcur, &w->default_bg_bank);
            pcur = find_key(wblock, "default_pal_row");
            parse_int_after(pcur, &w->default_pal_row);
            pcur = find_key(wblock, "use_world_pals");
            {
                int u = 0;
                parse_int_after(pcur, &u);
                w->use_world_pals = u ? 1 : 0;
            }
            pcur = find_key(wblock, "pal_bg");
            if (pcur) {
                parse_pal_rows(pcur, w->pal_bg);
            } else {
                int i;
                for (i = 0; i < R01_PAL_ROWS; i++) {
                    w->pal_bg[i] = p->global_pal_bg[i];
                }
            }
            pcur = find_key(wblock, "pal_spr");
            if (pcur) {
                parse_pal_rows(pcur, w->pal_spr);
            } else {
                int i;
                for (i = 0; i < R01_PAL_ROWS; i++) {
                    w->pal_spr[i] = p->global_pal_spr[i];
                }
            }

            screens = strstr(wblock, "\"screens\"");
            if (screens) {
                const char *s_arr = strchr(screens, '[');
                const char *s_end = NULL;
                if (s_arr) {
                    int depth = 0;
                    const char *q = s_arr;
                    for (; *q; q++) {
                        if (*q == '[') {
                            depth++;
                        } else if (*q == ']') {
                            depth--;
                            if (depth == 0) {
                                s_end = q;
                                break;
                            }
                        }
                    }
                }
                scur = s_arr ? s_arr + 1 : NULL;
                if (scur && s_end) {
                    for (si = 0; si < R01_MAX_SCREENS_PER_WORLD; si++) {
                        const char *sobj = NULL;
                        const char *scan;
                        const char *send;
                        char *sblock;
                        size_t slen;
                        R01Screen *s;
                        char *hex;
                        size_t hexcap = R01_SCREEN_PX_W * R01_SCREEN_PX_H * 2 + 8;

                        for (scan = scur; scan < s_end; scan++) {
                            if (*scan == '{') {
                                sobj = scan;
                                break;
                            }
                        }
                        if (!sobj) {
                            break;
                        }
                        send = sobj + 1;
                        {
                            int depth = 1;
                            while (send < s_end && depth > 0) {
                                if (*send == '{') {
                                    depth++;
                                } else if (*send == '}') {
                                    depth--;
                                }
                                send++;
                            }
                        }
                        slen = (size_t)(send - sobj);
                        sblock = (char *)malloc(slen + 1);
                        hex = (char *)malloc(hexcap);
                        if (!sblock || !hex) {
                            free(sblock);
                            free(hex);
                            free(wblock);
                            free(json);
                            set_err(err_buf, err_cap, "oom");
                            return -1;
                        }
                        memcpy(sblock, sobj, slen);
                        sblock[slen] = 0;

                        s = &w->screens[w->screen_count];
                        memset(s, 0, sizeof(*s));
                        pcur = find_key(sblock, "col");
                        parse_int_after(pcur, &s->col);
                        pcur = find_key(sblock, "row");
                        parse_int_after(pcur, &s->row);
                        pcur = find_key(sblock, "present");
                        {
                            int pr = 1;
                            parse_int_after(pcur, &pr);
                            s->present = pr ? 1 : 0;
                        }
                        pcur = find_key(sblock, "parallax");
                        {
                            int prx = 0;
                            parse_int_after(pcur, &prx);
                            s->parallax = prx ? 1 : 0;
                        }
                        pcur = find_key(sblock, "pixels_hex");
                        if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                            if (parse_hex(hex, s->pixels, sizeof(s->pixels)) != 0) {
                                free(hex);
                                free(sblock);
                                free(wblock);
                                free(json);
                                set_err(err_buf, err_cap, "bad pixels_hex");
                                return -1;
                            }
                        }
                        pcur = find_key(sblock, "tiles_hex");
                        if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                            parse_hex(hex, s->tiles, sizeof(s->tiles));
                        }
                        pcur = find_key(sblock, "attrs_hex");
                        if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                            parse_hex(hex, s->attrs, sizeof(s->attrs));
                        }

                        w->screen_count++;
                        free(sblock);
                        free(hex);
                        scur = send;
                    }
                }
            }

            for (bi = 0; bi < R01_BG_BANKS; bi++) {
                /* Find bg_banks array entries sequentially from wblock */
                (void)bi;
            }
            {
                const char *banks = strstr(wblock, "\"bg_banks\"");
                const char *bcur;
                if (banks) {
                    bcur = strchr(banks, '[');
                    if (bcur) {
                        bcur++;
                        for (bi = 0; bi < R01_BG_BANKS; bi++) {
                            const char *bobj = strchr(bcur, '{');
                            const char *bend;
                            char *bblock;
                            size_t blen;
                            char *hexbuf;
                            size_t hexcap = R01_BANK_CHR_BYTES * 2 + 8;
                            R01BgBank *b;

                            if (!bobj) {
                                break;
                            }
                            bend = bobj + 1;
                            {
                                int depth = 1;
                                while (*bend && depth > 0) {
                                    if (*bend == '{') {
                                        depth++;
                                    } else if (*bend == '}') {
                                        depth--;
                                    }
                                    bend++;
                                }
                            }
                            blen = (size_t)(bend - bobj);
                            bblock = (char *)malloc(blen + 1);
                            hexbuf = (char *)malloc(hexcap);
                            if (!bblock || !hexbuf) {
                                free(bblock);
                                free(hexbuf);
                                free(wblock);
                                free(json);
                                set_err(err_buf, err_cap, "oom");
                                return -1;
                            }
                            memcpy(bblock, bobj, blen);
                            bblock[blen] = 0;
                            b = &w->bg_banks[bi];
                            memset(b, 0, sizeof(*b));
                            pcur = find_key(bblock, "tile_count");
                            parse_int_after(pcur, &b->tile_count);
                            if (b->tile_count < 0) {
                                b->tile_count = 0;
                            }
                            if (b->tile_count > R01_TILES_PER_BANK) {
                                b->tile_count = R01_TILES_PER_BANK;
                            }
                            pcur = find_key(bblock, "chr_hex");
                            if (pcur && parse_string_after(pcur, hexbuf, hexcap) == 0) {
                                size_t need = (size_t)b->tile_count * R01_TILE_BYTES;
                                if (parse_hex(hexbuf, b->chr, need) != 0 && need > 0) {
                                    free(bblock);
                                    free(hexbuf);
                                    free(wblock);
                                    free(json);
                                    set_err(err_buf, err_cap, "bad chr_hex");
                                    return -1;
                                }
                            }
                            free(bblock);
                            free(hexbuf);
                            bcur = bend;
                        }
                    }
                }
            }

            free(wblock);
            cursor = wend;
        }
    }

    if (p->active_world < 0 || p->active_world >= R01_MAX_WORLDS) {
        p->active_world = 0;
    }
    free(json);
    return 0;
}

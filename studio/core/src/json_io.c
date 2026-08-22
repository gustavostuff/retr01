#include "retr01_studio/json_io.h"
#include "retr01_studio/play.h"
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

static void write_constraints(FILE *f, const char *indent, const R01Constraints *c) {
    fprintf(f, "%s\"constraints\": {\n", indent);
    fprintf(f, "%s  \"player_meta\": %d,\n", indent, c->player_meta);
    fprintf(f, "%s  \"enemy_anim_rate\": %d,\n", indent, c->enemy_anim_rate);
    fprintf(f, "%s  \"anim_rate\": %d,\n", indent, c->anim_rate);
    fprintf(f, "%s  \"scroll_mode\": %d,\n", indent, c->scroll_mode);
    fprintf(f, "%s  \"deadzone_x\": %d,\n", indent, c->deadzone_x);
    fprintf(f, "%s  \"deadzone_y\": %d,\n", indent, c->deadzone_y);
    fprintf(f, "%s  \"transition\": %d\n", indent, c->transition);
    fprintf(f, "%s}", indent);
}

static void parse_constraints_obj(const char *obj, R01Constraints *c) {
    const char *pcur;
    if (!obj || !c) {
        return;
    }
    pcur = find_key(obj, "player_meta");
    parse_int_after(pcur, &c->player_meta);
    pcur = find_key(obj, "enemy_anim_rate");
    parse_int_after(pcur, &c->enemy_anim_rate);
    pcur = find_key(obj, "anim_rate");
    parse_int_after(pcur, &c->anim_rate);
    pcur = find_key(obj, "scroll_mode");
    parse_int_after(pcur, &c->scroll_mode);
    pcur = find_key(obj, "deadzone_x");
    parse_int_after(pcur, &c->deadzone_x);
    pcur = find_key(obj, "deadzone_y");
    parse_int_after(pcur, &c->deadzone_y);
    pcur = find_key(obj, "transition");
    parse_int_after(pcur, &c->transition);
    if (c->enemy_anim_rate < 1) {
        c->enemy_anim_rate = 1;
    }
    if (c->anim_rate < 1) {
        c->anim_rate = 1;
    }
    if (c->scroll_mode < 0 || c->scroll_mode > R01_SCROLL_HYBRID) {
        c->scroll_mode = R01_SCROLL_PIXEL;
    }
    if (c->transition != R01_XITION_FADE) {
        c->transition = R01_XITION_CUT;
    }
}

static void parse_constraints_key(const char *block, R01Constraints *c) {
    const char *pcur = find_key(block, "constraints");
    const char *brace;
    if (!pcur) {
        return;
    }
    brace = strchr(pcur, '{');
    if (brace) {
        parse_constraints_obj(brace, c);
    }
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
    fprintf(f, "  \"version\": 7,\n");
    fprintf(f, "  \"name\": \"%s\",\n", p->name);
    fprintf(f, "  \"active_world\": %d,\n", p->active_world);
    fprintf(f, "  \"active_screen\": %d,\n", p->active_screen);
    fprintf(f, "  \"active_plane\": %d,\n", p->active_plane);
    fprintf(f, "  \"generate_bank\": %d,\n", p->generate_bank);
    fprintf(f, "  \"paint_color\": %d,\n", p->paint_color);
    fprintf(f, "  \"has_cart_save\": %d,\n", p->has_cart_save ? 1 : 0);
    write_constraints(f, "  ", &p->constraints);
    fprintf(f, ",\n");

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
        fprintf(f, "      \"grid_cols\": %d,\n", w->grid_cols > 0 ? w->grid_cols : R01_GRID_SIZE);
        fprintf(f, "      \"grid_rows\": %d,\n", w->grid_rows > 0 ? w->grid_rows : R01_GRID_SIZE);
        fprintf(f, "      \"default_bg_bank\": %d,\n", w->default_bg_bank);
        fprintf(f, "      \"default_pal_row\": %d,\n", w->default_pal_row);
        fprintf(f, "      \"use_world_pals\": %d,\n", w->use_world_pals ? 1 : 0);
        fprintf(f, "      \"use_constraints\": %d,\n", w->use_constraints ? 1 : 0);
        write_constraints(f, "      ", &w->constraints);
        fprintf(f, ",\n");
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
            fprintf(f, "          \"pixels_hex\": \"");
            write_hex(f, s->pixels, sizeof(s->pixels));
            fprintf(f, "\",\n");
            fprintf(f, "          \"tiles_hex\": \"");
            write_hex(f, s->tiles, sizeof(s->tiles));
            fprintf(f, "\",\n");
            fprintf(f, "          \"attrs_hex\": \"");
            write_hex(f, s->attrs, sizeof(s->attrs));
            fprintf(f, "\",\n");
            fprintf(f, "          \"oam\": [\n");
            {
                int oi;
                for (oi = 0; oi < s->oam_count; oi++) {
                    const R01Oam *o = &s->oam[oi];
                    fprintf(f, "            {\"x\":%d,\"y\":%d,\"tile\":%d,\"attr\":%d}%s\n", o->x, o->y, o->tile,
                            o->attr, oi + 1 < s->oam_count ? "," : "");
                }
            }
            fprintf(f, "          ]\n");
            fprintf(f, "        }%s\n", si + 1 < w->screen_count ? "," : "");
        }
        fprintf(f, "      ],\n");
        fprintf(f, "      \"planes\": [\n");
        for (si = 0; si < R01_MAX_PARALLAX_PLANES; si++) {
            const R01ParallaxPlane *pl = &w->planes[si];
            fprintf(f, "        {\n");
            fprintf(f, "          \"present\": %d,\n", pl->present ? 1 : 0);
            fprintf(f, "          \"slot\": %d,\n", pl->slot);
            fprintf(f, "          \"pixels_hex\": \"");
            write_hex(f, pl->pixels, sizeof(pl->pixels));
            fprintf(f, "\",\n");
            fprintf(f, "          \"tiles_hex\": \"");
            write_hex(f, pl->tiles, sizeof(pl->tiles));
            fprintf(f, "\",\n");
            fprintf(f, "          \"attrs_hex\": \"");
            write_hex(f, pl->attrs, sizeof(pl->attrs));
            fprintf(f, "\"\n");
            fprintf(f, "        }%s\n", si + 1 < R01_MAX_PARALLAX_PLANES ? "," : "");
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
        fprintf(f, "      ],\n");
        fprintf(f, "      \"spr_banks\": [\n");
        for (bi = 0; bi < R01_SPR_BANKS; bi++) {
            const R01SprBank *b = &w->spr_banks[bi];
            size_t chr_n = (size_t)b->tile_count * R01_TILE_BYTES;
            fprintf(f, "        {\n");
            fprintf(f, "          \"tile_count\": %d,\n", b->tile_count);
            fprintf(f, "          \"chr_hex\": \"");
            write_hex(f, b->chr, chr_n);
            fprintf(f, "\"\n");
            fprintf(f, "        }%s\n", bi + 1 < R01_SPR_BANKS ? "," : "");
        }
        fprintf(f, "      ],\n");
        fprintf(f, "      \"metas\": [\n");
        {
            int mi;
            for (mi = 0; mi < w->meta_count; mi++) {
                const R01MetaSprite *m = &w->metas[mi];
                int pi;
                fprintf(f, "        {\n");
                fprintf(f, "          \"present\": %d,\n", m->present ? 1 : 0);
                fprintf(f, "          \"frame_count\": %d,\n", m->frame_count);
                fprintf(f, "          \"parts\": [\n");
                for (pi = 0; pi < m->part_count; pi++) {
                    const R01MetaPart *part = &m->parts[pi];
                    fprintf(f, "            {\"dx\":%d,\"dy\":%d,\"tile\":%d,\"attr\":%d}%s\n", part->dx, part->dy,
                            part->tile, part->attr, pi + 1 < m->part_count ? "," : "");
                }
                fprintf(f, "          ]\n");
                fprintf(f, "        }%s\n", mi + 1 < w->meta_count ? "," : "");
            }
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
    if (parse_int_after(pcur, &version) != 0 || version < 1 || version > 7) {
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
    p->active_plane = -1;
    pcur = find_key(json, "active_plane");
    parse_int_after(pcur, &p->active_plane);
    pcur = find_key(json, "generate_bank");
    parse_int_after(pcur, &p->generate_bank);
    pcur = find_key(json, "paint_color");
    parse_int_after(pcur, &p->paint_color);
    pcur = find_key(json, "has_cart_save");
    {
        int hs = 0;
        parse_int_after(pcur, &hs);
        p->has_cart_save = hs ? 1 : 0;
    }
    parse_constraints_key(json, &p->constraints);

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
            r01_constraints_init_default(&w->constraints);
            w->grid_cols = R01_GRID_SIZE;
            w->grid_rows = R01_GRID_SIZE;
            pcur = find_key(wblock, "present");
            {
                int pr = 0;
                parse_int_after(pcur, &pr);
                w->present = pr ? 1 : 0;
            }
            pcur = find_key(wblock, "grid_cols");
            {
                int gc = R01_GRID_SIZE;
                parse_int_after(pcur, &gc);
                if (gc < 1 || gc > R01_GRID_SIZE) {
                    gc = R01_GRID_SIZE;
                }
                w->grid_cols = gc;
            }
            pcur = find_key(wblock, "grid_rows");
            {
                int gr = R01_GRID_SIZE;
                parse_int_after(pcur, &gr);
                if (gr < 1 || gr > R01_GRID_SIZE) {
                    gr = R01_GRID_SIZE;
                }
                w->grid_rows = gr;
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
            pcur = find_key(wblock, "use_constraints");
            {
                int u = 0;
                parse_int_after(pcur, &u);
                w->use_constraints = u ? 1 : 0;
            }
            parse_constraints_key(wblock, &w->constraints);
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
                        /* legacy v2 "parallax" on screens is ignored */
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
                        {
                            const char *oam = strstr(sblock, "\"oam\"");
                            const char *ocur;
                            const char *oend = NULL;
                            if (oam) {
                                ocur = strchr(oam, '[');
                                if (ocur) {
                                    int depth = 0;
                                    const char *q = ocur;
                                    for (; *q; q++) {
                                        if (*q == '[') {
                                            depth++;
                                        } else if (*q == ']') {
                                            depth--;
                                            if (depth == 0) {
                                                oend = q;
                                                break;
                                            }
                                        }
                                    }
                                    ocur++;
                                    while (ocur && oend && ocur < oend && s->oam_count < R01_MAX_OAM_PER_SCREEN) {
                                        const char *obrace = NULL;
                                        const char *scan;
                                        for (scan = ocur; scan < oend; scan++) {
                                            if (*scan == '{') {
                                                obrace = scan;
                                                break;
                                            }
                                        }
                                        if (!obrace) {
                                            break;
                                        }
                                        {
                                            int x = 0, y = 0, tile = 0, attr = 0;
                                            const char *k;
                                            k = find_key(obrace, "x");
                                            parse_int_after(k, &x);
                                            k = find_key(obrace, "y");
                                            parse_int_after(k, &y);
                                            k = find_key(obrace, "tile");
                                            parse_int_after(k, &tile);
                                            k = find_key(obrace, "attr");
                                            parse_int_after(k, &attr);
                                            s->oam[s->oam_count].x = (uint8_t)x;
                                            s->oam[s->oam_count].y = (uint8_t)y;
                                            s->oam[s->oam_count].tile = (uint8_t)tile;
                                            s->oam[s->oam_count].attr = (uint8_t)attr;
                                            s->oam_count++;
                                        }
                                        ocur = strchr(obrace, '}');
                                        if (!ocur) {
                                            break;
                                        }
                                        ocur++;
                                    }
                                }
                            }
                        }

                        w->screen_count++;
                        free(sblock);
                        free(hex);
                        scur = send;
                    }
                }
            }

            {
                const char *planes = strstr(wblock, "\"planes\"");
                const char *p_arr;
                const char *p_end = NULL;
                const char *pcur2;
                int pi;
                if (planes) {
                    p_arr = strchr(planes, '[');
                    if (p_arr) {
                        int depth = 0;
                        const char *q = p_arr;
                        for (; *q; q++) {
                            if (*q == '[') {
                                depth++;
                            } else if (*q == ']') {
                                depth--;
                                if (depth == 0) {
                                    p_end = q;
                                    break;
                                }
                            }
                        }
                    }
                    pcur2 = p_arr ? p_arr + 1 : NULL;
                    if (pcur2 && p_end) {
                        for (pi = 0; pi < R01_MAX_PARALLAX_PLANES; pi++) {
                            const char *pobj = NULL;
                            const char *scan;
                            const char *pend;
                            char *pblock;
                            size_t plen;
                            R01ParallaxPlane *pl;
                            char *hex;
                            size_t hexcap = R01_SCREEN_PX_W * R01_SCREEN_PX_H * 2 + 8;

                            for (scan = pcur2; scan < p_end; scan++) {
                                if (*scan == '{') {
                                    pobj = scan;
                                    break;
                                }
                            }
                            if (!pobj) {
                                break;
                            }
                            pend = pobj + 1;
                            {
                                int depth = 1;
                                while (pend < p_end && depth > 0) {
                                    if (*pend == '{') {
                                        depth++;
                                    } else if (*pend == '}') {
                                        depth--;
                                    }
                                    pend++;
                                }
                            }
                            plen = (size_t)(pend - pobj);
                            pblock = (char *)malloc(plen + 1);
                            hex = (char *)malloc(hexcap);
                            if (!pblock || !hex) {
                                free(pblock);
                                free(hex);
                                free(wblock);
                                free(json);
                                set_err(err_buf, err_cap, "oom");
                                return -1;
                            }
                            memcpy(pblock, pobj, plen);
                            pblock[plen] = 0;
                            pl = &w->planes[pi];
                            memset(pl, 0, sizeof(*pl));
                            pl->slot = pi;
                            pcur = find_key(pblock, "present");
                            {
                                int pr = 0;
                                parse_int_after(pcur, &pr);
                                pl->present = pr ? 1 : 0;
                            }
                            pcur = find_key(pblock, "slot");
                            parse_int_after(pcur, &pl->slot);
                            if (pl->slot < 0 || pl->slot >= R01_MAX_PARALLAX_PLANES) {
                                pl->slot = pi;
                            }
                            pcur = find_key(pblock, "pixels_hex");
                            if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                                parse_hex(hex, pl->pixels, sizeof(pl->pixels));
                            }
                            pcur = find_key(pblock, "tiles_hex");
                            if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                                parse_hex(hex, pl->tiles, sizeof(pl->tiles));
                            }
                            pcur = find_key(pblock, "attrs_hex");
                            if (pcur && parse_string_after(pcur, hex, hexcap) == 0) {
                                parse_hex(hex, pl->attrs, sizeof(pl->attrs));
                            }
                            free(pblock);
                            free(hex);
                            pcur2 = pend;
                        }
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

            {
                const char *banks = strstr(wblock, "\"spr_banks\"");
                const char *bcur;
                if (banks) {
                    bcur = strchr(banks, '[');
                    if (bcur) {
                        bcur++;
                        for (bi = 0; bi < R01_SPR_BANKS; bi++) {
                            const char *bobj = strchr(bcur, '{');
                            const char *bend;
                            char *bblock;
                            size_t blen;
                            char *hexbuf;
                            size_t hexcap = R01_BANK_CHR_BYTES * 2 + 8;
                            R01SprBank *b;

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
                            b = &w->spr_banks[bi];
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
                                if (need > 0) {
                                    parse_hex(hexbuf, b->chr, need);
                                }
                            }
                            free(bblock);
                            free(hexbuf);
                            bcur = bend;
                        }
                    }
                }
            }

            {
                const char *metas = strstr(wblock, "\"metas\"");
                const char *mcur;
                const char *mend = NULL;
                if (metas) {
                    mcur = strchr(metas, '[');
                    if (mcur) {
                        int depth = 0;
                        const char *q = mcur;
                        for (; *q; q++) {
                            if (*q == '[') {
                                depth++;
                            } else if (*q == ']') {
                                depth--;
                                if (depth == 0) {
                                    mend = q;
                                    break;
                                }
                            }
                        }
                        mcur++;
                        while (mcur && mend && mcur < mend && w->meta_count < R01_MAX_METASPRITES) {
                            const char *mobj = NULL;
                            const char *scan;
                            const char *mobj_end;
                            R01MetaSprite *m;
                            for (scan = mcur; scan < mend; scan++) {
                                if (*scan == '{') {
                                    mobj = scan;
                                    break;
                                }
                            }
                            if (!mobj) {
                                break;
                            }
                            mobj_end = mobj + 1;
                            {
                                int d = 1;
                                while (mobj_end < mend && d > 0) {
                                    if (*mobj_end == '{') {
                                        d++;
                                    } else if (*mobj_end == '}') {
                                        d--;
                                    }
                                    mobj_end++;
                                }
                            }
                            m = &w->metas[w->meta_count];
                            memset(m, 0, sizeof(*m));
                            {
                                int pr = 1, fc = 1;
                                const char *k = find_key(mobj, "present");
                                parse_int_after(k, &pr);
                                m->present = pr ? 1 : 0;
                                k = find_key(mobj, "frame_count");
                                parse_int_after(k, &fc);
                                m->frame_count = fc < 1 ? 1 : (fc > R01_MAX_META_FRAMES ? R01_MAX_META_FRAMES : fc);
                            }
                            {
                                const char *parts = strstr(mobj, "\"parts\"");
                                const char *pcur2;
                                const char *pend = mobj_end;
                                if (parts && parts < mobj_end) {
                                    pcur2 = strchr(parts, '[');
                                    if (pcur2) {
                                        pcur2++;
                                        while (pcur2 < pend && m->part_count < R01_MAX_META_PARTS) {
                                            const char *pobj = NULL;
                                            const char *ps;
                                            for (ps = pcur2; ps < pend; ps++) {
                                                if (*ps == '{') {
                                                    pobj = ps;
                                                    break;
                                                }
                                                if (*ps == ']') {
                                                    pobj = NULL;
                                                    break;
                                                }
                                            }
                                            if (!pobj) {
                                                break;
                                            }
                                            {
                                                int dx = 0, dy = 0, tile = 0, attr = 0;
                                                const char *k = find_key(pobj, "dx");
                                                parse_int_after(k, &dx);
                                                k = find_key(pobj, "dy");
                                                parse_int_after(k, &dy);
                                                k = find_key(pobj, "tile");
                                                parse_int_after(k, &tile);
                                                k = find_key(pobj, "attr");
                                                parse_int_after(k, &attr);
                                                m->parts[m->part_count].dx = (int8_t)dx;
                                                m->parts[m->part_count].dy = (int8_t)dy;
                                                m->parts[m->part_count].tile = (uint8_t)tile;
                                                m->parts[m->part_count].attr = (uint8_t)attr;
                                                m->part_count++;
                                            }
                                            pcur2 = strchr(pobj, '}');
                                            if (!pcur2) {
                                                break;
                                            }
                                            pcur2++;
                                        }
                                    }
                                }
                            }
                            w->meta_count++;
                            mcur = mobj_end;
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

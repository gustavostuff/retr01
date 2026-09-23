#include "r01a_layout.h"

#include "r01a_board.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/entity.h"
#include "netlist_sim/island.h"
#include "netlist_sim/passive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *orient_str(NsPkgOrient o) {
    switch (o) {
    case NS_ORIENT_90:
        return "90";
    case NS_ORIENT_180:
        return "180";
    case NS_ORIENT_270:
        return "270";
    case NS_ORIENT_0:
    default:
        return "0";
    }
}

static NsPkgOrient orient_parse(const char *s) {
    if (!s) {
        return NS_ORIENT_0;
    }
    if (strcmp(s, "90") == 0) {
        return NS_ORIENT_90;
    }
    if (strcmp(s, "180") == 0) {
        return NS_ORIENT_180;
    }
    if (strcmp(s, "270") == 0) {
        return NS_ORIENT_270;
    }
    return NS_ORIENT_0;
}

static char *read_file(const char *path, size_t *len_out) {
    FILE *f;
    long sz;
    char *buf;
    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0 || sz > 4 * 1024 * 1024) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[sz] = '\0';
    if (len_out) {
        *len_out = (size_t)sz;
    }
    return buf;
}

static int json_int(const char *obj, const char *key, int *out) {
    char pat[64];
    const char *p;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(obj, pat);
    if (!p) {
        return 0;
    }
    p = strchr(p, ':');
    if (!p) {
        return 0;
    }
    *out = (int)strtol(p + 1, NULL, 10);
    return 1;
}

static int json_str(const char *obj, const char *key, char *out, size_t out_len) {
    char pat[64];
    const char *p;
    const char *q;
    size_t n;
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    p = strstr(obj, pat);
    if (!p) {
        return 0;
    }
    p = strchr(p, ':');
    if (!p) {
        return 0;
    }
    p = strchr(p, '"');
    if (!p) {
        return 0;
    }
    p++;
    q = strchr(p, '"');
    if (!q) {
        return 0;
    }
    n = (size_t)(q - p);
    if (n >= out_len) {
        n = out_len - 1;
    }
    memcpy(out, p, n);
    out[n] = '\0';
    return 1;
}

static NsPassiveKind kind_parse(const char *s) {
    if (!s) {
        return (NsPassiveKind)-1;
    }
    if (strcmp(s, "R") == 0) {
        return NS_PASSIVE_R;
    }
    if (strcmp(s, "CCAP") == 0) {
        return NS_PASSIVE_CCAP;
    }
    if (strcmp(s, "ECAP") == 0) {
        return NS_PASSIVE_ECAP;
    }
    if (strcmp(s, "OSC") == 0) {
        return NS_PASSIVE_OSC;
    }
    if (strcmp(s, "OSC4LEGS") == 0) {
        return NS_PASSIVE_OSC4LEGS;
    }
    if (strcmp(s, "D") == 0) {
        return NS_PASSIVE_D;
    }
    return (NsPassiveKind)-1;
}

static void ensure_part(R01aBoard *board, const char *id, const char *kind, const char *value, int x, int y) {
    NsPassiveKind pk;
    if (!board || !id || r01a_board_entity_by_refdes(board, id)) {
        return;
    }
    if (kind && strcmp(kind, "BB") == 0) {
        NsBreadboard *bb = r01a_board_add_breadboard(board, x, y);
        if (bb) {
            snprintf(bb->refdes_buf, sizeof(bb->refdes_buf), "%s", id);
            bb->base.refdes = bb->refdes_buf;
        }
        return;
    }
    pk = kind_parse(kind);
    if (pk >= 0) {
        NsPassive *p = r01a_board_add_passive(board, pk, value, x, y);
        if (p) {
            snprintf(p->refdes_buf, sizeof(p->refdes_buf), "%s", id);
            p->base.refdes = p->refdes_buf;
        }
    }
}

static void apply_part(R01aBoard *board, const char *id, int x, int y, NsPkgOrient orient, int px, int py,
                      int have_pivot) {
    NsEntity *e = r01a_board_entity_by_refdes(board, id);
    if (!e) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        NsPassive *p = (NsPassive *)e;
        ns_passive_set_orient(p, orient);
        if (have_pivot) {
            ns_passive_set_pivot(p, px, py);
        } else {
            ns_passive_set_pivot(p, p->pivot_x + (x - e->board_x), p->pivot_y + (y - e->board_y));
        }
        return;
    }
    ns_entity_set_orient(e, orient);
    if (e->visual == NS_ENTITY_VIS_BREADBOARD) {
        ns_breadboard_sync_body((NsBreadboard *)e);
    }
    ns_entity_place(e, x, y);
}

int r01a_layout_save(const char *path, const R01aBoard *board, int pan_x, int pan_y, int zoom, int air_always) {
    FILE *f;
    const NsIsland *island;
    int i;
    int first;

    if (!path || !board) {
        return -1;
    }
    if (zoom < 1) {
        zoom = 1;
    }
    if (zoom > 8) {
        zoom = 8;
    }
    island = ns_island_group_at(r01a_board_group((R01aBoard *)board), 0);
    f = fopen(path, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": 1,\n");
    fprintf(f, "  \"pan_x\": %d,\n", pan_x);
    fprintf(f, "  \"pan_y\": %d,\n", pan_y);
    fprintf(f, "  \"zoom\": %d,\n", zoom);
    fprintf(f, "  \"air_always\": %d,\n", air_always ? 1 : 0);
    fprintf(f, "  \"wire_mode\": \"%s\",\n", board->wire_mode == R01A_WIRE_MANUAL ? "manual" : "auto");
    fprintf(f, "  \"parts\": [\n");
    first = 1;
    if (island) {
        for (i = 0; i < island->entity_count; i++) {
            const NsEntity *e = island->entities[i];
            int px = 0;
            int py = 0;
            const char *kind = "";
            const char *value = "";
            if (!e || !e->refdes) {
                continue;
            }
            if (e->visual == NS_ENTITY_VIS_PASSIVE) {
                const NsPassive *p = (const NsPassive *)e;
                px = p->pivot_x;
                py = p->pivot_y;
                kind = ns_passive_kind_name(p->kind);
                value = p->value;
            } else if (e->visual == NS_ENTITY_VIS_BREADBOARD) {
                kind = "BB";
            }
            if (!first) {
                fprintf(f, ",\n");
            }
            first = 0;
            fprintf(f,
                    "    {\"id\": \"%s\", \"kind\": \"%s\", \"value\": \"%s\", \"x\": %d, \"y\": %d, "
                    "\"orient\": \"%s\", \"px\": %d, \"py\": %d}",
                    e->refdes, kind, value, e->board_x, e->board_y, orient_str(e->orient), px, py);
        }
    }
    fprintf(f, "\n  ],\n");
    fprintf(f, "  \"jumpers\": [\n");
    first = 1;
    for (i = 0; i < board->jumper_count; i++) {
        if (!first) {
            fprintf(f, ",\n");
        }
        first = 0;
        fprintf(f,
                "    {\"ac\": %d, \"al\": %d, \"bc\": %d, \"bl\": %d, \"ba\": \"%s\", \"bb\": \"%s\", "
                "\"cr\": %u, \"cg\": %u, \"cb\": %u, \"rt\": %u, \"hf\": %u, \"md\": %d}",
                board->jumpers[i].a.col, board->jumpers[i].a.lane, board->jumpers[i].b.col,
                board->jumpers[i].b.lane, r01a_jumper_a_ref(&board->jumpers[i]),
                r01a_jumper_b_ref(&board->jumpers[i]), (unsigned)board->jumpers[i].r,
                (unsigned)board->jumpers[i].g, (unsigned)board->jumpers[i].bcol,
                (unsigned)board->jumpers[i].route, (unsigned)board->jumpers[i].h_first,
                (int)board->jumpers[i].mid);
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);
    return 0;
}

int r01a_layout_load(const char *path, R01aBoard *board, int *pan_x, int *pan_y, int *zoom, int *air_always) {
    char *buf;
    const char *p;
    int n;
    int have_parts = 0;
    int saw_bb1 = 0;
    char mode[16];

    if (!path || !board) {
        return -1;
    }
    buf = read_file(path, NULL);
    if (!buf) {
        return -1;
    }
    if (json_int(buf, "pan_x", &n) && pan_x) {
        *pan_x = n;
    }
    if (json_int(buf, "pan_y", &n) && pan_y) {
        *pan_y = n;
    }
    if (json_int(buf, "zoom", &n) && zoom) {
        if (n < 1) {
            n = 1;
        }
        if (n > 8) {
            n = 8;
        }
        *zoom = n;
    }
    if (json_int(buf, "air_always", &n) && air_always) {
        *air_always = n ? 1 : 0;
    }
    if (json_str(buf, "wire_mode", mode, sizeof(mode))) {
        r01a_board_set_wire_mode(board, strcmp(mode, "manual") == 0 ? R01A_WIRE_MANUAL : R01A_WIRE_AUTO);
    }
    p = strstr(buf, "\"parts\"");
    if (p) {
        have_parts = 1;
        p = strchr(p, '[');
    }
    while (p && *p && *p != ']') {
        char id[24];
        char os[8];
        char kind[16];
        char value[32];
        char objbuf[384];
        int x = 0;
        int y = 0;
        int px = 0;
        int py = 0;
        int have_px;
        int have_py;
        size_t nobj;
        const char *end;
        p = strchr(p, '{');
        if (!p) {
            break;
        }
        end = strchr(p, '}');
        if (!end) {
            break;
        }
        nobj = (size_t)(end - p + 1);
        if (nobj >= sizeof(objbuf)) {
            nobj = sizeof(objbuf) - 1;
        }
        memcpy(objbuf, p, nobj);
        objbuf[nobj] = '\0';
        if (!json_str(objbuf, "id", id, sizeof(id))) {
            p = end + 1;
            continue;
        }
        if (strcmp(id, "BB1") == 0) {
            saw_bb1 = 1;
        }
        json_int(objbuf, "x", &x);
        json_int(objbuf, "y", &y);
        os[0] = '\0';
        kind[0] = '\0';
        value[0] = '\0';
        json_str(objbuf, "orient", os, sizeof(os));
        json_str(objbuf, "kind", kind, sizeof(kind));
        json_str(objbuf, "value", value, sizeof(value));
        have_px = json_int(objbuf, "px", &px);
        have_py = json_int(objbuf, "py", &py);
        ensure_part(board, id, kind, value, x, y);
        apply_part(board, id, x, y, orient_parse(os), px, py, have_px && have_py);
        p = end + 1;
    }
    r01a_board_jumper_clear(board);
    p = strstr(buf, "\"jumpers\"");
    if (p) {
        p = strchr(p, '[');
    }
    while (p && *p && *p != ']') {
        NsPbHole a;
        NsPbHole b;
        char ba[R01A_BB_REF_LEN];
        char bbref[R01A_BB_REF_LEN];
        char objbuf[384];
        const char *end;
        NsBreadboard *bba;
        NsBreadboard *bbb;
        size_t nobj;
        int cr = 220;
        int cg = 160;
        int cb = 40;
        int rt = 0;
        int hf = 0;
        int md = 0;
        p = strchr(p, '{');
        if (!p) {
            break;
        }
        end = strchr(p, '}');
        if (!end) {
            break;
        }
        nobj = (size_t)(end - p + 1);
        if (nobj >= sizeof(objbuf)) {
            nobj = sizeof(objbuf) - 1;
        }
        memcpy(objbuf, p, nobj);
        objbuf[nobj] = '\0';
        a.col = a.lane = b.col = b.lane = 0;
        ba[0] = '\0';
        bbref[0] = '\0';
        json_int(objbuf, "ac", &a.col);
        json_int(objbuf, "al", &a.lane);
        json_int(objbuf, "bc", &b.col);
        json_int(objbuf, "bl", &b.lane);
        json_str(objbuf, "ba", ba, sizeof(ba));
        json_str(objbuf, "bb", bbref, sizeof(bbref));
        json_int(objbuf, "cr", &cr);
        json_int(objbuf, "cg", &cg);
        json_int(objbuf, "cb", &cb);
        json_int(objbuf, "rt", &rt);
        json_int(objbuf, "hf", &hf);
        json_int(objbuf, "md", &md);
        if (cr < 0) {
            cr = 0;
        }
        if (cr > 255) {
            cr = 255;
        }
        if (cg < 0) {
            cg = 0;
        }
        if (cg > 255) {
            cg = 255;
        }
        if (cb < 0) {
            cb = 0;
        }
        if (cb > 255) {
            cb = 255;
        }
        if (!ba[0]) {
            snprintf(ba, sizeof(ba), "%s", bbref[0] ? bbref : "BB1");
        }
        if (!bbref[0]) {
            snprintf(bbref, sizeof(bbref), "%s", ba);
        }
        bba = (NsBreadboard *)r01a_board_entity_by_refdes(board, ba);
        bbb = (NsBreadboard *)r01a_board_entity_by_refdes(board, bbref);
        if (bba && bba->base.visual == NS_ENTITY_VIS_BREADBOARD && bbb &&
            bbb->base.visual == NS_ENTITY_VIS_BREADBOARD) {
            r01a_board_jumper_add_across(board, bba, a, bbb, b, (uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
        } else if (bba && bba->base.visual == NS_ENTITY_VIS_BREADBOARD) {
            r01a_board_jumper_add_on(board, bba, a, b, (uint8_t)cr, (uint8_t)cg, (uint8_t)cb);
        } else {
            r01a_board_jumper_add(board, a, b);
        }
        if (rt && board->jumper_count > 0) {
            r01a_board_jumper_set_route(board, board->jumper_count - 1, hf, md);
        }
        p = end + 1;
    }
    if (have_parts && !saw_bb1) {
        r01a_board_remove_breadboard(board, &board->breadboard);
    }
    free(buf);
    return 0;
}

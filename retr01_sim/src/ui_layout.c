#include "ui.h"

#include "retr01_sim/board_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define R01S_LAYOUT_VERSION 2

static const char *const LAYOUT_READ_PATHS[] = {
    "retr01_sim/ui_layout.json",
    "../retr01_sim/ui_layout.json",
    "ui_layout.json",
    NULL,
};

static const char *layout_write_path(void) {
    const char *env = getenv("R01S_LAYOUT");
    FILE *f;
    int i;
    if (env && env[0]) {
        return env;
    }
    /* Prefer creating/updating under retr01_sim/ when that dir exists. */
    f = fopen("retr01_sim/ui_layout.json", "r");
    if (f) {
        fclose(f);
        return "retr01_sim/ui_layout.json";
    }
    f = fopen("retr01_sim/.", "r");
    if (f) {
        fclose(f);
        return "retr01_sim/ui_layout.json";
    }
    for (i = 0; LAYOUT_READ_PATHS[i]; i++) {
        f = fopen(LAYOUT_READ_PATHS[i], "r");
        if (f) {
            fclose(f);
            return LAYOUT_READ_PATHS[i];
        }
    }
    return "ui_layout.json";
}

static int chip_index_by_refdes(const R01sUi *ui, const char *refdes) {
    int i;
    if (!ui || !refdes || !refdes[0]) {
        return -1;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        if (e && e->refdes && strcmp(e->refdes, refdes) == 0) {
            return i;
        }
    }
    return -1;
}

static int island_frames_saved(const R01sUi *ui) {
    int i;
    int n_islands;
    if (!ui || !ui->group) {
        return 0;
    }
    n_islands = r01s_island_group_count(ui->group);
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        if (ui->save_island_w[i] > 0 && ui->save_island_h[i] > 0) {
            return 1;
        }
    }
    return 0;
}

static void capture_snapshots(R01sUi *ui) {
    int i;
    if (!ui || !ui->group) {
        return;
    }
    /*
     * Island frames+chip relatives and compact chip abs positions are independent.
     * In island mode, refresh both from live. In compact mode, only refresh compact
     * chips — keep the last island-mode frame/chip snapshot (do not derive frames
     * from live group, which may still be builder defaults or a prior bad arrange).
     */
    if (ui->layout_compact) {
        if (!island_frames_saved(ui)) {
            r01s_ui_snapshot_island_frames(ui);
        }
        for (i = 0; i < ui->chip_count; i++) {
            const R01sEntity *e = ui->chips[i];
            ui->compact_chip_x[i] = e ? e->board_x : 0;
            ui->compact_chip_y[i] = e ? e->board_y : 0;
            ui->compact_chip_orient[i] = e ? (uint8_t)e->orient : (uint8_t)R01S_ORIENT_H;
        }
        ui->compact_saved = 1;
    } else {
        r01s_ui_snapshot_island_layout(ui);
    }
}

int r01s_ui_layout_save(R01sUi *ui) {
    const char *path;
    FILE *f;
    int i;
    int n_islands;
    int first;

    if (!ui || !ui->group) {
        return -1;
    }
    capture_snapshots(ui);
    path = layout_write_path();
    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "layout: could not write %s\n", path);
        return -1;
    }

    n_islands = r01s_island_group_count(ui->group);
    fprintf(f, "{\n");
    fprintf(f, "  \"version\": %d,\n", R01S_LAYOUT_VERSION);
    fprintf(f, "  \"mode\": \"%s\",\n", ui->layout_compact ? "compact" : "islands");
    fprintf(f, "  \"pan_x\": %d,\n", ui->pan_x);
    fprintf(f, "  \"pan_y\": %d,\n", ui->pan_y);

    /* Always persist both layouts in the same file:
     * - islands + island_chips: island-mode frames and island-relative chips
     * - compact_chips: absolute compact-mode chip placements
     * Active mode is recorded, but neither section is discarded when toggling.
     */
    fprintf(f, "  \"islands\": [\n");
    first = 1;
    for (i = 0; i < n_islands && i < R01S_MAX_ISLANDS; i++) {
        const R01sIsland *island = r01s_island_group_at(ui->group, i);
        int ix = 0;
        int iy = 0;
        int iw = 0;
        int ih = 0;
        ix = ui->save_island_x[i];
        iy = ui->save_island_y[i];
        iw = ui->save_island_w[i];
        ih = ui->save_island_h[i];
        /* Prefer dedicated island-mode snapshot; fall back to live frames. */
        if (iw <= 0 || ih <= 0) {
            if (island && island->board_w > 0 && island->board_h > 0) {
                ix = island->board_x;
                iy = island->board_y;
                iw = island->board_w;
                ih = island->board_h;
                ui->save_island_x[i] = ix;
                ui->save_island_y[i] = iy;
                ui->save_island_w[i] = iw;
                ui->save_island_h[i] = ih;
                ui->layout_saved = 1;
            } else {
                continue;
            }
        }
        if (!first) {
            fprintf(f, ",\n");
        }
        first = 0;
        fprintf(f, "    {\"i\": %d, \"x\": %d, \"y\": %d, \"w\": %d, \"h\": %d}", i, ix, iy, iw, ih);
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"island_chips\": [\n");
    first = 1;
    for (i = 0; i < ui->chip_count; i++) {
        const R01sEntity *e = ui->chips[i];
        const char *id;
        int rx;
        int ry;
        const char *orient_s;
        if (!e || !e->refdes) {
            continue;
        }
        id = e->refdes;
        if (ui->layout_compact) {
            /* Compact mode — keep last island-relative snapshot, not board coords. */
            rx = ui->save_chip_x[i];
            ry = ui->save_chip_y[i];
            orient_s = ui->save_chip_orient[i] == (uint8_t)R01S_ORIENT_V ? "V" : "H";
        } else {
            const R01sIsland *island = r01s_island_group_at(ui->group, ui->chip_island[i]);
            if (island) {
                rx = e->board_x - island->board_x;
                ry = e->board_y - island->board_y;
            } else {
                rx = ui->save_chip_x[i];
                ry = ui->save_chip_y[i];
            }
            /* Keep save_* in sync so a later compact save still has island chips. */
            ui->save_chip_x[i] = rx;
            ui->save_chip_y[i] = ry;
            ui->save_chip_orient[i] = (uint8_t)e->orient;
            orient_s = e->orient == R01S_ORIENT_V ? "V" : "H";
        }
        if (!first) {
            fprintf(f, ",\n");
        }
        first = 0;
        fprintf(f,
                "    {\"id\": \"%s\", \"island\": %d, \"rx\": %d, \"ry\": %d, \"orient\": \"%s\"}", id,
                (int)ui->chip_island[i], rx, ry, orient_s);
    }
    fprintf(f, "\n  ],\n");

    fprintf(f, "  \"compact_chips\": [\n");
    first = 1;
    if (ui->compact_saved) {
        for (i = 0; i < ui->chip_count; i++) {
            const R01sEntity *e = ui->chips[i];
            const char *id;
            if (!e || !e->refdes) {
                continue;
            }
            id = e->refdes;
            if (!first) {
                fprintf(f, ",\n");
            }
            first = 0;
            fprintf(f, "    {\"id\": \"%s\", \"x\": %d, \"y\": %d, \"orient\": \"%s\"}", id,
                    ui->compact_chip_x[i], ui->compact_chip_y[i],
                    ui->compact_chip_orient[i] == (uint8_t)R01S_ORIENT_V ? "V" : "H");
        }
    }
    fprintf(f, "\n  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    ui->layout_dirty = 0;
    return 0;
}

static const char *json_find(const char *hay, const char *needle) {
    return hay ? strstr(hay, needle) : NULL;
}

static int json_int_after(const char *p, const char *key, int *out) {
    const char *k;
    char *end;
    long v;
    if (!p || !key || !out) {
        return 0;
    }
    k = json_find(p, key);
    if (!k) {
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

static int json_string_after(const char *p, const char *key, char *buf, size_t buf_len) {
    const char *k;
    size_t n = 0;
    if (!p || !key || !buf || buf_len == 0) {
        return 0;
    }
    k = json_find(p, key);
    if (!k) {
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

static const char *json_next_object(const char *p) {
    const char *brace;
    if (!p) {
        return NULL;
    }
    brace = strchr(p, '{');
    return brace;
}

static const char *json_object_end(const char *obj) {
    int depth = 0;
    const char *p = obj;
    if (!p || *p != '{') {
        return NULL;
    }
    for (; *p; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                return p + 1;
            }
        }
    }
    return NULL;
}

int r01s_ui_layout_load(R01sUi *ui) {
    const char *path = NULL;
    char *buf = NULL;
    long sz;
    FILE *f = NULL;
    int i;
    int file_version = 1;
    int mode_compact = 0;
    int pan_x = 0;
    int pan_y = 0;
    char mode[16];
    const char *section;
    const char *obj;
    const char *end;
    int n_islands;

    if (!ui || !ui->group) {
        return -1;
    }
    {
        const char *env = getenv("R01S_LAYOUT");
        int pi;
        if (env && env[0]) {
            path = env;
            f = fopen(path, "rb");
        } else {
            for (pi = 0; LAYOUT_READ_PATHS[pi]; pi++) {
                f = fopen(LAYOUT_READ_PATHS[pi], "rb");
                if (f) {
                    path = LAYOUT_READ_PATHS[pi];
                    break;
                }
            }
        }
    }
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    mode[0] = '\0';
    json_int_after(buf, "\"version\"", &file_version);
    if (file_version <= 0) {
        file_version = 1;
    }
    json_string_after(buf, "\"mode\"", mode, sizeof(mode));
    mode_compact = (strcmp(mode, "compact") == 0);
    json_int_after(buf, "\"pan_x\"", &pan_x);
    json_int_after(buf, "\"pan_y\"", &pan_y);

    n_islands = r01s_island_group_count(ui->group);
    /* Clear prior frame snapshot so a missing/empty islands[] cannot look "valid". */
    for (i = 0; i < R01S_MAX_ISLANDS; i++) {
        ui->save_island_x[i] = 0;
        ui->save_island_y[i] = 0;
        ui->save_island_w[i] = 0;
        ui->save_island_h[i] = 0;
    }
    section = json_find(buf, "\"islands\"");
    if (section) {
        const char *stop = json_find(buf, "\"island_chips\"");
        int got_frame = 0;
        obj = json_next_object(section);
        while (obj && (!stop || obj < stop)) {
            int idx = -1, x = 0, y = 0, w = 0, h = 0;
            char slice[256];
            end = json_object_end(obj);
            if (!end || (size_t)(end - obj) >= sizeof(slice)) {
                break;
            }
            memcpy(slice, obj, (size_t)(end - obj));
            slice[end - obj] = '\0';
            if (json_int_after(slice, "\"i\"", &idx) && idx >= 0 && idx < n_islands &&
                idx < R01S_MAX_ISLANDS) {
                json_int_after(slice, "\"x\"", &x);
                json_int_after(slice, "\"y\"", &y);
                json_int_after(slice, "\"w\"", &w);
                json_int_after(slice, "\"h\"", &h);
                if (w > 0 && h > 0) {
                    ui->save_island_x[idx] = x;
                    ui->save_island_y[idx] = y;
                    ui->save_island_w[idx] = w;
                    ui->save_island_h[idx] = h;
                    got_frame = 1;
                }
            }
            obj = json_next_object(end);
        }
        if (got_frame) {
            ui->layout_saved = 1;
        }
    }

    section = json_find(buf, "\"island_chips\"");
    if (section) {
        const char *stop = json_find(buf, "\"compact_chips\"");
        obj = json_next_object(section);
        while (obj && (!stop || obj < stop)) {
            char slice[320];
            char id[32];
            char orient[8];
            int rx = 0;
            int ry = 0;
            int island = 0;
            int ci;
            end = json_object_end(obj);
            if (!end || (size_t)(end - obj) >= sizeof(slice)) {
                break;
            }
            memcpy(slice, obj, (size_t)(end - obj));
            slice[end - obj] = '\0';
            id[0] = '\0';
            orient[0] = 'H';
            orient[1] = '\0';
            if (json_string_after(slice, "\"id\"", id, sizeof(id))) {
                if (!json_int_after(slice, "\"rx\"", &rx)) {
                    json_int_after(slice, "\"x\"", &rx);
                }
                if (!json_int_after(slice, "\"ry\"", &ry)) {
                    json_int_after(slice, "\"y\"", &ry);
                }
                json_int_after(slice, "\"island\"", &island);
                json_string_after(slice, "\"orient\"", orient, sizeof(orient));
                ci = chip_index_by_refdes(ui, id);
                if (ci >= 0) {
                    ui->save_chip_x[ci] = rx;
                    ui->save_chip_y[ci] = ry;
                    ui->save_chip_orient[ci] =
                        (orient[0] == 'V' || orient[0] == 'v') ? (uint8_t)R01S_ORIENT_V
                                                               : (uint8_t)R01S_ORIENT_H;
                    if (island >= 0 && island < n_islands) {
                        ui->chip_island[ci] = (uint8_t)island;
                    }
                }
            }
            obj = json_next_object(end);
        }
        ui->layout_saved = 1;
    }

    section = json_find(buf, "\"compact_chips\"");
    if (section) {
        obj = json_next_object(section);
        while (obj) {
            char slice[320];
            char id[32];
            char orient[8];
            int x = 0, y = 0;
            int ci;
            end = json_object_end(obj);
            if (!end || (size_t)(end - obj) >= sizeof(slice)) {
                break;
            }
            memcpy(slice, obj, (size_t)(end - obj));
            slice[end - obj] = '\0';
            id[0] = '\0';
            orient[0] = 'H';
            orient[1] = '\0';
            if (json_string_after(slice, "\"id\"", id, sizeof(id))) {
                json_int_after(slice, "\"x\"", &x);
                json_int_after(slice, "\"y\"", &y);
                json_string_after(slice, "\"orient\"", orient, sizeof(orient));
                ci = chip_index_by_refdes(ui, id);
                if (ci >= 0) {
                    ui->compact_chip_x[ci] = x;
                    ui->compact_chip_y[ci] = y;
                    ui->compact_chip_orient[ci] =
                        (orient[0] == 'V' || orient[0] == 'v') ? (uint8_t)R01S_ORIENT_V
                                                               : (uint8_t)R01S_ORIENT_H;
                    ui->compact_saved = 1;
                }
            }
            obj = json_next_object(end);
        }
    }

    /* Apply island-mode geometry (frames first, then island-relative chips). */
    if (ui->layout_saved && !mode_compact) {
        r01s_ui_load_island_layout(ui, file_version);
    }

    ui->pan_x = pan_x;
    ui->pan_y = pan_y;
    ui->layout_compact = 0;
    if (mode_compact && ui->compact_saved) {
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *e = ui->chips[i];
            if (!e) {
                continue;
            }
            if (e->visual == R01S_ENTITY_VIS_IC) {
                r01s_entity_set_orient(e, (R01sPkgOrient)ui->compact_chip_orient[i]);
            }
            r01s_entity_place(e, ui->compact_chip_x[i], ui->compact_chip_y[i]);
        }
        ui->layout_compact = 1;
    }
    r01s_ui_clamp_pan(ui);
    ui->layout_dirty = 0;
    fprintf(stderr, "layout: loaded %s (%s)\n", path ? path : "?", mode_compact ? "compact" : "islands");
    free(buf);
    return 0;
}

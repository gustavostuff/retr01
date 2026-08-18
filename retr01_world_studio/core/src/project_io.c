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

static const char *next_screen_object(const char *json, const char *cursor, char **out_buf)
{
    const char *start;
    const char *end;
    size_t depth;
    size_t n;
    char *buf;

    *out_buf = NULL;

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
    buf = (char *)malloc(n + 1);
    if (!buf) {
        return NULL;
    }
    memcpy(buf, start, n);
    buf[n] = '\0';
    *out_buf = buf;
    return end;
}

static uint8_t *alloc_canvas(void)
{
    return (uint8_t *)calloc(1, RETR01_CANVAS_BYTES);
}

static void screen_free_canvas(retr01_project_screen_t *ps)
{
    if (!ps) {
        return;
    }
    free(ps->canvas);
    ps->canvas = NULL;
}

void retr01_project_free(retr01_project_t *proj)
{
    int i;
    if (!proj) {
        return;
    }
    for (i = 0; i < RETR01_PROJECT_MAX_SCREENS; i++) {
        screen_free_canvas(&proj->screens[i]);
    }
    memset(proj, 0, sizeof(*proj));
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
    proj->active_chr_tile = 0;
    proj->grid_w = 4;
    proj->grid_h = 4;
    proj->build_start_world = 0;
    proj->build_start_col = 0;
    proj->build_start_row = 0;
    snprintf(proj->build_output_name, sizeof(proj->build_output_name), "untitled");

    retr01_palette_set_defaults(&proj->palette);
    if (palette_v01_path) {
        retr01_palette_load_v01(palette_v01_path, &proj->palette);
    }

    proj->screen_count = 1;
    proj->active_screen = 0;
    sc = &proj->screens[0];
    snprintf(sc->id, sizeof(sc->id), "w0_c0_r0");
    sc->world = 0;
    retr01_screen_clear(&sc->screen);
    sc->canvas = alloc_canvas();
    sc->canvas_palette = 0;
    sc->generate_dirty = 1;
}

retr01_project_screen_t *retr01_project_active_screen(retr01_project_t *proj)
{
    if (!proj || proj->screen_count <= 0 || proj->active_screen < 0 ||
        proj->active_screen >= proj->screen_count) {
        return NULL;
    }
    return &proj->screens[proj->active_screen];
}

retr01_project_screen_t *retr01_project_find_screen(retr01_project_t *proj, int world, uint8_t col,
                                                    uint8_t row)
{
    int i;
    if (!proj) {
        return NULL;
    }
    for (i = 0; i < proj->screen_count; i++) {
        retr01_project_screen_t *ps = &proj->screens[i];
        if (ps->world == world && ps->screen.col == col && ps->screen.row == row) {
            return ps;
        }
    }
    return NULL;
}

retr01_project_screen_t *retr01_project_ensure_screen(retr01_project_t *proj, int world,
                                                      uint8_t col, uint8_t row)
{
    retr01_project_screen_t *found;
    int i;

    if (!proj) {
        return NULL;
    }

    found = retr01_project_find_screen(proj, world, col, row);
    if (found) {
        proj->active_screen = (int)(found - proj->screens);
        return found;
    }

    if (proj->screen_count >= RETR01_PROJECT_MAX_SCREENS) {
        return NULL;
    }

    i = proj->screen_count++;
    memset(&proj->screens[i], 0, sizeof(proj->screens[i]));
    snprintf(proj->screens[i].id, sizeof(proj->screens[i].id), "w%d_c%d_r%d", world, col, row);
    proj->screens[i].world = world;
    retr01_screen_clear(&proj->screens[i].screen);
    proj->screens[i].screen.col = col;
    proj->screens[i].screen.row = row;
    proj->screens[i].canvas = alloc_canvas();
    proj->screens[i].canvas_palette = (uint8_t)proj->active_bg_palette;
    proj->screens[i].generate_dirty = 1;
    proj->active_screen = i;
    return &proj->screens[i];
}

int retr01_project_delete_screen(retr01_project_t *proj, int index)
{
    int i;
    if (!proj || index < 0 || index >= proj->screen_count) {
        return -1;
    }
    if (proj->screen_count <= 1) {
        return -1;
    }
    screen_free_canvas(&proj->screens[index]);
    for (i = index; i < proj->screen_count - 1; i++) {
        proj->screens[i] = proj->screens[i + 1];
    }
    memset(&proj->screens[proj->screen_count - 1], 0, sizeof(proj->screens[0]));
    proj->screen_count--;
    if (proj->active_screen >= proj->screen_count) {
        proj->active_screen = proj->screen_count - 1;
    }
    return 0;
}

int retr01_project_collect_world_screens(const retr01_project_t *proj, int world,
                                         const retr01_project_screen_t **out, int max_out)
{
    int i;
    int n = 0;
    if (!proj || !out || max_out <= 0) {
        return -1;
    }
    for (i = 0; i < proj->screen_count && n < max_out; i++) {
        if (proj->screens[i].world == world) {
            out[n++] = &proj->screens[i];
        }
    }
    return n;
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
    fprintf(f, "  \"grid_w\": %u,\n", proj->grid_w);
    fprintf(f, "  \"grid_h\": %u,\n", proj->grid_h);
    fprintf(f, "  \"build\": {\n");
    fprintf(f, "    \"start_world\": %d,\n", proj->build_start_world);
    fprintf(f, "    \"start_col\": %d,\n", proj->build_start_col);
    fprintf(f, "    \"start_row\": %d,\n", proj->build_start_row);
    fprintf(f, "    \"output_name\": \"%s\"\n", proj->build_output_name);
    fprintf(f, "  },\n");
    fprintf(f, "  \"screens\": [\n");

    for (i = 0; i < proj->screen_count; i++) {
        const retr01_project_screen_t *ps = &proj->screens[i];
        const retr01_screen_t *s = &ps->screen;
        fprintf(f, "    {\n");
        fprintf(f, "      \"world\": %d,\n", ps->world);
        fprintf(f, "      \"col\": %u,\n", s->col);
        fprintf(f, "      \"row\": %u,\n", s->row);
        fprintf(f, "      \"flags\": %u,\n", s->flags);
        fprintf(f, "      \"authored_bank\": %u,\n", s->authored_bank);
        fprintf(f, "      \"canvas_palette\": %u,\n", ps->canvas_palette);
        fprintf(f, "      \"tiles_hex\": \"");
        encode_hex(f, s->tiles, sizeof(s->tiles));
        fprintf(f, "\",\n");
        fprintf(f, "      \"attrs_hex\": \"");
        encode_hex(f, s->attrs, sizeof(s->attrs));
        fprintf(f, "\",\n");
        fprintf(f, "      \"canvas_hex\": \"");
        if (ps->canvas) {
            encode_hex(f, ps->canvas, RETR01_CANVAS_BYTES);
        } else {
            /* keep field present for loaders */
        }
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
    char *screen_obj = NULL;
    const char *cursor = NULL;
    int screen_idx = 0;
    int bank;
    int gw = 4;
    int gh = 4;

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
    if (read_int_after_key(json, "grid_w", &gw) == 0 && gw >= 1 && gw <= 64) {
        proj->grid_w = (uint8_t)gw;
    }
    if (read_int_after_key(json, "grid_h", &gh) == 0 && gh >= 1 && gh <= 64) {
        proj->grid_h = (uint8_t)gh;
    }

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

    screen_free_canvas(&proj->screens[0]);
    proj->screen_count = 0;
    while ((cursor = next_screen_object(json, cursor, &screen_obj)) != NULL) {
        retr01_project_screen_t *ps;
        const char *tiles_hex;
        const char *attrs_hex;
        const char *canvas_hex;
        int col = 0;
        int row = 0;
        int world = 0;
        int flags = 0;
        int authored = 0;
        int cpal = 0;

        if (screen_idx >= RETR01_PROJECT_MAX_SCREENS) {
            free(screen_obj);
            free(json);
            return -1;
        }

        ps = &proj->screens[screen_idx];
        memset(ps, 0, sizeof(*ps));
        read_int_after_key(screen_obj, "world", &world);
        read_int_after_key(screen_obj, "col", &col);
        read_int_after_key(screen_obj, "row", &row);
        read_int_after_key(screen_obj, "flags", &flags);
        read_int_after_key(screen_obj, "authored_bank", &authored);
        read_int_after_key(screen_obj, "canvas_palette", &cpal);
        ps->world = world;
        ps->screen.col = (uint8_t)col;
        ps->screen.row = (uint8_t)row;
        ps->screen.flags = (uint8_t)flags;
        ps->screen.authored_bank = (uint8_t)authored;
        ps->canvas_palette = (uint8_t)cpal;
        snprintf(ps->id, sizeof(ps->id), "w%d_c%d_r%d", world, col, row);

        tiles_hex = find_hex_field(screen_obj, "tiles_hex");
        attrs_hex = find_hex_field(screen_obj, "attrs_hex");
        if (!tiles_hex || !attrs_hex ||
            decode_hex(tiles_hex, ps->screen.tiles, sizeof(ps->screen.tiles)) != 0 ||
            decode_hex(attrs_hex, ps->screen.attrs, sizeof(ps->screen.attrs)) != 0) {
            free(screen_obj);
            free(json);
            return -1;
        }

        ps->canvas = alloc_canvas();
        canvas_hex = find_hex_field(screen_obj, "canvas_hex");
        if (canvas_hex && ps->canvas) {
            decode_hex(canvas_hex, ps->canvas, RETR01_CANVAS_BYTES);
        }

        free(screen_obj);
        screen_obj = NULL;
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
        int used = 0;

        if (!banks) {
            break;
        }

        snprintf(bank_key, sizeof(bank_key), "\"bank\": %d", bank);
        bank_obj = strstr(banks, bank_key);
        if (!bank_obj) {
            continue;
        }

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
    retr01_map_build_screen_t *map_screens;
    retr01_map_build_world_t worlds[RETR01_MAX_WORLDS];
    size_t world_n[RETR01_MAX_WORLDS];
    retr01_cart_t cart;
    uint8_t *map_blob = NULL;
    size_t map_len = 0;
    int i;
    int w;
    int built = 0;
    int bank;

    if (!proj || !out_path) {
        return -1;
    }

    map_screens = (retr01_map_build_screen_t *)calloc(
        (size_t)RETR01_MAX_WORLDS * RETR01_PROJECT_MAX_SCREENS, sizeof(*map_screens));
    if (!map_screens) {
        return -1;
    }

    if (!proj || !out_path) {
        return -1;
    }

    bank = proj->active_bank;
    if (bank < 0 || bank > 3) {
        bank = 0;
    }

    memset(world_n, 0, sizeof(world_n));
    memset(worlds, 0, sizeof(worlds));

    for (i = 0; i < proj->screen_count; i++) {
        int world = proj->screens[i].world;
        size_t n;
        if (world < 0 || world >= RETR01_MAX_WORLDS) {
            continue;
        }
        n = world_n[world];
        if (n >= RETR01_PROJECT_MAX_SCREENS) {
            free(map_screens);
            return -1;
        }
        map_screens[world * RETR01_PROJECT_MAX_SCREENS + n].screen = proj->screens[i].screen;
        map_screens[world * RETR01_PROJECT_MAX_SCREENS + n].screen.authored_bank = (uint8_t)bank;
        world_n[world] = n + 1;
    }

    for (w = 0; w < RETR01_MAX_WORLDS; w++) {
        int max_col = 0;
        int max_row = 0;
        size_t n = world_n[w];
        if (n == 0) {
            continue;
        }
        for (i = 0; i < (int)n; i++) {
            retr01_screen_t *sc = &map_screens[w * RETR01_PROJECT_MAX_SCREENS + i].screen;
            if (sc->col > max_col) {
                max_col = sc->col;
            }
            if (sc->row > max_row) {
                max_row = sc->row;
            }
        }
        worlds[w].desc.grid_w = proj->grid_w;
        worlds[w].desc.grid_h = proj->grid_h;
        if (worlds[w].desc.grid_w < (uint8_t)(max_col + 1)) {
            worlds[w].desc.grid_w = (uint8_t)(max_col + 1);
        }
        if (worlds[w].desc.grid_h < (uint8_t)(max_row + 1)) {
            worlds[w].desc.grid_h = (uint8_t)(max_row + 1);
        }
        worlds[w].desc.empty_off = 0;
        worlds[w].screens = &map_screens[w * RETR01_PROJECT_MAX_SCREENS];
        worlds[w].screen_count = n;
        built++;
    }

    if (built <= 0) {
        free(map_screens);
        return -1;
    }

    if (retr01_map_build(worlds, RETR01_MAX_WORLDS, &map_blob, &map_len) != 0) {
        free(map_screens);
        return -1;
    }

    retr01_cart_init(&cart);
    cart.prg = (uint8_t *)malloc(4);
    if (!cart.prg) {
        free(map_blob);
        free(map_screens);
        return -1;
    }
    memset(cart.prg, 0xEA, 4);
    cart.prg_size = 4;

    cart.chr = (uint8_t *)malloc(RETR01_CHR_BANK_BYTES);
    if (!cart.chr) {
        free(map_blob);
        retr01_cart_free(&cart);
        free(map_screens);
        return -1;
    }
    memcpy(cart.chr, proj->chr_banks[bank], RETR01_CHR_BANK_BYTES);
    cart.chr_size = RETR01_CHR_BANK_BYTES;

    cart.map = map_blob;
    cart.map_size = map_len;
    cart.world_count = (uint8_t)built;

    i = retr01_cart_write_file(out_path, &cart);
    retr01_cart_free(&cart);
    free(map_screens);
    return i;
}

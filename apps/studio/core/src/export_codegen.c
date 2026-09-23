#include "retr01_studio/export_codegen.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ID_STEM_MAX 48

#ifndef R01_REPO_ROOT
#define R01_REPO_ROOT "."
#endif

static void set_err(char *err_buf, size_t err_cap, const char *msg) {
    if (err_buf && err_cap > 0) {
        snprintf(err_buf, err_cap, "%s", msg ? msg : "error");
    }
}

static int join_path(char *dst, size_t dst_sz, const char *dir, const char *leaf) {
    size_t dl;
    size_t ll;
    if (!dst || dst_sz < 1u || !dir || !leaf) {
        return -1;
    }
    dl = strlen(dir);
    ll = strlen(leaf);
    if (dl + 1u + ll + 1u > dst_sz) {
        return -1;
    }
    memcpy(dst, dir, dl);
    dst[dl] = '/';
    memcpy(dst + dl + 1u, leaf, ll + 1u);
    return 0;
}

static int join_path_err(char *dst, size_t dst_sz, const char *dir, const char *leaf, char *err_buf,
                         size_t err_cap) {
    if (join_path(dst, dst_sz, dir, leaf) != 0) {
        set_err(err_buf, err_cap, "path too long");
        return -1;
    }
    return 0;
}

static void split_stem(const char *path_stem, char *out_dir, size_t out_cap, char *base_name,
                       size_t base_cap) {
    const char *slash;
    if (!path_stem || !path_stem[0]) {
        if (out_dir && out_cap) {
            out_dir[0] = '.';
            out_dir[1] = '\0';
        }
        if (base_name && base_cap) {
            base_name[0] = '\0';
        }
        return;
    }
    slash = strrchr(path_stem, '/');
    if (!slash) {
        if (out_dir && out_cap) {
            out_dir[0] = '.';
            out_dir[1] = '\0';
        }
        if (base_name && base_cap) {
            snprintf(base_name, base_cap, "%s", path_stem);
        }
        return;
    }
    if (out_dir && out_cap) {
        size_t n = (size_t)(slash - path_stem);
        if (n >= out_cap) {
            n = out_cap - 1;
        }
        memcpy(out_dir, path_stem, n);
        out_dir[n] = '\0';
        if (n == 0) {
            out_dir[0] = '.';
            out_dir[1] = '\0';
        }
    }
    if (base_name && base_cap) {
        snprintf(base_name, base_cap, "%s", slash + 1);
    }
}

static int write_bytes(const char *path, const void *data, size_t len, char *err_buf, size_t err_cap) {
    FILE *f;
    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    f = fopen(path, "wb");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write file");
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, f) != len) {
        fclose(f);
        set_err(err_buf, err_cap, "write failed");
        return -1;
    }
    fclose(f);
    return 0;
}

static int copy_file(const char *src, const char *dst, char *err_buf, size_t err_cap) {
    FILE *in;
    FILE *out;
    char buf[4096];
    size_t n;
    if (r01_path_ensure_parent(dst, err_buf, err_cap) != 0) {
        return -1;
    }
    in = fopen(src, "rb");
    if (!in) {
        set_err(err_buf, err_cap, "cannot read file");
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        set_err(err_buf, err_cap, "cannot write file");
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            set_err(err_buf, err_cap, "copy failed");
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static void copy_capped(char *dst, size_t cap, const char *src) {
    size_t n;
    if (!dst || cap < 1u) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    n = strlen(src);
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int stem_in_list(char list[][ID_STEM_MAX], int n, const char *stem) {
    int i;
    for (i = 0; i < n; i++) {
        if (strcmp(list[i], stem) == 0) {
            return 1;
        }
    }
    return 0;
}

static void uniquify_stem(char *stem, size_t cap, char used[][ID_STEM_MAX], int *used_n, int used_cap) {
    char base[ID_STEM_MAX];
    char cand[ID_STEM_MAX];
    unsigned k = 2u;
    if (!stem || !used_n) {
        return;
    }
    copy_capped(base, sizeof(base), stem);
    copy_capped(cand, sizeof(cand), stem);
    while (stem_in_list(used, *used_n, cand)) {
        size_t n = strlen(base);
        copy_capped(cand, sizeof(cand), base);
        if (n + 4u < sizeof(cand)) {
            cand[n] = '_';
            if (k >= 10u) {
                cand[n + 1u] = (char)('0' + (k / 10u));
                cand[n + 2u] = (char)('0' + (k % 10u));
                cand[n + 3u] = '\0';
            } else {
                cand[n + 1u] = (char)('0' + k);
                cand[n + 2u] = '\0';
            }
        }
        k++;
        if (k > 99u) {
            break;
        }
    }
    copy_capped(stem, cap, cand);
    if (*used_n >= 0 && *used_n < used_cap) {
        copy_capped(used[*used_n], ID_STEM_MAX, stem);
        (*used_n)++;
    }
}

static void make_macro_stem(char *dst, size_t cap, const char *label) {
    char slug[R01_ENTITY_NAME_MAX];
    size_t i;
    r01_id_slugify(slug, sizeof(slug), label);
    for (i = 0; slug[i]; i++) {
        if (slug[i] >= 'a' && slug[i] <= 'z') {
            slug[i] = (char)(slug[i] - 'a' + 'A');
        }
    }
    if (slug[0] >= '0' && slug[0] <= '9') {
        dst[0] = 'N';
        dst[1] = '_';
        copy_capped(dst + 2, cap > 2u ? cap - 2u : 0u, slug);
    } else {
        copy_capped(dst, cap, slug);
    }
}

static int stem_reserved_ent(const char *stem) {
    return strcmp(stem, "TYPE_COUNT") == 0 || strcmp(stem, "TYPE_MAX") == 0 ||
           strcmp(stem, "MARKED_PLAYER") == 0;
}

static int stem_reserved_warp(const char *stem) {
    return strcmp(stem, "ENTRANCE_COUNT") == 0 || strcmp(stem, "ENTRANCE_MAX") == 0;
}

static void reserve_prefix(char *stem, size_t cap) {
    char tmp[ID_STEM_MAX];
    tmp[0] = 'N';
    tmp[1] = '_';
    copy_capped(tmp + 2, sizeof(tmp) - 2u, stem);
    copy_capped(stem, cap, tmp);
}

static int write_entity_ids_header(const char *path, const R01Project *p, char *err_buf, size_t err_cap) {
    FILE *f;
    char used[R01_MAX_ENTITY_TYPES][ID_STEM_MAX];
    int used_n = 0;
    int n = 0;
    int marked = 255;
    int i;

    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write file");
        return -1;
    }
    if (p) {
        n = p->entity_count;
        if (n < 0) {
            n = 0;
        }
        if (n > R01_MAX_ENTITY_TYPES) {
            n = R01_MAX_ENTITY_TYPES;
        }
        if (p->player_entity >= 0 && p->player_entity < n) {
            marked = p->player_entity;
        }
    }
    fprintf(f, "#ifndef R01_ENTITY_IDS_H\n");
    fprintf(f, "#define R01_ENTITY_IDS_H\n\n");
    fprintf(f, "/* Generated by Studio export. Overwrite OK. */\n\n");
    fprintf(f, "#define R01_ENT_TYPE_MAX %d\n", R01_MAX_ENTITY_TYPES);
    fprintf(f, "#define R01_ENT_TYPE_COUNT %d\n", n);
    fprintf(f, "#define R01_ENT_MARKED_PLAYER %d\n", marked);
    for (i = 0; i < n; i++) {
        const R01EntityType *e = &p->entities[i];
        char stem[ID_STEM_MAX];
        if (!e->present) {
            continue;
        }
        make_macro_stem(stem, sizeof(stem), r01_entity_display_name(e));
        if (stem_reserved_ent(stem)) {
            reserve_prefix(stem, sizeof(stem));
        }
        uniquify_stem(stem, sizeof(stem), used, &used_n, R01_MAX_ENTITY_TYPES);
        fprintf(f, "#define R01_ENT_%s %d\n", stem, i);
    }
    fprintf(f, "\n#endif\n");
    fclose(f);
    return 0;
}

static int write_warp_ids_header(const char *path, const R01Project *p, char *err_buf, size_t err_cap) {
    FILE *f;
    char used[R01_MAX_WARP_ENTRANCES][ID_STEM_MAX];
    int used_n = 0;
    const R01World *w = p ? &p->worlds[0] : NULL;
    int n = 0;
    int present_n = 0;
    int i;

    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write file");
        return -1;
    }
    if (w) {
        n = w->warp_entrance_count;
        if (n < 0) {
            n = 0;
        }
        if (n > R01_MAX_WARP_ENTRANCES) {
            n = R01_MAX_WARP_ENTRANCES;
        }
        for (i = 0; i < n; i++) {
            if (w->warp_entrances[i].present) {
                present_n++;
            }
        }
    }
    fprintf(f, "#ifndef R01_WARP_IDS_H\n");
    fprintf(f, "#define R01_WARP_IDS_H\n\n");
    fprintf(f, "/* Generated by Studio export. Overwrite OK. */\n\n");
    fprintf(f, "#define R01_WARP_ENTRANCE_MAX %d\n", R01_MAX_WARP_ENTRANCES);
    fprintf(f, "#define R01_WARP_ENTRANCE_COUNT %d\n", present_n);
    for (i = 0; i < n; i++) {
        const R01WarpEntrance *e = &w->warp_entrances[i];
        char stem[ID_STEM_MAX];
        if (!e->present) {
            continue;
        }
        make_macro_stem(stem, sizeof(stem), e->id[0] ? e->id : "unnamed");
        if (stem_reserved_warp(stem)) {
            reserve_prefix(stem, sizeof(stem));
        }
        uniquify_stem(stem, sizeof(stem), used, &used_n, R01_MAX_WARP_ENTRANCES);
        fprintf(f, "#define R01_WARP_%s %d\n", stem, i);
    }
    fprintf(f, "\n#endif\n");
    fclose(f);
    return 0;
}

static int write_generated_ids(const char *out_dir, const R01Project *p, char *err_buf, size_t err_cap) {
    char inc_dir[R01_PATH_MAX];
    char path[R01_PATH_MAX];
    if (join_path_err(inc_dir, sizeof(inc_dir), out_dir, "include", err_buf, err_cap) != 0) {
        return -1;
    }
    if (r01_path_mkdir_p(inc_dir, err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_entity_ids.h", err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_entity_ids_header(path, p, err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_warp_ids.h", err_buf, err_cap) != 0) {
        return -1;
    }
    return write_warp_ids_header(path, p, err_buf, err_cap);
}

static int write_game_logic_once(const char *out_dir, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    char src[R01_PATH_MAX];
    struct stat st;
    if (join_path_err(path, sizeof(path), out_dir, "game_logic.c", err_buf, err_cap) != 0) {
        return -1;
    }
    if (stat(path, &st) == 0) {
        return 0;
    }
    snprintf(src, sizeof(src), "%s/apps/sdk/r01_c/game_logic.c", R01_REPO_ROOT);
    if (copy_file(src, path, err_buf, err_cap) == 0) {
        return 0;
    }
    {
        static const char k_logic[] =
            "/* Author game logic. Created once. Never overwritten. */\n"
            "#include <r01_engine.h>\n"
            "#include \"r01_entity_ids.h\"\n"
            "#include \"r01_warp_ids.h\"\n\n"
            "/* Catalog indexes: R01_ENT_* and R01_WARP_* from include/ (export overwrites). */\n\n"
            "void r01_game_on_init(R01GameCtx *ctx) {\n"
            "    r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);\n"
            "    r01_camera_set_deadzone(ctx, 32, 70);\n"
            "    r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);\n"
            "    r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT);\n"
            "    r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);\n"
            "    r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);\n"
            "    r01_player_anim_set_idle_state(ctx, 0);\n"
            "    r01_player_anim_set_walk_all(ctx, 1);\n"
            "    r01_player_anim_set_crouch_state(ctx, 3);\n"
            "    r01_player_anim_set_jump_state(ctx, 2);\n"
            "    r01_bgm_play(ctx, 1);\n"
            "}\n\n"
            "void r01_game_on_tick(R01GameCtx *ctx) {\n"
            "    if (r01_pad_down(ctx, R01_PAD_X) && r01_player_moving_x(ctx)) {\n"
            "        r01_player_set_move_mul(ctx, 2);\n"
            "        r01_player_anim_set_frame_delay(ctx, 5);\n"
            "    }\n"
            "}\n\n"
            "void r01_game_on_vblank(R01GameCtx *ctx) {\n"
            "    (void)ctx;\n"
            "}\n";
        return write_bytes(path, k_logic, sizeof(k_logic) - 1u, err_buf, err_cap);
    }
}

static int write_data_bins(const char *data_dir, const R01Project *p, char *err_buf, size_t err_cap) {
    const R01World *w = p ? &p->worlds[0] : NULL;
    char path[R01_PATH_MAX];
    uint8_t pal_plane[R01_PAL_PLANE_BYTES];
    int row, pal, c, o = 0;
    int si;

    if (!w) {
        return 0;
    }
    if (r01_path_mkdir_p(data_dir, err_buf, err_cap) != 0) {
        return -1;
    }

    for (row = 0; row < R01_PAL_ROWS; row++) {
        for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
            for (c = 0; c < R01_PAL_COLORS; c++) {
                pal_plane[o++] = p->global_pal_bg[row][pal].idx[c] & 63u;
            }
        }
    }
    if (join_path_err(path, sizeof(path), data_dir, "pal_bg.bin", err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_bytes(path, pal_plane, sizeof(pal_plane), err_buf, err_cap) != 0) {
        return -1;
    }
    o = 0;
    for (row = 0; row < R01_PAL_ROWS; row++) {
        for (pal = 0; pal < R01_PALS_PER_ROW; pal++) {
            for (c = 0; c < R01_PAL_COLORS; c++) {
                pal_plane[o++] = p->global_pal_spr[row][pal].idx[c] & 63u;
            }
        }
    }
    if (join_path_err(path, sizeof(path), data_dir, "pal_spr.bin", err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_bytes(path, pal_plane, sizeof(pal_plane), err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), data_dir, "chr_bg0.bin", err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_bytes(path, p->bg_banks[0].chr, R01_BANK_CHR_BYTES, err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), data_dir, "chr_spr0.bin", err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_bytes(path, p->spr_banks[0].chr, R01_BANK_CHR_BYTES, err_buf, err_cap) != 0) {
        return -1;
    }

    for (si = 0; si < w->screen_count; si++) {
        const R01Screen *s = &w->screens[si];
        uint8_t map[R01_CART_SCREEN_PAYLOAD];
        char leaf[64];
        if (!s->present) {
            continue;
        }
        memcpy(map, s->tiles, R01_TILES_PER_SCREEN);
        memcpy(map + R01_TILES_PER_SCREEN, s->attrs, R01_ATTRS_PER_SCREEN);
        snprintf(leaf, sizeof(leaf), "map_screen_%d_%d.bin", s->col, s->row);
        if (join_path_err(path, sizeof(path), data_dir, leaf, err_buf, err_cap) != 0) {
            return -1;
        }
        if (write_bytes(path, map, sizeof(map), err_buf, err_cap) != 0) {
            return -1;
        }
    }
    {
        uint8_t buf[1 + R01_MAX_ENTITY_INSTANCES * R01_CART_INSTANCE_SIZE];
        int n = w->instance_count;
        int i;
        if (n < 0) {
            n = 0;
        }
        if (n > R01_MAX_ENTITY_INSTANCES) {
            n = R01_MAX_ENTITY_INSTANCES;
        }
        buf[0] = (uint8_t)n;
        for (i = 0; i < n; i++) {
            uint8_t *rec = buf + 1 + i * R01_CART_INSTANCE_SIZE;
            const R01EntityInstance *inst = &w->instances[i];
            rec[0] = (uint8_t)inst->type_id;
            rec[1] = (uint8_t)((inst->flip_h ? 1u : 0u) | (inst->flip_v ? 2u : 0u));
            rec[2] = (uint8_t)(inst->world_x & 0xFFu);
            rec[3] = (uint8_t)((inst->world_x >> 8) & 0xFFu);
            rec[4] = (uint8_t)(inst->world_y & 0xFFu);
            rec[5] = (uint8_t)((inst->world_y >> 8) & 0xFFu);
        }
        if (join_path_err(path, sizeof(path), data_dir, "spawns.bin", err_buf, err_cap) != 0) {
            return -1;
        }
        if (write_bytes(path, buf, 1u + (size_t)n * R01_CART_INSTANCE_SIZE, err_buf, err_cap) != 0) {
            return -1;
        }
    }
    return 0;
}

int r01_export_compile_prg(const char *path_stem, char *err_buf, size_t err_cap) {
    char out_dir[R01_PATH_MAX];
    char base_name[64];
    char logic[R01_PATH_MAX];
    char out_prg[R01_PATH_MAX];
    uint8_t prg[R01_PRG_BYTES];
    struct stat st;

    if (!path_stem) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    split_stem(path_stem, out_dir, sizeof(out_dir), base_name, sizeof(base_name));
    (void)base_name;
    if (join_path_err(logic, sizeof(logic), out_dir, "game_logic.c", err_buf, err_cap) != 0) {
        return -1;
    }
    if (stat(logic, &st) != 0) {
        snprintf(logic, sizeof(logic), "%s/apps/sdk/r01_c/game_logic.c", R01_REPO_ROOT);
    }
    if (join_path_err(out_prg, sizeof(out_prg), out_dir, "retr01.prg", err_buf, err_cap) != 0) {
        return -1;
    }
    if (!r01_prg_needs_rebuild(out_prg, logic)) {
        return 0;
    }
    return r01_prg_compile_sdk(logic, prg, out_prg, err_buf, err_cap);
}

int r01_export_codegen(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap) {
    char out_dir[R01_PATH_MAX];
    char base_name[64];
    char path[R01_PATH_MAX];

    if (!p || !path_stem) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    split_stem(path_stem, out_dir, sizeof(out_dir), base_name, sizeof(base_name));
    (void)base_name;
    if (write_game_logic_once(out_dir, err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_generated_ids(out_dir, p, err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), out_dir, "data", err_buf, err_cap) != 0) {
        return -1;
    }
    return write_data_bins(path, p, err_buf, err_cap);
}

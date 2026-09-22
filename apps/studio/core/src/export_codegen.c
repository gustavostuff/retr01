#include "retr01_studio/export_codegen.h"
#include "retr01_studio/prg_phase1.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
            "#include <r01_engine.h>\n\n"
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
            "    r01_solid_pattern_add(ctx, 0, 1);\n"
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
    if (join_path_err(path, sizeof(path), out_dir, "data", err_buf, err_cap) != 0) {
        return -1;
    }
    return write_data_bins(path, p, err_buf, err_cap);
}

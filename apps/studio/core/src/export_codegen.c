#include "retr01_studio/export_codegen.h"
#include "retr01_studio/collision.h"
#include "retr01_studio/entities.h"
#include "retr01_studio/play.h"
#include "retr01_studio/project.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef R01_REPO_ROOT
#define R01_REPO_ROOT "."
#endif
#ifndef R01_PKG_ROOT
#define R01_PKG_ROOT R01_REPO_ROOT "/retr01"
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

static int write_text(const char *path, const char *text, char *err_buf, size_t err_cap) {
    return write_bytes(path, text, text ? strlen(text) : 0, err_buf, err_cap);
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
        set_err(err_buf, err_cap, "cannot open source");
        return -1;
    }
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        set_err(err_buf, err_cap, "cannot write dest");
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

static int ensure_codegen_dirs(const char *out_dir, char *err_buf, size_t err_cap) {
    static const char *subs[] = {
        "C",           "C/include",   "ASM",         "ASM/include", "ASM/boot",   "ASM/game",
        "ASM/io",      "ASM/player",  "ASM/sprite",  "ASM/collision", "ASM/tables", "ASM/link",
        "data",
    };
    char path[R01_PATH_MAX];
    size_t i;
    for (i = 0; i < sizeof(subs) / sizeof(subs[0]); i++) {
        if (join_path_err(path, sizeof(path), out_dir, subs[i], err_buf, err_cap) != 0) {
            return -1;
        }
        if (r01_path_mkdir_p(path, err_buf, err_cap) != 0) {
            return -1;
        }
    }
    return 0;
}

static void fill_present_mask(uint8_t mask[32], const R01World *w) {
    int i;
    memset(mask, 0, 32);
    if (!w) {
        return;
    }
    for (i = 0; i < w->screen_count; i++) {
        const R01Screen *s = &w->screens[i];
        unsigned bit;
        if (!s->present || s->col < 0 || s->col >= R01_GRID_MAX || s->row < 0 || s->row >= R01_GRID_MAX) {
            continue;
        }
        bit = (unsigned)s->col;
        mask[(size_t)s->row * 2u + bit / 8u] =
            (uint8_t)(mask[(size_t)s->row * 2u + bit / 8u] | (uint8_t)(1u << (bit % 8u)));
    }
}

static void pick_spawn_screen(const R01World *w, int *col, int *row) {
    int idx;
    if (!col || !row) {
        return;
    }
    *col = R01_START_COL;
    *row = R01_START_ROW;
    if (!w) {
        return;
    }
    idx = r01_world_default_screen(w);
    if (idx >= 0 && idx < w->screen_count && w->screens[idx].present) {
        *col = w->screens[idx].col;
        *row = w->screens[idx].row;
    }
}

static int write_headers(const char *inc_dir, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    if (join_path_err(path, sizeof(path), inc_dir, "r01_game.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "/* Generated by Retr01 Studio export. */\n"
                   "#ifndef R01_GAME_H\n#define R01_GAME_H\n\n"
                   "#include <stdint.h>\n\n"
                   "typedef struct R01GameCtx {\n"
                   "    int player_x;\n"
                   "    int player_y;\n"
                   "    int cam_x;\n"
                   "    int cam_y;\n"
                   "    uint8_t pad;\n"
                   "    uint8_t pad_prev;\n"
                   "    int cam_deadzone_x;\n"
                   "    int cam_deadzone_y;\n"
                   "    int cam_axis_lock;\n"
                   "    int bg0_wrap_x;\n"
                   "    int bg0_wrap_y;\n"
                   "    int bg0_clip_bg1;\n"
                   "    int fade_level;\n"
                   "    int fade_target;\n"
                   "    int fade_color;\n"
                   "    int fade_pending_entrance;\n"
                   "    int player_anim_state;\n"
                   "    int player_anim_frame;\n"
                   "    int player_anim_ctr;\n"
                   "    int player_anim_flip_h;\n"
                   "    int player_anim_dir;\n"
                   "    int player_anim_moving;\n"
                   "    int player_default_face;\n"
                   "    int player_idle_state;\n"
                   "    int player_walk_state[8];\n"
                   "    int player_state_delay[4];\n"
                   "    int player_crouch_state;\n"
                   "    int player_crouching;\n"
                   "    int player_jump_state;\n"
                   "    int player_airborne;\n"
                   "    int game_mode;\n"
                   "    int plat_gravity;\n"
                   "    int plat_jump;\n"
                   "    int plat_meter;\n"
                   "    int plat_vel_y;\n"
                   "    int plat_frac_x;\n"
                   "    int plat_frac_y;\n"
                   "    int plat_grounded;\n"
                   "    int plat_jump_held;\n"
                   "    int player_run_on_x;\n"
                   "    uint8_t solid_pat_count;\n"
                   "    uint8_t solid_pat_bank[64];\n"
                   "    uint8_t solid_pat_tile[64];\n"
                   "    int bgm_track;\n"
                   "    struct R01Projectile {\n"
                   "        int active;\n"
                   "        int x, y, vx, vy, ttl;\n"
                   "        uint8_t tile, pal;\n"
                   "    } projectiles[8];\n"
                   "} R01GameCtx;\n\n"
                   "void r01_game_init(R01GameCtx *ctx);\n"
                   "void r01_game_tick(R01GameCtx *ctx);\n"
                   "void r01_game_vblank(R01GameCtx *ctx);\n\n"
                   "void r01_custom_on_init(R01GameCtx *ctx);\n"
                   "void r01_custom_on_tick(R01GameCtx *ctx);\n"
                   "void r01_custom_on_vblank(R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_engine.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_ENGINE_H\n#define R01_ENGINE_H\n\n"
                   "#include \"r01_game.h\"\n"
                   "#include \"r01_input.h\"\n"
                   "#include \"r01_player.h\"\n"
                   "#include \"r01_entity.h\"\n"
                   "#include \"r01_camera.h\"\n"
                   "#include \"r01_bg0.h\"\n"
                   "#include \"r01_physics.h\"\n"
                   "#include \"r01_events.h\"\n"
                   "#include \"r01_fade.h\"\n"
                   "#include \"r01_warp.h\"\n"
                   "#include \"r01_projectile.h\"\n"
                   "#include \"r01_player_anim.h\"\n"
                   "#include \"r01_bgm.h\"\n"
                   "#include \"r01_sfx.h\"\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_bgm.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_BGM_H\n#define R01_BGM_H\n\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "/* Start looping BGM track (1-based). Host Play reads packed PRG at $B000. */\n"
                   "void r01_bgm_play(R01GameCtx *ctx, int track);\n"
                   "void r01_bgm_stop(R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_sfx.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_SFX_H\n#define R01_SFX_H\n\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_SFX_X 1 /* pulse blip */\n"
                   "#define R01_SFX_Y 2 /* noise tick */\n"
                   "/* Short SFX on voices 6-8. */\n"
                   "void r01_sfx_play(R01GameCtx *ctx, int id);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_input.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_INPUT_H\n#define R01_INPUT_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn);\n"
                   "uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn);\n"
                   "#define R01_BTN_X 0\n#define R01_BTN_Y 1\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_player.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_PLAYER_H\n#define R01_PLAYER_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "void r01_player_warp(R01GameCtx *ctx, int col, int row);\n"
                   "void r01_player_set_type(uint8_t type_id);\n"
                   "/* Hold face X for 2x walk. Host Play packs this into world flags bit 5. */\n"
                   "void r01_player_set_run_on_x(R01GameCtx *ctx);\n\n"
                   "#include \"r01_player_anim.h\"\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_entity.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_ENTITY_H\n#define R01_ENTITY_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n\n"
                   "/* Soft contract: docs/general/software-api.md (4x4x6 parts, 16 live). */\n"
                   "#define R01_ENTITY_ONSCREEN_MAX 16\n\n"
                   "int  r01_entity_spawn(uint8_t type, int wx, int wy);\n"
                   "void r01_entity_remove(int inst);\n"
                   "void r01_entity_set_active(int inst, int on);\n\n"
                   "void r01_entity_set_xy(int inst, int wx, int wy);\n"
                   "void r01_entity_get_xy(int inst, int *wx, int *wy);\n"
                   "void r01_entity_set_state(int inst, uint8_t state);\n"
                   "void r01_entity_set_frame(int inst, uint8_t frame);\n"
                   "void r01_entity_set_flip(int inst, int flip_h, int flip_v);\n"
                   "void r01_entity_get_flip(int inst, int *flip_h, int *flip_v);\n"
                   "void r01_entity_set_rot90(int inst, uint8_t quad);\n\n"
                   "int  r01_entity_alive(int inst);\n"
                   "int  r01_entity_count_live(void);\n"
                   "uint8_t r01_entity_type(int inst);\n\n"
                   "void r01_world_warp_screen(int col, int row);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_camera.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_CAMERA_H\n#define R01_CAMERA_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_CAM_AXIS_BOTH 0\n"
                   "#define R01_CAM_AXIS_H 1\n"
                   "#define R01_CAM_AXIS_V 2\n"
                   "#define R01_CAM_DEADZONE_OFF 0\n"
                   "#define R01_CAM_DEADZONE_X_DEFAULT 32\n"
                   "#define R01_CAM_DEADZONE_Y_DEFAULT 30\n"
                   "void r01_game_camera_update(R01GameCtx *ctx);\n"
                   "void r01_game_camera_snap(R01GameCtx *ctx);\n"
                   "/* Packs width/height. Follow may snap edges 1 px inward (world-scrolling.md). */\n"
                   "void r01_camera_set_deadzone(R01GameCtx *ctx, int dx, int dy);\n"
                   "void r01_camera_disable_deadzone(R01GameCtx *ctx);\n"
                   "void r01_camera_set_axis_lock(R01GameCtx *ctx, int mode);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_bg0.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_BG0_H\n#define R01_BG0_H\n\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_BG0_WRAP_OFF 0\n"
                   "#define R01_BG0_WRAP_ON 1\n"
                   "#define R01_BG0_CLIP_OFF 0\n"
                   "#define R01_BG0_CLIP_ON 1\n"
                   "/* Tile present BG0 layout on X/Y when sampling leaves the box. Wrap axes use period n/n rate. */\n"
                   "void r01_bg0_set_wrap(R01GameCtx *ctx, int wrap_x, int wrap_y);\n"
                   "/* Hide BG0 outside present BG1 camera slots when enable != 0. */\n"
                   "void r01_bg0_set_clip_to_bg1(R01GameCtx *ctx, int enable);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_physics.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_PHYSICS_H\n#define R01_PHYSICS_H\n\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_GAME_MODE_TOPDOWN 0\n"
                   "#define R01_GAME_MODE_PLATFORMER 1\n"
                   "#define R01_PLAT_GRAVITY_SCALE 16\n"
                   "#define R01_PLAT_GRAVITY_DEFAULT 4\n"
                   "#define R01_PLAT_GRAVITY_MAX 255\n"
                   "#define R01_PLAT_JUMP_DEFAULT 4\n"
                   "#define R01_PLAT_METER_DEFAULT 16\n"
                   "void r01_game_set_mode(R01GameCtx *ctx, int mode);\n"
                   "void r01_solid_pattern_add(R01GameCtx *ctx, int bank, int tile);\n"
                   "void r01_platformer_set_gravity(R01GameCtx *ctx, int units);\n"
                   "void r01_platformer_set_jump(R01GameCtx *ctx, int impulse);\n"
                   "void r01_platformer_set_meter(R01GameCtx *ctx, int px_per_meter);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_events.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_EVENTS_H\n#define R01_EVENTS_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "typedef void (*R01EventFn)(R01GameCtx *ctx);\n"
                   "int r01_event_on_button(uint8_t btn, R01EventFn fn);\n"
                   "void r01_runtime_dispatch_buttons(R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_fade.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_FADE_H\n#define R01_FADE_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_FADE_BLACK 0\n"
                   "#define R01_FADE_WHITE 1\n"
                   "#define R01_FADE_MAX 255\n"
                   "void r01_game_fade_start(R01GameCtx *ctx, int to_black_or_white, int target_level);\n"
                   "int r01_game_fade_active(const R01GameCtx *ctx);\n"
                   "int r01_game_fade_tick(R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_warp.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_WARP_H\n#define R01_WARP_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "void r01_game_warp_to_tile(R01GameCtx *ctx, int screen_col, int screen_row, int tile_col,\n"
                   "                           int tile_row);\n"
                   "int r01_game_warp_by_id(R01GameCtx *ctx, const char *warp_id);\n"
                   "void r01_game_warp_check(R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_projectile.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_PROJECTILE_H\n#define R01_PROJECTILE_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_AIM_X_NONE 0\n"
                   "#define R01_AIM_X_RIGHT 1\n"
                   "#define R01_AIM_X_LEFT (-1)\n"
                   "#define R01_AIM_Y_NONE 0\n"
                   "#define R01_AIM_Y_DOWN 1\n"
                   "#define R01_AIM_Y_UP (-1)\n"
                   "#define R01_PROJ_SPEED_DEFAULT 3\n"
                   "#define R01_PROJ_SPEED_FAST 4\n"
                   "int r01_projectile_fire(R01GameCtx *ctx, int dx, int dy, int speed);\n"
                   "void r01_projectile_tick(R01GameCtx *ctx);\n"
                   "int r01_projectile_count_active(const R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), inc_dir, "r01_player_anim.h", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path,
                   "#ifndef R01_PLAYER_ANIM_H\n#define R01_PLAYER_ANIM_H\n\n"
                   "#include <stdint.h>\n"
                   "typedef struct R01GameCtx R01GameCtx;\n"
                   "#define R01_PLAYER_DIR_RIGHT 0\n"
                   "#define R01_PLAYER_DIR_DOWN_RIGHT 1\n"
                   "#define R01_PLAYER_DIR_DOWN 2\n"
                   "#define R01_PLAYER_DIR_DOWN_LEFT 3\n"
                   "#define R01_PLAYER_DIR_LEFT 4\n"
                   "#define R01_PLAYER_DIR_UP_LEFT 5\n"
                   "#define R01_PLAYER_DIR_UP 6\n"
                   "#define R01_PLAYER_DIR_UP_RIGHT 7\n"
                   "#define R01_PLAYER_FACE_RIGHT 0\n"
                   "#define R01_PLAYER_FACE_DOWN 1\n"
                   "#define R01_PLAYER_FACE_LEFT 2\n"
                   "#define R01_PLAYER_FACE_UP 3\n"
                   "void r01_player_anim_init(R01GameCtx *ctx);\n"
                   "void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx);\n"
                   "void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx);\n"
                   "void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx);\n"
                   "void r01_player_anim_set_crouch_state(R01GameCtx *ctx, int entity_state_idx);\n"
                   "void r01_player_anim_set_jump_state(R01GameCtx *ctx, int entity_state_idx);\n"
                   "void r01_player_default_face_set(R01GameCtx *ctx, int face);\n"
                   "void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks);\n"
                   "void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy);\n"
                   "void r01_player_anim_tick(R01GameCtx *ctx);\n"
                   "int r01_player_anim_entity_state(const R01GameCtx *ctx);\n"
                   "int r01_player_anim_frame(const R01GameCtx *ctx);\n"
                   "int r01_player_anim_flip_h(const R01GameCtx *ctx);\n"
                   "int r01_player_anim_moving(const R01GameCtx *ctx);\n\n"
                   "#endif\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }
    return 0;
}

static int write_custom_logic(const char *c_dir, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    struct stat st;
    if (join_path_err(path, sizeof(path), c_dir, "custom_logic.c", err_buf, err_cap) != 0) {

        return -1;

    }
    if (stat(path, &st) == 0) {
        return 0;
    }
    return write_text(path,
                      "/* User game logic. Created once by Studio export. Never overwritten. */\n"
                      "#include \"include/r01_engine.h\"\n\n"
                      "void r01_custom_on_init(R01GameCtx *ctx) {\n"
                      "    r01_camera_set_deadzone(ctx, R01_CAM_DEADZONE_X_DEFAULT, R01_CAM_DEADZONE_Y_DEFAULT);\n"
                      "    /* Examples:\n"
                      "     * r01_game_set_mode(ctx, R01_GAME_MODE_PLATFORMER);\n"
                      "     * r01_platformer_set_gravity(ctx, R01_PLAT_GRAVITY_DEFAULT);\n"
                      "     * r01_platformer_set_jump(ctx, R01_PLAT_JUMP_DEFAULT);\n"
                      "     * r01_platformer_set_meter(ctx, R01_PLAT_METER_DEFAULT);\n"
                      "     * r01_player_anim_set_idle_state(ctx, 0);\n"
                      "     * r01_player_anim_set_walk_all(ctx, 1);\n"
                      "     * r01_player_anim_set_crouch_state(ctx, 2);\n"
                      "     * r01_player_anim_set_jump_state(ctx, 3);\n"
                      "     * r01_bgm_play(ctx, 1);\n"
                      "     * r01_player_set_run_on_x(ctx);\n"
                      "     * r01_solid_pattern_add(ctx, 0, 1);\n"
                      "     * r01_camera_disable_deadzone(ctx); /* 1:1 camera track */\n"
                      "     * r01_bg0_set_wrap(ctx, R01_BG0_WRAP_ON, R01_BG0_WRAP_ON);\n"
                      "     * r01_bg0_set_clip_to_bg1(ctx, R01_BG0_CLIP_ON);\n"
                      "     * r01_projectile_fire(ctx, R01_AIM_X_RIGHT, R01_AIM_Y_NONE, R01_PROJ_SPEED_FAST);\n"
                      "     * r01_game_fade_start(ctx, R01_FADE_BLACK, R01_FADE_MAX);\n"
                      "     * r01_game_warp_by_id(ctx, \"w_00\");\n"
                      "     */\n"
                      "}\n\n"
                      "void r01_custom_on_tick(R01GameCtx *ctx) {\n"
                      "    (void)ctx;\n"
                      "}\n\n"
                      "void r01_custom_on_vblank(R01GameCtx *ctx) {\n"
                      "    (void)ctx;\n"
                      "}\n",
                      err_buf, err_cap);
}

static int write_warp_ids_header(const char *inc_dir, const R01World *w, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    FILE *f;
    int i;
    if (join_path_err(path, sizeof(path), inc_dir, "r01_warp_ids.h", err_buf, err_cap) != 0) {

        return -1;

    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write warp ids");
        return -1;
    }
    fprintf(f, "/* Generated warp entrance ids for custom_logic. */\n");
    fprintf(f, "#ifndef R01_WARP_IDS_H\n#define R01_WARP_IDS_H\n\n");
    if (w) {
        for (i = 0; i < w->warp_entrance_count; i++) {
            if (!w->warp_entrances[i].present) {
                continue;
            }
            fprintf(f, "#define R01_WARP_%s \"%s\"\n", w->warp_entrances[i].id,
                    w->warp_entrances[i].id);
        }
    }
    fprintf(f, "\n#endif\n");
    fclose(f);
    return 0;
}

static int write_base_game(FILE *f, const R01Project *p) {
    const R01World *w = p ? &p->worlds[0] : NULL;
    uint8_t mask[32];
    int spawn_c = R01_START_COL, spawn_r = R01_START_ROW;
    int pe;
    int i;
    int inst_spawn_x = -1;
    int inst_spawn_y = -1;

    fill_present_mask(mask, w);
    pick_spawn_screen(w, &spawn_c, &spawn_r);
    pe = w ? r01_world_player_entity(p) : -1;
    if (w && pe >= 0) {
        for (i = 0; i < w->instance_count; i++) {
            if (w->instances[i].type_id == pe) {
                inst_spawn_x = w->instances[i].world_x;
                inst_spawn_y = w->instances[i].world_y;
                break;
            }
        }
    }

    fprintf(f, "/* AUTO-GENERATED by Retr01 Studio export. Do not edit. */\n");
    fprintf(f, "#include \"include/r01_game.h\"\n#include \"include/r01_engine.h\"\n\n");
    fprintf(f, "typedef struct {\n");
    fprintf(f, "    int type_id;\n");
    fprintf(f, "    int world_x;\n");
    fprintf(f, "    int world_y;\n");
    fprintf(f, "} R01InstRec;\n\n");
    fprintf(f, "static const uint8_t play_present[32] = {");
    for (i = 0; i < 32; i++) {
        fprintf(f, "%s%u", i ? ", " : "", (unsigned)mask[i]);
    }
    fprintf(f, "};\n");
    fprintf(f, "static const uint8_t play_spawn_cell = 0x%02Xu;\n",
            (unsigned)R01_CELL_PACK(spawn_c, spawn_r));
    fprintf(f, "static const uint8_t play_spawn_col = %d;\n", spawn_c);
    fprintf(f, "static const uint8_t play_spawn_row = %d;\n", spawn_r);
    fprintf(f, "static const int player_entity = %d;\n", pe);
    if (w && w->instance_count > 0) {
        fprintf(f, "static const R01InstRec entity_insts[%d] = {\n", w->instance_count);
        for (i = 0; i < w->instance_count; i++) {
            fprintf(f, "    {%d, %d, %d}%s\n", w->instances[i].type_id, w->instances[i].world_x,
                    w->instances[i].world_y, i + 1 < w->instance_count ? "," : "");
        }
        fprintf(f, "};\n");
        fprintf(f, "static const int entity_inst_count = %d;\n", w->instance_count);
    } else {
        fprintf(f, "static const R01InstRec *entity_insts = 0;\n");
        fprintf(f, "static const int entity_inst_count = 0;\n");
    }
    if (inst_spawn_x >= 0) {
        fprintf(f, "static const int player_inst_x = %d;\n", inst_spawn_x);
        fprintf(f, "static const int player_inst_y = %d;\n", inst_spawn_y);
    }
    if (w && pe >= 0 && pe < p->entity_count && p->entities[pe].state_count > 0 &&
        p->entities[pe].states[0].frame_count > 0) {
        const R01EntityState *st0 = &p->entities[pe].states[0];
        fprintf(f, "static const int player_hit_x = %d;\n", st0->hitbox_x);
        fprintf(f, "static const int player_hit_y = %d;\n", st0->hitbox_y);
        fprintf(f, "static const int player_hit_w = %d;\n", st0->hitbox_w);
        fprintf(f, "static const int player_hit_h = %d;\n", st0->hitbox_h);
    } else {
        fprintf(f, "static const int player_hit_x = 0, player_hit_y = 0;\n");
        fprintf(f, "static const int player_hit_w = %d, player_hit_h = %d;\n", R01_PLAY_PLAYER_W,
                R01_PLAY_PLAYER_H);
    }
    {
        int si;
        fprintf(f, "const int player_state_frames[4] = {");
        for (si = 0; si < 4; si++) {
            int fc = 1;
            if (w && pe >= 0 && pe < p->entity_count && si < p->entities[pe].state_count) {
                fc = r01_entity_state_drawable_frame_count(&p->entities[pe].states[si]);
                if (fc < 1) {
                    fc = 1;
                }
            }
            fprintf(f, "%s%d", si ? ", " : "", fc);
        }
        fprintf(f, "};\n");
    }
    if (w && w->warp_entrance_count > 0) {
        int wi, wrote = 0;
        fprintf(f, "\ntypedef struct { const char *id; int sc, sr, tc, tr; } R01WarpEntRec;\n");
        fprintf(f, "typedef struct { int ent; int dsc, dsr, dtc, dtr; uint8_t flags; } R01WarpExitRec;\n");
        fprintf(f, "static const R01WarpEntRec warp_ents[] = {\n");
        for (wi = 0; wi < w->warp_entrance_count; wi++) {
            const R01WarpEntrance *we = &w->warp_entrances[wi];
            if (!we->present) {
                continue;
            }
            if (wrote) {
                fprintf(f, ",\n");
            }
            fprintf(f, "    {\"%s\", %d, %d, %d, %d}", we->id, we->screen_col, we->screen_row, we->tile_col,
                    we->tile_row);
            wrote++;
        }
        fprintf(f, "\n};\nstatic const int warp_ent_count = %d;\n", wrote);
        wrote = 0;
        fprintf(f, "static const R01WarpExitRec warp_exits[] = {\n");
        for (wi = 0; wi < w->warp_exit_count; wi++) {
            const R01WarpExit *wx = &w->warp_exits[wi];
            if (!wx->present) {
                continue;
            }
            if (wrote) {
                fprintf(f, ",\n");
            }
            fprintf(f, "    {%d, %d, %d, %d, %d, %u}", wx->entrance_idx, wx->dest_screen_col,
                    wx->dest_screen_row, wx->dest_tile_col, wx->dest_tile_row, (unsigned)wx->flags);
            wrote++;
        }
        fprintf(f, "\n};\nstatic const int warp_exit_count = %d;\n", wrote);
    } else {
        fprintf(f, "\ntypedef struct { const char *id; int sc, sr, tc, tr; } R01WarpEntRec;\n");
        fprintf(f, "typedef struct { int ent; int dsc, dsr, dtc, dtr; uint8_t flags; } R01WarpExitRec;\n");
        fprintf(f, "static const R01WarpEntRec warp_ents[1] = {{0}};\n");
        fprintf(f, "static const R01WarpExitRec warp_exits[1] = {{0}};\n");
        fprintf(f, "static const int warp_ent_count = 0;\nstatic const int warp_exit_count = 0;\n");
    }
    fprintf(f, "\nstatic int player_instance_spawn(int *out_x, int *out_y) {\n");
    fprintf(f, "    int i;\n");
    fprintf(f, "    if (player_entity < 0) return 0;\n");
    fprintf(f, "    for (i = 0; i < entity_inst_count; i++) {\n");
    fprintf(f, "        if (entity_insts[i].type_id != player_entity) continue;\n");
    fprintf(f, "        if (out_x) *out_x = entity_insts[i].world_x;\n");
    fprintf(f, "        if (out_y) *out_y = entity_insts[i].world_y;\n");
    fprintf(f, "        return 1;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void r01_game_init(R01GameCtx *ctx) {\n");
    fprintf(f, "    int sx, sy;\n");
    fprintf(f, "    (void)play_present;\n");
    fprintf(f, "    (void)player_hit_x; (void)player_hit_y;\n");
    fprintf(f, "    (void)player_hit_w; (void)player_hit_h;\n");
    fprintf(f, "    if (!ctx) return;\n");
    fprintf(f, "    r01_game_ctx_init(ctx);\n");
    {
        int pn = p->solid_pat_count;
        int pi;
        if (pn < 0) {
            pn = 0;
        }
        if (pn > R01_SOLID_PAT_MAX) {
            pn = R01_SOLID_PAT_MAX;
        }
        fprintf(f, "    ctx->solid_pat_count = %d;\n", pn);
        for (pi = 0; pi < pn; pi++) {
            fprintf(f, "    ctx->solid_pat_bank[%d] = %u;\n", pi, (unsigned)p->solid_pat_bank[pi]);
            fprintf(f, "    ctx->solid_pat_tile[%d] = %u;\n", pi, (unsigned)p->solid_pat_tile[pi]);
        }
    }
    fprintf(f, "    if (!player_instance_spawn(&sx, &sy)) {\n");
    fprintf(f, "        sx = play_spawn_col * %d + (%d - %d) / 2;\n", R01_SCREEN_PX_W, R01_SCREEN_PX_W,
            R01_PLAY_PLAYER_W);
    fprintf(f, "        sy = play_spawn_row * %d + (%d - %d) / 2;\n", R01_SCREEN_PX_H, R01_SCREEN_PX_H,
            R01_PLAY_PLAYER_H);
    fprintf(f, "    }\n");
    fprintf(f, "    ctx->player_x = sx;\n    ctx->player_y = sy;\n");
    fprintf(f, "    r01_custom_on_init(ctx);\n");
    fprintf(f, "    r01_game_camera_snap(ctx);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void r01_game_tick(R01GameCtx *ctx) {\n");
    fprintf(f, "    if (!ctx) return;\n");
    fprintf(f, "    r01_runtime_dispatch_buttons(ctx);\n");
    fprintf(f, "    ctx->pad_prev = ctx->pad;\n");
    fprintf(f, "    r01_custom_on_tick(ctx);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void r01_game_vblank(R01GameCtx *ctx) {\n");
    fprintf(f, "    if (!ctx) return;\n");
    fprintf(f, "    r01_custom_on_vblank(ctx);\n");
    fprintf(f, "}\n");
    return 0;
}

static int write_asm_tables(const char *asm_dir, const R01Project *p, const R01World *w, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    char tables_dir[R01_PATH_MAX];
    FILE *f;
    uint8_t mask[32];
    int spawn_c, spawn_r;
    int di = 0;
    int si;

    if (join_path_err(tables_dir, sizeof(tables_dir), asm_dir, "tables", err_buf, err_cap) != 0) {


        return -1;


    }
    fill_present_mask(mask, w);
    pick_spawn_screen(w, &spawn_c, &spawn_r);

    if (join_path_err(path, sizeof(path), tables_dir, "play_present.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    fprintf(f, "; screen present mask (32 bytes @ PLAY+$00, 16 rows x u16 LE)\nplay_present:\n        .byte ");
    for (si = 0; si < 32; si++) {
        fprintf(f, "%s$%02X", si ? ", " : "", mask[si]);
    }
    fprintf(f, "\n");
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "play_spawn.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    fprintf(f, "play_spawn_cell: .byte $%02X ; col|(row<<4)\n",
            (unsigned)R01_CELL_PACK(spawn_c, spawn_r));
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "play_coll_dir.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    fprintf(f, "; collision directory: col, row, tab_lo, tab_hi per present screen\n");
    {
        int pat_n = p ? p->solid_pat_count : 0;
        size_t data_off;
        if (pat_n < 0) {
            pat_n = 0;
        }
        if (pat_n > R01_SOLID_PAT_MAX) {
            pat_n = R01_SOLID_PAT_MAX;
        }
        data_off = 1u + (size_t)pat_n * 2u;
        if (w) {
            for (si = 0; si < w->screen_count; si++) {
                const R01Screen *s = &w->screens[si];
                uint16_t tab_addr;
                if (!s->present || s->col < 0 || s->col >= R01_GRID_MAX || s->row < 0 || s->row >= R01_GRID_MAX) {
                    continue;
                }
                tab_addr = (uint16_t)(0x8700u + data_off);
                fprintf(f, "        .byte $%02X, $%02X, $%02X, $%02X  ; screen (%d,%d)\n", s->col & 0xFF,
                        s->row & 0xFF, tab_addr & 0xFF, (tab_addr >> 8) & 0xFF, s->col, s->row);
                data_off += R01_TILES_PER_SCREEN;
                di++;
            }
        }
    }
    fprintf(f, "play_coll_count: .byte $%02X\n", di & 0xFF);
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "play_solid_data.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    fprintf(f, "; solid pattern list @ CPU $8700 (count + bank,tile pairs), then probe bytes\n");
    fprintf(f, "; boot copies the list to system RAM $0200\n");
    {
        int pat_n = p ? p->solid_pat_count : 0;
        int pi;
        if (pat_n < 0) {
            pat_n = 0;
        }
        if (pat_n > R01_SOLID_PAT_MAX) {
            pat_n = R01_SOLID_PAT_MAX;
        }
        fprintf(f, "        .byte $%02X\n", pat_n & 0xFF);
        for (pi = 0; pi < pat_n; pi++) {
            fprintf(f, "        .byte $%02X, $%02X\n", p->solid_pat_bank[pi], p->solid_pat_tile[pi]);
        }
    }
    if (w) {
        for (si = 0; si < w->screen_count; si++) {
            const R01Screen *s = &w->screens[si];
            int cell;
            if (!s->present) {
                continue;
            }
            fprintf(f, "; screen (%d,%d)\n", s->col, s->row);
            for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
                int on = r01_project_pattern_solid(p, r01_attr_solid_bank(s->attrs[cell]), (int)s->tiles[cell]);
                fprintf(f, "        .byte $%02X\n", on ? 1 : 0);
            }
        }
    }
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "player_entity.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    if (w) {
        int pe = r01_world_player_entity(p);
        if (pe >= 0 && pe < p->entity_count && p->entities[pe].state_count > 0 &&
            p->entities[pe].states[0].frame_count > 0) {
            const R01EntityState *st0 = &p->entities[pe].states[0];
            fprintf(f, "player_entity_id: .byte $%02X\n", pe & 0xFF);
            fprintf(f, "player_hit_x: .byte $%02X\n", st0->hitbox_x & 0xFF);
            fprintf(f, "player_hit_y: .byte $%02X\n", st0->hitbox_y & 0xFF);
            fprintf(f, "player_hit_w: .byte $%02X\n", st0->hitbox_w & 0xFF);
            fprintf(f, "player_hit_h: .byte $%02X\n", st0->hitbox_h & 0xFF);
        } else {
            fprintf(f, "player_entity_id: .byte $FF\n");
        }
    }
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "entity_types.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    fprintf(f, "; packed entity type records (cart export mirrors cart.c)\n");
    if (w) {
        for (si = 0; si < p->entity_count; si++) {
            fprintf(f, "; type %d: %s\n", si, p->entities[si].name);
        }
    }
    fclose(f);

    if (join_path_err(path, sizeof(path), tables_dir, "entity_insts.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write tables");
        return -1;
    }
    if (w) {
        for (si = 0; si < w->instance_count; si++) {
            const R01EntityInstance *in = &w->instances[si];
            fprintf(f, "        .byte $%02X, $%02X, $%02X, $%02X, $%02X, $%02X\n", in->type_id & 0xFF,
                    (in->flip_h ? 1 : 0) | (in->flip_v ? 2 : 0), in->world_x & 0xFF,
                    (in->world_x >> 8) & 0xFF, in->world_y & 0xFF, (in->world_y >> 8) & 0xFF);
        }
    }
    fclose(f);
    return 0;
}

static int write_asm_tree(const char *asm_dir, const R01Project *p, const R01World *w, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    const char *src_collision;

    if (write_asm_tables(asm_dir, p, w, err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "include/r01_hw.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "; Retr01 I/O map (CPU view)\n"
                   "PPUCTRL       = $7F00\nPPUSTATUS     = $7F01\n"
                   "SCROLL_X      = $7F02\nSCROLL_Y      = $7F03\n"
                   "BG0_SCROLL_X  = $7F06\nBG0_SCROLL_Y  = $7F07\n"
                   "PAL_ROW       = $7F08\nPAL_DATA      = $7F09\n"
                   "VRAM_ADDR_LO  = $7F10\nVRAM_ADDR_HI  = $7F11\nVRAM_DATA = $7F12\n"
                   "OAM_ADDR      = $7F20\nOAM_DATA      = $7F21\n"
                   "CARTEE_CMD    = $7F22\nCARTEE_ADDR   = $7F23\nCARTEE_DATA   = $7F24\n"
                   "WORLD         = $7F30\n"
                   "PAD0          = $7F60\nPAD1          = $7F61\n"
                   "MEEPROM_AL    = $7F70\nMEEPROM_AH    = $7F71\nMEEPROM_DATA  = $7F72\n"
                   "MAP_LO        = $7F90\nMAP_MID       = $7F91\nMAP_HI        = $7F92\n"
                   "MAP_CTRL      = $7F93\n"
                   "PPUCTRL_L1_EN = $01\nPPUCTRL_L0_EN = $02\nPPUCTRL_SPR_EN = $04\n"
                   "PPUCTRL_NMI_EN = $80\nPPUCTRL_BOOT  = $07\n"
                   "CARTEE_CMD_READ  = $80\nCARTEE_CMD_WRITE = $40\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "include/r01_play.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "PLAY_BASE       = $8100\n"
                   "PLAY_PRESENT    = $8100\n"
                   "PLAY_SPAWN_CELL = $8120\n"
                   "PLAY_COLL_COUNT = $8121\n"
                   "PLAY_COLL_DIR   = $8122\n"
                   "PLAY_INST_COUNT = $81C0\n"
                   "PLAY_INST_TABLE = $81C1\n"
                   "PLAY_SOLID_RAM  = $0200\n"
                   "PLAY_SOLID_LIST = $8700\n"
                   "R01P_MARKER     = $80F0\n"
                   "PLAT_GRAVITY    = $80F7\n"
                   "PLAT_JUMP       = $80F8\n"
                   "PLAT_METER      = $80F9\n"
                   "PLAT_CROUCH     = $80FA\n"
                   "PLAYER_ANIM_IDLE = $80FB\n"
                   "PLAYER_ANIM_WALK = $80FC\n"
                   "PLAYER_ANIM_JUMP = $80FD\n"
                   "BGM_BOOT         = $80FE\n"
                   "BGM_BASE         = $B000\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "include/r01_cart.inc", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "SCREEN_TILES_X = 16\nSCREEN_TILES_Y = 15\n"
                   "SCREEN_PX_W    = 128\nSCREEN_PX_H    = 120\n"
                   "OAM_MAX        = 64\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "main.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "; Retr01 game PRG entry - include chain for ca65 (Phase 5C).\n"
                   ".setcpu \"65c02\"\n"
                   ".include \"include/r01_hw.inc\"\n"
                   ".include \"include/r01_play.inc\"\n"
                   ".include \"include/r01_cart.inc\"\n"
                   ".include \"boot/reset.s\"\n"
                   ".include \"boot/boot_stream.s\"\n"
                   ".include \"game/init.s\"\n"
                   ".include \"game/main_loop.s\"\n"
                   ".include \"collision/play_collision.s\"\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "game.cfg", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "MEMORY {\n"
                   "    PRG: start = $8000, size = $8000, fill = yes, fillval = $EA;\n"
                   "}\n"
                   "SEGMENTS {\n"
                   "    CODE: load = PRG, type = ro;\n"
                   "    PLAY: load = PRG, type = ro, start = $8100;\n"
                   "    VECTORS: load = PRG, type = ro, start = $FFFA;\n"
                   "}\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "boot/reset.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "WORLD     = $7F30\n"
                   ".segment \"CODE\"\n.org $8000\n"
                   "reset:\n        sei\n        cld\n        ldx #$ff\n        txs\n"
                   "        lda #0\n        sta WORLD\n        sta SCROLL_X\n        sta SCROLL_Y\n"
                   "        lda #PPUCTRL_BOOT\n        sta PPUCTRL\n"
                   "        jmp boot_stream\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "boot/vectors.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   ".segment \"VECTORS\"\n.org $FFFA\n"
                   "        .word main\n        .word reset\n        .word main\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "boot/boot_stream.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "; Palette + MAP boot stream - bytes patched by Studio packer (prg_phase1.c).\n"
                   "boot_stream:\n        jmp main\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "game/init.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "init_game:\n        .include \"../tables/play_present.inc\"\n"
                   "        rts\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "game/main_loop.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "main:\n        lda PPUSTATUS\n        and #$80\n        beq main\n"
                   "        jsr tick_frame\n        jsr vblank_frame\n        jmp main\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "game/tick.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path,
                   "tick_frame:\n        lda PAD0\n        sta $00FE\n        rts\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "game/vblank.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "vblank_frame:\n        rts\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "io/pad.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "; pad read helpers (Phase 5D+)\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "io/scroll.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; scroll latch ($7F02/$7F03)\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "io/oam_port.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; OAM port ($7F20/$7F21)\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "io/map_port.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; MAP port ($7F90-$7F93)\n", err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "player/spawn.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "; first player instance -> origin\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "player/move.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; axis movement + play_pos_ok\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "player/warp.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; pad Y jump (platformer). No X/Y face-button warps.\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "player/camera.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; center camera on player\n", err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "sprite/oam_clear.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "; memset OAM slots to $FF\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "sprite/oam_build.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; player state0/frame0 + instances\n", err_buf, err_cap) != 0) {
        return -1;
    }
    if (join_path_err(path, sizeof(path), asm_dir, "sprite/oam_blit.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (write_text(path, "; write one 8x8 OAM entry\n", err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "collision/solid_probe.s", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "; tile solid lookup via shadow table\n", err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), asm_dir, "link/prg.layout", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_text(path, "# Host packer segment layout (see prg_phase1.c)\nCODE=$8000\nPLAY=$8100\n",
                   err_buf, err_cap) != 0) {
        return -1;
    }

    src_collision = R01_PKG_ROOT "/studio/core/asm/play_collision.s";
    if (join_path_err(path, sizeof(path), asm_dir, "collision/play_collision.s", err_buf, err_cap) != 0) {

        return -1;

    }
    if (copy_file(src_collision, path, err_buf, err_cap) != 0) {
        return -1;
    }
    return 0;
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
        if (!s->present) {
            continue;
        }
        memcpy(map, s->tiles, R01_TILES_PER_SCREEN);
        memcpy(map + R01_TILES_PER_SCREEN, s->attrs, R01_ATTRS_PER_SCREEN);
        {
            char leaf[64];
            snprintf(leaf, sizeof(leaf), "map_screen_%d_%d.bin", s->col, s->row);
            if (join_path_err(path, sizeof(path), data_dir, leaf, err_buf, err_cap) != 0) {
                return -1;
            }
        }
        if (write_bytes(path, map, sizeof(map), err_buf, err_cap) != 0) {
            return -1;
        }
    }
    return 0;
}

static int write_runtime_c(const char *c_dir, char *err_buf, size_t err_cap) {
    char path[R01_PATH_MAX];
    char src[R01_PATH_MAX];
    if (join_path_err(path, sizeof(path), c_dir, "r01_runtime.c", err_buf, err_cap) != 0) {

        return -1;

    }
    snprintf(src, sizeof(src), "%s/studio/core/src/export_runtime.c", R01_PKG_ROOT);
    if (copy_file(src, path, err_buf, err_cap) != 0) {
        return write_text(path,
                          "/* Generated runtime stubs for custom_logic (host-side). */\n"
                          "#include \"include/r01_engine.h\"\n\n"
                          "void r01_game_ctx_init(R01GameCtx *ctx) {\n"
                          "    if (!ctx) return;\n"
                          "    ctx->cam_deadzone_x = R01_CAM_DEADZONE_X_DEFAULT;\n"
                          "    ctx->cam_deadzone_y = R01_CAM_DEADZONE_Y_DEFAULT;\n"
                          "    ctx->cam_axis_lock = R01_CAM_AXIS_BOTH;\n"
                          "    ctx->fade_color = R01_FADE_BLACK;\n"
                          "    ctx->fade_pending_entrance = -1;\n"
                          "}\n",
                          err_buf, err_cap);
    }
    return 0;
}

int r01_export_codegen(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap) {
    char out_dir[R01_PATH_MAX];
    char base_name[64];
    char path[R01_PATH_MAX];
    FILE *f;
    const R01World *w;

    if (!p || !path_stem) {
        set_err(err_buf, err_cap, "bad args");
        return -1;
    }
    split_stem(path_stem, out_dir, sizeof(out_dir), base_name, sizeof(base_name));
    w = &p->worlds[0];

    if (ensure_codegen_dirs(out_dir, err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), out_dir, "C/include", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_headers(path, err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_warp_ids_header(path, w, err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), out_dir, "C", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_custom_logic(path, err_buf, err_cap) != 0) {
        return -1;
    }
    if (write_runtime_c(path, err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), out_dir, "C/base_game.c", err_buf, err_cap) != 0) {


        return -1;


    }
    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    f = fopen(path, "w");
    if (!f) {
        set_err(err_buf, err_cap, "cannot write base_game.c");
        return -1;
    }
    if (write_base_game(f, p) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);

    if (join_path_err(path, sizeof(path), out_dir, "ASM", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_asm_tree(path, p, w, err_buf, err_cap) != 0) {
        return -1;
    }

    if (join_path_err(path, sizeof(path), out_dir, "data", err_buf, err_cap) != 0) {


        return -1;


    }
    if (write_data_bins(path, p, err_buf, err_cap) != 0) {
        return -1;
    }

    (void)base_name;
    return 0;
}

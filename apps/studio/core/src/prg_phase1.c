#include "retr01_studio/play.h"
#include "retr01_studio/prg_phase1.h"

#include "r01_hw_regs.h"
#include "retr01_studio/cart.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CODE_BASE 0x8000u
#define PLAY_OFF 0x0100u /* PRG+$0100 -> CPU $8100 */

#define PLAY_PRESENT 0
#define PLAY_PRESENT_BYTES 32
#define PLAY_SPAWN_CELL 32
#define PLAY_INST_COUNT 0xC0u
#define PLAY_INST_TABLE 0xC1u

#define R01P_OFF 0x00F0u
#define R01_PLAY_SOLID_DATA_OFF 0x0700u

static size_t s_solids_end;

#ifndef R01_REPO_ROOT
#define R01_REPO_ROOT "."
#endif

static void put_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint32_t pal_row_off(uint32_t plane_off, uint32_t plane_len, unsigned row) {
    unsigned r = row & 7u;
    if (plane_len == 0 || plane_len >= R01_PAL_PLANE_BYTES) {
        return plane_off + r * 16u;
    }
    return plane_off;
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

static void pick_spawn(const R01World *w, int *col, int *row) {
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

static void put_boot_u24(uint8_t prg[R01_PRG_BYTES], unsigned off, uint32_t v) {
    prg[R01_PRG_BOOTMAP_OFF + off] = (uint8_t)(v & 0xFFu);
    prg[R01_PRG_BOOTMAP_OFF + off + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    prg[R01_PRG_BOOTMAP_OFF + off + 2u] = (uint8_t)((v >> 16) & 0xFFu);
}

static void patch_boot_map(uint8_t prg[R01_PRG_BYTES], const R01PrgCartLayout *layout) {
    uint32_t off_bg = 0;
    uint32_t off_spr = 0;
    uint32_t off_map = 0;
    uint32_t off_bgm = 0;
    uint32_t off_world = 0;
    if (!prg) {
        return;
    }
    if (layout && layout->off_pal_bg) {
        off_bg = pal_row_off(layout->off_pal_bg, layout->len_pal_bg, layout->default_pal_row);
        off_spr = layout->off_pal_spr
                      ? pal_row_off(layout->off_pal_spr, layout->len_pal_spr, layout->default_pal_row)
                      : off_bg + 16u;
        off_map = layout->off_map_screen0;
        off_bgm = layout->off_bgm;
        off_world = layout->off_world0;
    }
    put_boot_u24(prg, 0, off_bg);
    put_boot_u24(prg, 3, off_spr);
    put_boot_u24(prg, 6, off_map);
    put_boot_u24(prg, 9, off_bgm);
    put_boot_u24(prg, 12, off_world);
    prg[R01_PRG_BOOTMAP_OFF + 15u] = 0;
}

static void fill_play_block(uint8_t prg[R01_PRG_BYTES], size_t play_off, const R01World *w) {
    uint8_t mask[PLAY_PRESENT_BYTES];
    int spawn_c = R01_START_COL, spawn_r = R01_START_ROW;
    int n = 0;
    int i;
    size_t base = play_off + PLAY_INST_TABLE;
    size_t limit = play_off + R01_PRG_PLAY_TAB_BYTES;

    fill_present_mask(mask, w);
    memcpy(prg + play_off + PLAY_PRESENT, mask, PLAY_PRESENT_BYTES);
    pick_spawn(w, &spawn_c, &spawn_r);
    prg[play_off + PLAY_SPAWN_CELL] = R01_CELL_PACK(spawn_c, spawn_r);
    prg[play_off + PLAY_INST_COUNT] = 0;
    if (!w) {
        return;
    }
    n = w->instance_count;
    if (n > R01_MAX_ENTITY_INSTANCES) {
        n = R01_MAX_ENTITY_INSTANCES;
    }
    if (n > 255) {
        n = 255;
    }
    if (base + (size_t)n * R01_CART_INSTANCE_SIZE > limit) {
        n = (int)((limit - base) / R01_CART_INSTANCE_SIZE);
    }
    for (i = 0; i < n; i++) {
        uint8_t *rec = prg + base + (size_t)i * R01_CART_INSTANCE_SIZE;
        const R01EntityInstance *inst = &w->instances[i];
        rec[0] = (uint8_t)inst->type_id;
        rec[1] = (uint8_t)((inst->flip_h ? 1u : 0u) | (inst->flip_v ? 2u : 0u));
        put_u16_le(rec + 2, (uint16_t)inst->world_x);
        put_u16_le(rec + 4, (uint16_t)inst->world_y);
    }
    prg[play_off + PLAY_INST_COUNT] = (uint8_t)n;
}

static void fill_solid_list(uint8_t prg[R01_PRG_BYTES], const R01Project *p) {
    size_t data_off = R01_PLAY_SOLID_DATA_OFF;
    int n;
    int i;

    n = p ? p->solid_pat_count : 0;
    if (n < 0) {
        n = 0;
    }
    if (n > R01_SOLID_PAT_MAX) {
        n = R01_SOLID_PAT_MAX;
    }
    if (data_off + 1u + (size_t)n * 2u > R01_PLAY_SOLID_DATA_OFF + R01_PRG_SOLIDS_MAX) {
        prg[data_off] = 0;
        s_solids_end = data_off + 1u;
        return;
    }
    prg[data_off++] = (uint8_t)n;
    for (i = 0; i < n; i++) {
        prg[data_off++] = p->solid_pat_bank[i];
        prg[data_off++] = p->solid_pat_tile[i];
    }
    s_solids_end = data_off;
}

void r01_prg_fill_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p) {
    const R01World *w0 = p ? &p->worlds[0] : NULL;
    unsigned wi;
    unsigned extra = 0;

    if (!prg) {
        return;
    }
    fill_play_block(prg, PLAY_OFF, w0);
    fill_solid_list(prg, p);

    prg[R01P_OFF] = 'R';
    prg[R01P_OFF + 1] = '0';
    prg[R01P_OFF + 2] = '1';
    prg[R01P_OFF + 3] = 'P';
    prg[R01P_OFF + 4] = R01_PRG_R01P_VER;
    put_u16_le(prg + R01P_OFF + 5, (uint16_t)(CODE_BASE + R01_PLAY_SOLID_DATA_OFF));

    memset(prg + R01_PRG_WORLDDIR_OFF, 0, R01_PRG_WORLDDIR_BYTES);
    put_u16_le(prg + R01_PRG_WORLDDIR_OFF, (uint16_t)(CODE_BASE + PLAY_OFF));
    for (wi = 1; wi < (unsigned)R01_MAX_WORLDS; wi++) {
        const R01World *w = p ? &p->worlds[wi] : NULL;
        if (!w || !w->present) {
            continue;
        }
        {
            size_t play_off = R01_PRG_WPLAY_OFF + (size_t)extra * R01_PRG_PLAY_TAB_BYTES;
            uint16_t cpu = (uint16_t)(R01_PRG_WPLAY_CPU + extra * R01_PRG_PLAY_TAB_BYTES);
            fill_play_block(prg, play_off, w);
            put_u16_le(prg + R01_PRG_WORLDDIR_OFF + (size_t)wi * 2u, cpu);
            extra++;
        }
    }
}

void r01_prg_patch_boot_map(uint8_t prg[R01_PRG_BYTES], const R01PrgCartLayout *layout) {
    patch_boot_map(prg, layout);
}

void r01_prg_overlay_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout) {
    r01_prg_fill_tables(prg, p);
    patch_boot_map(prg, layout);
}

static int write_bin_if_changed(const char *path, const uint8_t *data, size_t len, char *err_buf, size_t err_cap) {
    FILE *f;
    struct stat st;
    if (!path || !data) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "bad args");
        }
        return -1;
    }
    if (r01_path_ensure_parent(path, err_buf, err_cap) != 0) {
        return -1;
    }
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (size_t)st.st_size == len) {
        uint8_t *old = (uint8_t *)malloc(len ? len : 1u);
        if (old) {
            size_t n = 0;
            f = fopen(path, "rb");
            if (f) {
                n = fread(old, 1, len, f);
                fclose(f);
            }
            if (n == len && memcmp(old, data, len) == 0) {
                free(old);
                return 0;
            }
            free(old);
        }
    }
    f = fopen(path, "wb");
    if (!f) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "cannot write %s", path);
        }
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, f) != len) {
        fclose(f);
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "write failed");
        }
        return -1;
    }
    fclose(f);
    return 0;
}

int r01_prg_write_table_bins(const R01Project *p, const char *data_dir, char *err_buf, size_t err_cap) {
    uint8_t *prg;
    char path[R01_PATH_MAX];
    size_t solids_len;
    int rc = -1;

    if (!data_dir || !data_dir[0]) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "bad args");
        }
        return -1;
    }
    if (r01_path_mkdir_p(data_dir, err_buf, err_cap) != 0) {
        return -1;
    }
    prg = (uint8_t *)malloc(R01_PRG_BYTES);
    if (!prg) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "oom");
        }
        return -1;
    }
    memset(prg, 0, R01_PRG_BYTES);
    r01_prg_fill_tables(prg, p);
    solids_len = s_solids_end > R01_PLAY_SOLID_DATA_OFF ? s_solids_end - R01_PLAY_SOLID_DATA_OFF : 1u;
    if (solids_len > R01_PRG_SOLIDS_MAX) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "solid list hits $8800");
        }
        free(prg);
        return -1;
    }
    if (snprintf(path, sizeof(path), "%s/play8100.bin", data_dir) >= (int)sizeof(path) ||
        write_bin_if_changed(path, prg + PLAY_OFF, R01_PRG_PLAY_TAB_BYTES, err_buf, err_cap) != 0) {
        goto done;
    }
    if (snprintf(path, sizeof(path), "%s/worlddir.bin", data_dir) >= (int)sizeof(path) ||
        write_bin_if_changed(path, prg + R01_PRG_WORLDDIR_OFF, R01_PRG_WORLDDIR_BYTES, err_buf, err_cap) != 0) {
        goto done;
    }
    if (snprintf(path, sizeof(path), "%s/solids.bin", data_dir) >= (int)sizeof(path) ||
        write_bin_if_changed(path, prg + R01_PLAY_SOLID_DATA_OFF, solids_len, err_buf, err_cap) != 0) {
        goto done;
    }
    if (snprintf(path, sizeof(path), "%s/r01p.bin", data_dir) >= (int)sizeof(path) ||
        write_bin_if_changed(path, prg + R01P_OFF, R01_PRG_R01P_BYTES, err_buf, err_cap) != 0) {
        goto done;
    }
    {
        unsigned extra = 0;
        unsigned wi;
        for (wi = 1; p && wi < (unsigned)R01_MAX_WORLDS; wi++) {
            if (p->worlds[wi].present) {
                extra++;
            }
        }
        if (snprintf(path, sizeof(path), "%s/wplay.bin", data_dir) >= (int)sizeof(path)) {
            goto done;
        }
        if (extra > 0) {
            if (write_bin_if_changed(path, prg + R01_PRG_WPLAY_OFF, (size_t)extra * R01_PRG_PLAY_TAB_BYTES, err_buf,
                                     err_cap) != 0) {
                goto done;
            }
        } else {
            uint8_t z = 0;
            if (write_bin_if_changed(path, &z, 0, err_buf, err_cap) != 0) {
                goto done;
            }
        }
    }
    rc = 0;
done:
    free(prg);
    return rc;
}

void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout) {
    if (!prg) {
        return;
    }
    memset(prg, 0, R01_PRG_BYTES);
    r01_prg_fill_tables(prg, p);
    patch_boot_map(prg, layout);
}

static int data_bins_newer(const char *data_dir, time_t mt) {
    static const char *const names[] = {"play8100.bin", "worlddir.bin", "solids.bin", "r01p.bin", "wplay.bin", NULL};
    struct stat st;
    unsigned i;
    if (!data_dir || !data_dir[0]) {
        return 0;
    }
    for (i = 0; names[i]; i++) {
        char path[R01_PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", data_dir, names[i]) >= (int)sizeof(path)) {
            return 1;
        }
        if (stat(path, &st) == 0 && st.st_mtime > mt) {
            return 1;
        }
    }
    return 0;
}

static void split_dir(const char *path, char *out, size_t cap);

int r01_prg_needs_rebuild(const char *prg_path, const char *logic_c) {
    struct stat st;
    time_t mt;
    static const char *const deps[] = {
        "/apps/sdk/r01_c/build-prg.sh",
        "/apps/sdk/r01_c/ld/retr01.ld",
        "/apps/sdk/r01_c/asm/boot.s",
        "/apps/sdk/r01_c/asm/nmi.s",
        "/apps/sdk/r01_c/asm/map_copy.s",
        "/apps/sdk/r01_c/src/hw.c",
        "/apps/sdk/r01_c/src/boot.c",
        "/apps/sdk/r01_c/src/game.c",
        "/apps/sdk/r01_c/src/play_tick.c",
        "/apps/sdk/r01_c/src/tracker.c",
        "/apps/sdk/r01_c/src/map_win.c",
        "/apps/sdk/r01_c/src/oam_pa.c",
        "/apps/sdk/r01_c/src/main.c",
        "/apps/sdk/r01_c/include/r01_hw.h",
        "/apps/sdk/r01_c/include/r01_engine.h",
        "/apps/sdk/r01_c/include/r01_game.h",
        "/apps/sdk/r01_c/include/r01_player.h",
        "/apps/sdk/r01_c/include/r01_player_anim.h",
        "/apps/sdk/r01_c/include/r01_camera.h",
        "/apps/sdk/r01_c/include/r01_physics.h",
        "/apps/sdk/r01_c/include/r01_entity.h",
        "/apps/sdk/r01_c/include/r01_entity_ids.h",
        "/apps/sdk/r01_c/include/r01_warp_ids.h",
        "/apps/common/r01_play_camera.c",
        "/apps/common/r01_play_camera.h",
        "/apps/common/r01_play_physics.c",
        "/apps/common/r01_play_physics.h",
        "/apps/common/r01_play_collision.c",
        "/apps/common/r01_play_collision.h",
        "/apps/common/r01_play_anim.c",
        "/apps/common/r01_play_anim.h",
        "/apps/sdk/r01_c/data/play8100.bin",
        "/apps/sdk/r01_c/data/worlddir.bin",
        "/apps/sdk/r01_c/data/solids.bin",
        "/apps/sdk/r01_c/data/r01p.bin",
        "/tools/llvm-mos/bin/mos-common-clang",
        NULL
    };
    unsigned i;

    if (!prg_path || stat(prg_path, &st) != 0 || !S_ISREG(st.st_mode) || (size_t)st.st_size != R01_PRG_BYTES) {
        return 1;
    }
    mt = st.st_mtime;
    if (logic_c && stat(logic_c, &st) == 0 && st.st_mtime > mt) {
        return 1;
    }
    if (logic_c) {
        char dir[1024];
        char hdr[1100];
        split_dir(logic_c, dir, sizeof(dir));
        if (snprintf(hdr, sizeof(hdr), "%s/include/r01_entity_ids.h", dir) < (int)sizeof(hdr) &&
            stat(hdr, &st) == 0 && st.st_mtime > mt) {
            return 1;
        }
        if (snprintf(hdr, sizeof(hdr), "%s/include/r01_warp_ids.h", dir) < (int)sizeof(hdr) &&
            stat(hdr, &st) == 0 && st.st_mtime > mt) {
            return 1;
        }
        if (snprintf(hdr, sizeof(hdr), "%s/data", dir) < (int)sizeof(hdr) && data_bins_newer(hdr, mt)) {
            return 1;
        }
    }
    {
        char dir[1024];
        char data[1100];
        split_dir(prg_path, dir, sizeof(dir));
        if (snprintf(data, sizeof(data), "%s/data", dir) < (int)sizeof(data) && data_bins_newer(data, mt)) {
            return 1;
        }
    }
    for (i = 0; deps[i]; i++) {
        char path[1024];
        if (snprintf(path, sizeof(path), "%s%s", R01_REPO_ROOT, deps[i]) >= (int)sizeof(path)) {
            return 1;
        }
        if (stat(path, &st) == 0 && st.st_mtime > mt) {
            return 1;
        }
    }
    return 0;
}

static int file_size(const char *path, size_t *out) {
    struct stat st;
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    if (out) {
        *out = (size_t)st.st_size;
    }
    return 0;
}

static int read_prg_file(const char *path, uint8_t prg[R01_PRG_BYTES]) {
    FILE *f;
    size_t n;
    if (!path || !prg) {
        return -1;
    }
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    n = fread(prg, 1, R01_PRG_BYTES, f);
    fclose(f);
    return n == R01_PRG_BYTES ? 0 : -1;
}

static void split_dir(const char *path, char *out, size_t cap) {
    const char *slash;
    if (!out || cap < 2) {
        return;
    }
    if (!path || !path[0]) {
        out[0] = '.';
        out[1] = '\0';
        return;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        out[0] = '.';
        out[1] = '\0';
        return;
    }
    if (slash == path) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }
    {
        size_t n = (size_t)(slash - path);
        if (n >= cap) {
            n = cap - 1;
        }
        memcpy(out, path, n);
        out[n] = '\0';
    }
}

int r01_prg_compile_sdk(const char *logic_c, uint8_t prg[R01_PRG_BYTES], const char *out_prg_path, char *err_buf,
                        size_t err_cap) {
    char cmd[2048];
    char tmp[1024];
    char data[R01_PATH_MAX];
    char dir[1024];
    char cand[R01_PATH_MAX];
    const char *outp = out_prg_path;
    int st;
    size_t sz = 0;
    struct stat stb;

    if (!prg) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "bad args");
        }
        return -1;
    }
    if (!logic_c || !logic_c[0]) {
        logic_c = R01_REPO_ROOT "/apps/sdk/r01_c/game_logic.c";
    }
    if (!outp || !outp[0]) {
        snprintf(tmp, sizeof(tmp), "/tmp/retr01_sdk_%d.prg", (int)getpid());
        outp = tmp;
    }
    if (r01_path_ensure_parent(outp, err_buf, err_cap) != 0) {
        return -1;
    }
    split_dir(outp, dir, sizeof(dir));
    if (snprintf(cand, sizeof(cand), "%s/data/play8100.bin", dir) < (int)sizeof(cand) && stat(cand, &stb) == 0) {
        snprintf(data, sizeof(data), "%s/data", dir);
    } else {
        split_dir(logic_c, dir, sizeof(dir));
        if (snprintf(cand, sizeof(cand), "%s/data/play8100.bin", dir) < (int)sizeof(cand) && stat(cand, &stb) == 0) {
            snprintf(data, sizeof(data), "%s/data", dir);
        } else {
            snprintf(data, sizeof(data), "%s/apps/sdk/r01_c/data", R01_REPO_ROOT);
        }
    }
    if (snprintf(cmd, sizeof(cmd), "\"%s/apps/sdk/r01_c/build-prg.sh\" \"%s\" \"%s\" \"%s\"", R01_REPO_ROOT, logic_c,
                 outp, data) >= (int)sizeof(cmd)) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "compile command too long");
        }
        return -1;
    }
    st = system(cmd);
    if (st == -1 || !WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "llvm-mos missing or PRG build failed. Run ./scripts/fetch-llvm-mos.sh");
        }
        return -1;
    }
    if (file_size(outp, &sz) != 0 || sz != R01_PRG_BYTES) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "PRG size %zu (want %u)", sz, (unsigned)R01_PRG_BYTES);
        }
        return -1;
    }
    return read_prg_file(outp, prg);
}

int r01_prg_load_or_compile(const char *cart_path, uint8_t prg[R01_PRG_BYTES], char *err_buf, size_t err_cap) {
    char dir[R01_PATH_MAX];
    char logic[R01_PATH_MAX];
    char built[R01_PATH_MAX];
    struct stat st;

    split_dir(cart_path, dir, sizeof(dir));
    if (snprintf(logic, sizeof(logic), "%s/game_logic.c", dir) >= (int)sizeof(logic)) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "path too long");
        }
        return -1;
    }
    if (stat(logic, &st) != 0) {
        snprintf(logic, sizeof(logic), "%s/apps/sdk/r01_c/game_logic.c", R01_REPO_ROOT);
    }
    if (snprintf(built, sizeof(built), "%s/retr01.prg", dir) >= (int)sizeof(built)) {
        if (err_buf && err_cap) {
            snprintf(err_buf, err_cap, "path too long");
        }
        return -1;
    }
    if (!r01_prg_needs_rebuild(built, logic)) {
        if (read_prg_file(built, prg) == 0) {
            return 0;
        }
    }
    return r01_prg_compile_sdk(logic, prg, built, err_buf, err_cap);
}

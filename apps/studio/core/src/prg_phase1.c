#include "retr01_studio/collision.h"
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
#define PLAY_COLL_COUNT 33
#define PLAY_COLL_DIR 34
#define PLAY_INST_COUNT 0xC0u
#define PLAY_INST_TABLE 0xC1u

#define R01P_OFF 0x00F0u
#define R01_PLAY_SOLID_DATA_OFF 0x0700u
#define R01_PLAY_INST_LIMIT 0x0500u /* before CPU $8500 */

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

static void fill_instance_table(uint8_t prg[R01_PRG_BYTES], const R01World *w) {
    int n = 0;
    int i;
    size_t base = PLAY_OFF + PLAY_INST_TABLE;
    size_t limit = R01_PLAY_INST_LIMIT;

    prg[PLAY_OFF + PLAY_INST_COUNT] = 0;
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
    prg[PLAY_OFF + PLAY_INST_COUNT] = (uint8_t)n;
}

static void fill_collision_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01World *w) {
    size_t data_off = R01_PLAY_SOLID_DATA_OFF;
    size_t grid_off = R01_PRG_COLL_GRID_OFF;
    int di = 0;
    int si;
    int n;
    int i;

    memset(prg + grid_off, 0, 16u * 16u * 2u);
    n = p ? p->solid_pat_count : 0;
    if (n < 0) {
        n = 0;
    }
    if (n > R01_SOLID_PAT_MAX) {
        n = R01_SOLID_PAT_MAX;
    }
    if (data_off + 1u + (size_t)n * 2u >= R01_PRG_C_OFF) {
        prg[PLAY_OFF + PLAY_COLL_COUNT] = 0;
        return;
    }
    prg[data_off++] = (uint8_t)n;
    for (i = 0; i < n; i++) {
        prg[data_off++] = p->solid_pat_bank[i];
        prg[data_off++] = p->solid_pat_tile[i];
    }

    if (!w) {
        prg[PLAY_OFF + PLAY_COLL_COUNT] = 0;
        return;
    }
    for (si = 0; si < w->screen_count; si++) {
        const R01Screen *s = &w->screens[si];
        int cell;
        uint16_t tab_addr;
        size_t slot;
        if (!s->present || s->col < 0 || s->col >= R01_GRID_MAX || s->row < 0 || s->row >= R01_GRID_MAX) {
            continue;
        }
        if (data_off + R01_TILES_PER_SCREEN > R01_PRG_C_OFF) {
            break;
        }
        tab_addr = (uint16_t)(CODE_BASE + data_off);
        for (cell = 0; cell < R01_TILES_PER_SCREEN; cell++) {
            int bank = r01_attr_solid_bank(s->attrs[cell]);
            int tile = (int)s->tiles[cell];
            prg[data_off++] = r01_project_pattern_solid(p, bank, tile) ? 1u : 0u;
        }
        slot = grid_off + ((size_t)s->row * 16u + (size_t)s->col) * 2u;
        prg[slot] = (uint8_t)(tab_addr & 0xFFu);
        prg[slot + 1u] = (uint8_t)(tab_addr >> 8);
        if (PLAY_OFF + PLAY_COLL_DIR + (size_t)(di + 1) * 4u <= PLAY_OFF + PLAY_INST_COUNT) {
            uint8_t *ent = prg + PLAY_OFF + PLAY_COLL_DIR + (size_t)di * 4u;
            ent[0] = (uint8_t)s->col;
            ent[1] = (uint8_t)s->row;
            ent[2] = (uint8_t)(tab_addr & 0xFFu);
            ent[3] = (uint8_t)(tab_addr >> 8);
            di++;
        }
    }
    prg[PLAY_OFF + PLAY_COLL_COUNT] = (uint8_t)di;
}

void r01_prg_overlay_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout) {
    uint8_t mask[PLAY_PRESENT_BYTES];
    int spawn_c = R01_START_COL, spawn_r = R01_START_ROW;
    const R01World *w = p ? &p->worlds[0] : NULL;

    if (!prg) {
        return;
    }
    pick_spawn(w, &spawn_c, &spawn_r);
    fill_present_mask(mask, w);
    memcpy(prg + PLAY_OFF + PLAY_PRESENT, mask, PLAY_PRESENT_BYTES);
    prg[PLAY_OFF + PLAY_SPAWN_CELL] = R01_CELL_PACK(spawn_c, spawn_r);
    fill_collision_tables(prg, p, w);
    fill_instance_table(prg, w);

    prg[R01P_OFF] = 'R';
    prg[R01P_OFF + 1] = '0';
    prg[R01P_OFF + 2] = '1';
    prg[R01P_OFF + 3] = 'P';
    prg[R01P_OFF + 4] = R01_PRG_R01P_VER;
    put_u16_le(prg + R01P_OFF + 5, (uint16_t)(CODE_BASE + R01_PLAY_SOLID_DATA_OFF));
    /* $80F7-$80FE stay linker zeros. Live gravity / anim / BGM start are author C. */

    patch_boot_map(prg, layout);
}

void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout) {
    if (!prg) {
        return;
    }
    memset(prg, 0, R01_PRG_BYTES);
    r01_prg_overlay_tables(prg, p, layout);
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
        "/apps/sdk/r01_c/include/r01_entity.h",
        "/apps/sdk/r01_c/include/r01_entity_ids.h",
        "/apps/sdk/r01_c/include/r01_warp_ids.h",
        "/apps/common/r01_play_camera.c",
        "/apps/common/r01_play_physics.c",
        "/apps/common/r01_play_collision.c",
        "/apps/common/r01_play_anim.c",
        "/apps/common/r01_play_anim_cart.c",
        "/apps/common/r01_play_anim_cart.h",
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
    const char *outp = out_prg_path;
    int st;
    size_t sz = 0;

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
    if (snprintf(cmd, sizeof(cmd), "\"%s/apps/sdk/r01_c/build-prg.sh\" \"%s\" \"%s\"", R01_REPO_ROOT, logic_c,
                 outp) >= (int)sizeof(cmd)) {
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

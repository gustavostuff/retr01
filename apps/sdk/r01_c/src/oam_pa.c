#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_anim.h"
#include "r01_play_anim_cart.h"

#include <stddef.h>

#ifndef R01_HOST_TEST

#define R01_PA_MAX 1032u
#define R01_CPU8(addr) (*(volatile uint8_t *)(uint16_t)(addr))
#define R01_PLAY_INST_COUNT 0x81C0u
#define R01_PLAY_INST_TABLE 0x81C1u

static uint8_t s_pa[R01_PA_MAX];
static uint16_t s_pa_len;
static uint8_t s_pa_ok;
static R01CartPlayerAnim s_anim_blob;
static R01PlayAnimCtx s_anim;
static uint8_t s_anim_ready;
static int s_hit_dx;
static int s_hit_dy;
static uint8_t s_hit_w = 8;
static uint8_t s_hit_h = 8;
static uint8_t s_state_delay[R01_PLAY_ANIM_STATES_MAX];
static uint8_t s_player_type = 0xFFu;
static uint8_t s_ent_n[32];
static uint8_t s_ent_spr[32][R01_CART_ENTITY_PARTS_MAX][4];
static uint8_t s_ent_types;
int8_t r01_ent_hx[32];
int8_t r01_ent_hy[32];
uint8_t r01_ent_hw[32];
uint8_t r01_ent_hh[32];
uint8_t r01_live_n;
uint8_t r01_live_type[16];
uint8_t r01_live_flags[16];
uint8_t r01_live_state[16];
uint16_t r01_live_x[16];
uint16_t r01_live_y[16];

void r01_player_hit_get(int *dx, int *dy, uint8_t *w, uint8_t *h) {
    if (dx) {
        *dx = s_hit_dx;
    }
    if (dy) {
        *dy = s_hit_dy;
    }
    if (w) {
        *w = s_hit_w;
    }
    if (h) {
        *h = s_hit_h;
    }
}

static uint16_t map_u16(void) {
    uint16_t v = r01_map_read();
    v |= (uint16_t)r01_map_read() << 8;
    return v;
}

static void cache_one_type(uint32_t cat, uint8_t t) {
    uint16_t off;
    uint16_t soff;
    uint16_t foff;
    uint8_t n;
    uint8_t i;
    s_ent_n[t] = 0;
    r01_map_seek(cat + (uint32_t)t * 2u);
    off = map_u16();
    if (off == 0u) {
        return;
    }
    r01_map_seek(cat + off);
    (void)r01_map_read();
    if (r01_map_read() < 1u) {
        return;
    }
    (void)r01_map_read();
    (void)r01_map_read();
    soff = map_u16();
    (void)map_u16();
    (void)map_u16();
    (void)map_u16();
    if (soff == 0u) {
        return;
    }
    r01_map_seek(cat + off + soff);
    if (r01_map_read() < 1u) {
        return;
    }
    (void)r01_map_read();
    foff = map_u16();
    if (foff == 0u) {
        return;
    }
    r01_map_seek(cat + off + soff + foff);
    (void)r01_map_read();
    n = r01_map_read();
    if (n > (uint8_t)R01_CART_ENTITY_PARTS_MAX) {
        n = (uint8_t)R01_CART_ENTITY_PARTS_MAX;
    }
    r01_ent_hx[t] = (int8_t)r01_map_read();
    r01_ent_hy[t] = (int8_t)r01_map_read();
    r01_ent_hw[t] = r01_map_read();
    r01_ent_hh[t] = r01_map_read();
    if (r01_ent_hw[t] < 1u) {
        r01_ent_hw[t] = 8;
    }
    if (r01_ent_hh[t] < 1u) {
        r01_ent_hh[t] = 8;
    }
    for (i = 0; i < n; i++) {
        s_ent_spr[t][i][0] = r01_map_read();
        s_ent_spr[t][i][1] = r01_map_read();
        s_ent_spr[t][i][2] = r01_map_read();
        s_ent_spr[t][i][3] = r01_map_read();
    }
    s_ent_n[t] = n;
}

static void cache_spawn_types(uint32_t world) {
    uint32_t cat;
    uint8_t t;
    s_ent_types = 0;
    s_player_type = 0xFFu;
    for (t = 0; t < 32u; t++) {
        s_ent_n[t] = 0;
        r01_ent_hx[t] = 0;
        r01_ent_hy[t] = 0;
        r01_ent_hw[t] = 8;
        r01_ent_hh[t] = 8;
    }
    r01_map_seek(world + R01_CART_WHDR_TYPE_COUNT);
    s_ent_types = r01_map_read();
    r01_map_seek(world + R01_CART_WHDR_PLAYER_ENTITY);
    s_player_type = r01_map_read();
    /* Cart pointer table slot 4 (entities): hdr 16 B + 24. Catalog is cart-global. */
    r01_map_seek((uint32_t)R01_CART_HDR_BYTES + 24u);
    cat = r01_map_read_u24();
    if (cat == 0u) {
        s_ent_types = 0;
        return;
    }
    if (s_ent_types > 32u) {
        s_ent_types = 32u;
    }
    for (t = 0; t < s_ent_types; t++) {
        cache_one_type(cat, t);
    }
}

static void boot_live_instances(void) {
    uint8_t n;
    uint8_t i;
    uint16_t base;
    r01_live_n = 0;
    n = R01_CPU8(R01_PLAY_INST_COUNT);
    if (n > 16u) {
        n = 16u;
    }
    base = (uint16_t)R01_PLAY_INST_TABLE;
    for (i = 0; i < n && r01_live_n < 16u; i++) {
        uint8_t type = R01_CPU8(base);
        uint8_t flags = R01_CPU8(base + 1u);
        uint16_t wx = (uint16_t)R01_CPU8(base + 2u) | ((uint16_t)R01_CPU8(base + 3u) << 8);
        uint16_t wy = (uint16_t)R01_CPU8(base + 4u) | ((uint16_t)R01_CPU8(base + 5u) << 8);
        base += (uint16_t)R01_CART_INSTANCE_SIZE;
        if (type == s_player_type) {
            continue;
        }
        r01_live_type[r01_live_n] = type;
        r01_live_flags[r01_live_n] = flags;
        r01_live_state[r01_live_n] = 0;
        r01_live_x[r01_live_n] = wx;
        r01_live_y[r01_live_n] = wy;
        r01_live_n++;
    }
}

void r01_pa_boot(void) {
    uint32_t world = r01_boot_u24(12);
    uint8_t flags;
    uint32_t off_insts;
    uint16_t i;
    s_pa_ok = 0;
    s_pa_len = 0;
    s_anim_ready = 0;
    s_hit_dx = 0;
    s_hit_dy = 0;
    s_hit_w = 8;
    s_hit_h = 8;
    for (i = 0; i < (uint16_t)R01_PLAY_ANIM_STATES_MAX; i++) {
        s_state_delay[i] = 0;
    }
    if (world == 0u) {
        return;
    }
    r01_map_seek(world + R01_CART_WHDR_PLAYER_HIT_X);
    s_hit_dx = (int)(int8_t)r01_map_read();
    s_hit_dy = (int)(int8_t)r01_map_read();
    s_hit_w = r01_map_read();
    s_hit_h = r01_map_read();
    if (s_hit_w < 1u) {
        s_hit_w = 8;
    }
    if (s_hit_h < 1u) {
        s_hit_h = 8;
    }
    cache_spawn_types(world);
    boot_live_instances();
    r01_map_seek(world + R01_CART_WHDR_FLAGS);
    flags = r01_map_read();
    if ((flags & R01_CART_WHDR_FLAG_PLAYER_ANIM) == 0u) {
        return;
    }
    r01_map_seek(world + R01_CART_WHDR_OFF_INSTS);
    off_insts = r01_map_read_u24();
    r01_map_seek(world + off_insts);
    for (i = 0; i < R01_PA_MAX; i++) {
        s_pa[i] = r01_map_read();
    }
    s_pa_len = R01_PA_MAX;
    if (r01_cart_player_anim_parse(s_pa, s_pa_len, &s_anim_blob) == 0) {
        const uint8_t *fh;
        s_pa_ok = 1;
        fh = r01_cart_player_anim_frame_hdr(&s_anim_blob, 0, 0);
        if (fh) {
            s_hit_dx = (int)fh[2] - (int)fh[0];
            s_hit_dy = (int)fh[3] - (int)fh[1];
            s_hit_w = fh[4] ? fh[4] : 8u;
            s_hit_h = fh[5] ? fh[5] : 8u;
        }
        {
            int si;
            for (si = 0; si < s_anim_blob.state_count && si < (int)R01_PLAY_ANIM_STATES_MAX; si++) {
                const uint8_t *sh = r01_cart_player_anim_frame_hdr(&s_anim_blob, si, 0);
                s_state_delay[si] = (sh && sh[7] > 0u) ? sh[7] : (uint8_t)R01_PLAY_ANIM_DELAY_DEFAULT;
            }
        }
    }
}

static int map_state(uint8_t v) {
    return (v == 0xFFu) ? -1 : (int)v;
}

static void anim_from_ctx(R01PlayAnimCtx *dst, const R01GameCtx *ctx) {
    uint8_t i;
    r01_play_anim_init(dst);
    dst->player_idle_state = map_state(ctx->player_idle_state);
    for (i = 0; i < 8u; i++) {
        dst->player_walk_state[i] = map_state(ctx->player_walk_state[i]);
    }
    dst->player_crouch_state = map_state(ctx->player_crouch_state);
    dst->player_jump_state = map_state(ctx->player_jump_state);
    dst->player_anim_delay_override = (int)ctx->player_anim_delay_override;
    dst->player_anim_moving = (int)ctx->player_anim_moving;
    for (i = 0; i < (uint8_t)R01_PLAY_ANIM_STATES_MAX; i++) {
        if (s_state_delay[i] > 0u) {
            dst->player_state_delay[i] = (int)s_state_delay[i];
        }
    }
}

void r01_game_anim_tick(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
    if (!ctx) {
        return;
    }
    if (!s_anim_ready) {
        anim_from_ctx(&s_anim, ctx);
        s_anim_ready = 1;
    }
    s_anim.player_idle_state = map_state(ctx->player_idle_state);
    {
        uint8_t d;
        for (d = 0; d < 8u; d++) {
            s_anim.player_walk_state[d] = map_state(ctx->player_walk_state[d]);
        }
    }
    s_anim.player_crouch_state = map_state(ctx->player_crouch_state);
    s_anim.player_jump_state = map_state(ctx->player_jump_state);
    s_anim.player_anim_delay_override = (int)ctx->player_anim_delay_override;
    r01_play_anim_set_airborne(&s_anim, airborne);
    r01_play_anim_set_crouching(&s_anim, crouching);
    r01_play_anim_update(&s_anim, adx, ady);
    if (s_pa_ok) {
        r01_play_anim_tick_cart(&s_anim, &s_anim_blob);
    }
}

#ifndef R01_HOST_TEST
static uint8_t s_oam_written;

static uint8_t draw_spawn_instances(const R01GameCtx *ctx, uint8_t written) {
    uint8_t i;
    if (!ctx) {
        return written;
    }
    for (i = 0; i < r01_live_n && written < (uint8_t)R01_OAM_MAX; i++) {
        uint8_t type = r01_live_type[i];
        uint8_t flags = r01_live_flags[i];
        uint8_t st = r01_live_state[i];
        uint16_t wx = r01_live_x[i];
        uint16_t wy = r01_live_y[i];
        uint8_t pc = (type < s_ent_types) ? s_ent_n[type] : 0;
        uint8_t pi;
        for (pi = 0; pi < pc && written < (uint8_t)R01_OAM_MAX; pi++) {
            int rx = (int)(int8_t)s_ent_spr[type][pi][1];
            int ry = (int)(int8_t)s_ent_spr[type][pi][2];
            uint8_t attr = s_ent_spr[type][pi][3];
            uint8_t tile = s_ent_spr[type][pi][0];
            int sx;
            int sy;
            if (st != 0u) {
                tile++;
            }
            if (flags & 1u) {
                rx = -rx - 8;
                attr = (uint8_t)(attr ^ R01_ATTR_FLIP_H);
            }
            if (flags & 2u) {
                ry = -ry - 8;
                attr = (uint8_t)(attr ^ R01_ATTR_FLIP_V);
            }
            sx = (int)wx + rx - (int)ctx->cam_x;
            sy = (int)wy + ry - (int)ctx->cam_y;
            if (sx + 8 <= 0 || sy + 8 <= 0 || sx >= R01_SCREEN_PX_W || sy >= R01_SCREEN_PX_H) {
                continue;
            }
            {
                uint8_t *o = r01_oam_scratch + (uint16_t)written * 4u;
                o[0] = (uint8_t)sy;
                o[1] = tile;
                o[2] = attr;
                o[3] = (uint8_t)sx;
            }
            written++;
        }
    }
    return written;
}
#endif

void r01_game_draw_sprites(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
    int sx;
    int sy;
    uint8_t written = 0;
    (void)airborne;
    (void)crouching;
    (void)adx;
    (void)ady;
    if (!ctx) {
        return;
    }
    if (!s_anim_ready) {
        anim_from_ctx(&s_anim, ctx);
        s_anim_ready = 1;
    }

    sx = (int)ctx->player_x - (int)ctx->cam_x;
    sy = (int)ctx->player_y - (int)ctx->cam_y;
    if (s_pa_ok) {
        int state = r01_play_anim_entity_state(&s_anim);
        int frame = r01_play_anim_frame(&s_anim);
        int pc = 0;
        const uint8_t *fh = r01_cart_player_anim_frame_hdr(&s_anim_blob, state, frame);
        const uint8_t *parts = r01_cart_player_anim_frame_parts(&s_anim_blob, state, frame, &pc);
        int origin_x = fh ? (int)fh[0] : 0;
        int origin_y = fh ? (int)fh[1] : 0;
        int flip = r01_play_anim_flip_h(&s_anim);
        int pi;
        for (pi = 0; pi < pc && pi < R01_CART_PLAYER_ANIM_PARTS_MAX && parts; pi++) {
            const uint8_t *pt = parts + (size_t)pi * 4u;
            int dx;
            int dy;
            uint8_t attr;
            int px;
            int py;
            if (flip) {
                r01_cart_part_pose(origin_x, origin_y, (int)(int8_t)pt[2], (int)(int8_t)pt[3], pt[1], 1, 0, &dx,
                                   &dy, &attr);
            } else {
                dx = (int)(int8_t)pt[2];
                dy = (int)(int8_t)pt[3];
                attr = pt[1];
            }
            px = sx + dx - origin_x;
            py = sy + dy - origin_y;
            if (px + 8 <= 0 || py + 8 <= 0 || px >= R01_SCREEN_PX_W || py >= R01_SCREEN_PX_H) {
                continue;
            }
            {
                uint8_t *o = r01_oam_scratch + (uint16_t)written * 4u;
                o[0] = (uint8_t)py;
                o[1] = pt[0];
                o[2] = attr;
                o[3] = (uint8_t)px;
            }
            written++;
        }
    } else if (sx + R01_PLAY_PLAYER_W > 0 && sy + R01_PLAY_PLAYER_H > 0 && sx < R01_SCREEN_PX_W &&
               sy < R01_SCREEN_PX_H) {
        r01_oam_scratch[0] = (uint8_t)sy;
        r01_oam_scratch[1] = 0;
        r01_oam_scratch[2] = 0;
        r01_oam_scratch[3] = (uint8_t)sx;
        written = 1;
    }
    written = draw_spawn_instances(ctx, written);
    s_oam_written = written;
}

void r01_game_draw_player(const R01GameCtx *ctx) {
    (void)ctx;
    r01_map_lock();
    r01_oam_commit(s_oam_written);
    r01_map_unlock();
}

#else
void r01_pa_boot(void) {
}

void r01_player_hit_get(int *dx, int *dy, uint8_t *w, uint8_t *h) {
    if (dx) {
        *dx = 0;
    }
    if (dy) {
        *dy = 0;
    }
    if (w) {
        *w = 8;
    }
    if (h) {
        *h = 8;
    }
}

void r01_game_anim_tick(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
    (void)ctx;
    (void)airborne;
    (void)crouching;
    (void)adx;
    (void)ady;
}

void r01_game_draw_sprites(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
    int sx;
    int sy;
    (void)airborne;
    (void)crouching;
    (void)adx;
    (void)ady;
    if (!ctx) {
        return;
    }
    sx = (int)ctx->player_x - (int)ctx->cam_x;
    sy = (int)ctx->player_y - (int)ctx->cam_y;
    r01_oam_reset();
    if (sx + R01_PLAY_PLAYER_W > 0 && sy + R01_PLAY_PLAYER_H > 0 && sx < R01_SCREEN_PX_W &&
        sy < R01_SCREEN_PX_H) {
        r01_oam_write((uint8_t)sy, 0, 0, (uint8_t)sx);
    }
}

void r01_game_draw_player(const R01GameCtx *ctx) {
    r01_game_draw_sprites(ctx, 0, 0, 0, 0);
}

uint8_t r01_live_n;
uint8_t r01_live_type[16];
uint8_t r01_live_flags[16];
uint8_t r01_live_state[16];
uint16_t r01_live_x[16];
uint16_t r01_live_y[16];
int8_t r01_ent_hx[32];
int8_t r01_ent_hy[32];
uint8_t r01_ent_hw[32];
uint8_t r01_ent_hh[32];
#endif

#include "r01_engine.h"

#include "r01_cart_caps.h"
#include "r01_play_anim.h"

#include <stddef.h>

#ifndef R01_HOST_TEST

#define R01_PLAY_INST_COUNT 0x81C0u
#define R01_PLAY_INST_TABLE 0x81C1u

static R01PlayAnimCtx s_anim;
static uint8_t s_anim_ready;
static int8_t s_hit_dx;
static int8_t s_hit_dy;
static uint8_t s_hit_w = 8;
static uint8_t s_hit_h = 8;
static uint8_t s_player_type = 0xFFu;
static uint8_t s_ent_n[32];
static uint8_t s_ent_spr[32][R01_CART_ENTITY_PARTS_MAX][4];
static uint8_t s_ent_types;
static uint8_t s_pl_sc;
static uint8_t s_pl_fc[R01_CART_ENTITY_STATES_MAX];
static uint8_t s_pl_delay[R01_CART_ENTITY_STATES_MAX][R01_CART_ENTITY_FRAMES_MAX];
static uint8_t s_pl_n[R01_CART_ENTITY_STATES_MAX][R01_CART_ENTITY_FRAMES_MAX];
static uint8_t s_pl_spr[R01_CART_ENTITY_STATES_MAX][R01_CART_ENTITY_FRAMES_MAX][R01_CART_ENTITY_PARTS_MAX][4];
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

void r01_player_hit_get(int8_t *dx, int8_t *dy, uint8_t *w, uint8_t *h) {
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

static void cache_player_frames(uint32_t cat, uint8_t t) {
    uint16_t off;
    uint16_t state_off[R01_CART_ENTITY_STATES_MAX];
    uint8_t sc;
    uint8_t si;
    uint8_t i;
    s_pl_sc = 0;
    for (si = 0; si < (uint8_t)R01_CART_ENTITY_STATES_MAX; si++) {
        s_pl_fc[si] = 0;
        for (i = 0; i < (uint8_t)R01_CART_ENTITY_FRAMES_MAX; i++) {
            s_pl_delay[si][i] = (uint8_t)R01_PLAY_ANIM_DELAY_DEFAULT;
            s_pl_n[si][i] = 0;
        }
    }
    if (t >= 32u) {
        return;
    }
    r01_map_seek(cat + (uint32_t)t * 2u);
    off = map_u16();
    if (off == 0u) {
        return;
    }
    r01_map_seek(cat + off);
    (void)r01_map_read();
    sc = r01_map_read();
    (void)r01_map_read();
    (void)r01_map_read();
    if (sc > (uint8_t)R01_CART_ENTITY_STATES_MAX) {
        sc = (uint8_t)R01_CART_ENTITY_STATES_MAX;
    }
    for (si = 0; si < (uint8_t)R01_CART_ENTITY_STATES_MAX; si++) {
        state_off[si] = map_u16();
    }
    s_pl_sc = sc;
    for (si = 0; si < sc; si++) {
        uint16_t soff = state_off[si];
        uint16_t frame_off[R01_CART_ENTITY_FRAMES_MAX];
        uint8_t fc;
        uint8_t fi;
        if (soff == 0u) {
            continue;
        }
        r01_map_seek(cat + off + soff);
        fc = r01_map_read();
        (void)r01_map_read();
        if (fc > (uint8_t)R01_CART_ENTITY_FRAMES_MAX) {
            fc = (uint8_t)R01_CART_ENTITY_FRAMES_MAX;
        }
        s_pl_fc[si] = fc;
        for (fi = 0; fi < (uint8_t)R01_CART_ENTITY_FRAMES_MAX; fi++) {
            frame_off[fi] = map_u16();
        }
        for (fi = 0; fi < fc; fi++) {
            uint16_t foff = frame_off[fi];
            uint8_t delay;
            uint8_t n;
            uint8_t pi;
            if (foff == 0u) {
                continue;
            }
            r01_map_seek(cat + off + soff + foff);
            delay = r01_map_read();
            n = r01_map_read();
            if (delay < 1u) {
                delay = (uint8_t)R01_PLAY_ANIM_DELAY_DEFAULT;
            }
            s_pl_delay[si][fi] = delay;
            (void)r01_map_read();
            (void)r01_map_read();
            (void)r01_map_read();
            (void)r01_map_read();
            if (n > (uint8_t)R01_CART_ENTITY_PARTS_MAX) {
                n = (uint8_t)R01_CART_ENTITY_PARTS_MAX;
            }
            for (pi = 0; pi < n; pi++) {
                s_pl_spr[si][fi][pi][0] = r01_map_read();
                s_pl_spr[si][fi][pi][1] = r01_map_read();
                s_pl_spr[si][fi][pi][2] = r01_map_read();
                s_pl_spr[si][fi][pi][3] = r01_map_read();
            }
            s_pl_n[si][fi] = n;
        }
    }
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
    cache_player_frames(cat, s_player_type);
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
    uint8_t i;
    s_anim_ready = 0;
    s_hit_dx = 0;
    s_hit_dy = 0;
    s_hit_w = 8;
    s_hit_h = 8;
    s_pl_sc = 0;
    if (world == 0u) {
        return;
    }
    r01_map_seek(world + R01_CART_WHDR_PLAYER_HIT_X);
    s_hit_dx = (int8_t)r01_map_read();
    s_hit_dy = (int8_t)r01_map_read();
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
    if (s_player_type < 32u && r01_ent_hw[s_player_type] > 0u) {
        s_hit_dx = r01_ent_hx[s_player_type];
        s_hit_dy = r01_ent_hy[s_player_type];
        s_hit_w = r01_ent_hw[s_player_type];
        s_hit_h = r01_ent_hh[s_player_type];
    }
    for (i = 0; i < (uint8_t)R01_PLAY_ANIM_STATES_MAX && i < s_pl_sc; i++) {
        if (s_pl_fc[i] > 0u) {
            s_anim.player_state_delay[i] = s_pl_delay[i][0];
        }
    }
}

static void anim_from_ctx(R01PlayAnimCtx *dst, const R01GameCtx *ctx) {
    uint8_t i;
    r01_play_anim_init(dst);
    dst->player_idle_state = ctx->player_idle_state;
    for (i = 0; i < 8u; i++) {
        dst->player_walk_state[i] = ctx->player_walk_state[i];
    }
    dst->player_crouch_state = ctx->player_crouch_state;
    dst->player_jump_state = ctx->player_jump_state;
    dst->player_anim_delay_override = ctx->player_anim_delay_override;
    dst->player_anim_moving = ctx->player_anim_moving;
    for (i = 0; i < (uint8_t)R01_PLAY_ANIM_STATES_MAX && i < s_pl_sc; i++) {
        if (s_pl_fc[i] > 0u) {
            dst->player_state_delay[i] = s_pl_delay[i][0];
        }
    }
}

static void anim_tick_catalog(R01PlayAnimCtx *ctx) {
    uint8_t st;
    uint8_t fc;
    uint8_t delay;
    uint8_t frame;
    if (!ctx) {
        return;
    }
    if (ctx->player_idle_state == (uint8_t)R01_PLAY_ANIM_UNMAPPED && ctx->player_anim_state == 0u) {
        ctx->player_anim_frame = 0;
        return;
    }
    st = ctx->player_anim_state;
    if (st >= s_pl_sc) {
        return;
    }
    fc = s_pl_fc[st];
    if (fc < 1u) {
        return;
    }
    if (fc <= 1u) {
        ctx->player_anim_frame = 0;
        return;
    }
    frame = ctx->player_anim_frame;
    if (frame >= fc) {
        frame = 0;
        ctx->player_anim_frame = 0;
    }
    delay = s_pl_delay[st][frame];
    delay = r01_play_anim_frame_delay(ctx, delay);
    ctx->player_anim_ctr++;
    if (ctx->player_anim_ctr < delay) {
        return;
    }
    ctx->player_anim_ctr = 0;
    ctx->player_anim_frame++;
    if (ctx->player_anim_frame >= fc) {
        ctx->player_anim_frame = 0;
    }
}

void r01_game_anim_tick(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
    uint8_t d;
    if (!ctx) {
        return;
    }
    if (!s_anim_ready) {
        anim_from_ctx(&s_anim, ctx);
        s_anim_ready = 1;
    }
    s_anim.player_idle_state = ctx->player_idle_state;
    for (d = 0; d < 8u; d++) {
        s_anim.player_walk_state[d] = ctx->player_walk_state[d];
    }
    s_anim.player_crouch_state = ctx->player_crouch_state;
    s_anim.player_jump_state = ctx->player_jump_state;
    s_anim.player_anim_delay_override = ctx->player_anim_delay_override;
    r01_play_anim_set_airborne(&s_anim, airborne);
    r01_play_anim_set_crouching(&s_anim, crouching);
    r01_play_anim_update(&s_anim, (int8_t)adx, (int8_t)ady);
    anim_tick_catalog(&s_anim);
}

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
            int16_t rx = (int16_t)(int8_t)s_ent_spr[type][pi][1];
            int16_t ry = (int16_t)(int8_t)s_ent_spr[type][pi][2];
            uint8_t attr = s_ent_spr[type][pi][3];
            uint8_t tile = s_ent_spr[type][pi][0];
            int16_t sx;
            int16_t sy;
            if (st != 0u) {
                tile++;
            }
            if (flags & 1u) {
                rx = (int16_t)(-rx - 8);
                attr = (uint8_t)(attr ^ R01_ATTR_FLIP_H);
            }
            if (flags & 2u) {
                ry = (int16_t)(-ry - 8);
                attr = (uint8_t)(attr ^ R01_ATTR_FLIP_V);
            }
            sx = (int16_t)((int16_t)wx + rx - (int16_t)ctx->cam_x);
            sy = (int16_t)((int16_t)wy + ry - (int16_t)ctx->cam_y);
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

static uint8_t draw_player_catalog(const R01GameCtx *ctx, uint8_t written) {
    int16_t ox;
    int16_t oy;
    uint8_t st;
    uint8_t fr;
    uint8_t pc;
    uint8_t pi;
    uint8_t flip;
    if (!ctx) {
        return written;
    }
    ox = (int16_t)((int16_t)ctx->player_x - (int16_t)ctx->cam_x);
    oy = (int16_t)((int16_t)ctx->player_y - (int16_t)ctx->cam_y);
    st = r01_play_anim_entity_state(&s_anim);
    fr = r01_play_anim_frame(&s_anim);
    if (st >= s_pl_sc) {
        st = 0;
    }
    if (fr >= s_pl_fc[st]) {
        fr = 0;
    }
    pc = s_pl_n[st][fr];
    flip = r01_play_anim_flip_h(&s_anim);
    if (pc < 1u) {
        if (ox + R01_PLAY_PLAYER_W > 0 && oy + R01_PLAY_PLAYER_H > 0 && ox < R01_SCREEN_PX_W &&
            oy < R01_SCREEN_PX_H && written < (uint8_t)R01_OAM_MAX) {
            uint8_t *o = r01_oam_scratch + (uint16_t)written * 4u;
            o[0] = (uint8_t)oy;
            o[1] = 0;
            o[2] = 0;
            o[3] = (uint8_t)ox;
            written++;
        }
        return written;
    }
    for (pi = 0; pi < pc && written < (uint8_t)R01_OAM_MAX; pi++) {
        int16_t rx = (int16_t)(int8_t)s_pl_spr[st][fr][pi][1];
        int16_t ry = (int16_t)(int8_t)s_pl_spr[st][fr][pi][2];
        uint8_t attr = s_pl_spr[st][fr][pi][3];
        uint8_t tile = s_pl_spr[st][fr][pi][0];
        int16_t sx;
        int16_t sy;
        if (flip) {
            rx = (int16_t)(-rx - 8);
            attr = (uint8_t)(attr ^ R01_ATTR_FLIP_H);
        }
        sx = (int16_t)(ox + rx);
        sy = (int16_t)(oy + ry);
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
    return written;
}

void r01_game_draw_sprites(const R01GameCtx *ctx, int airborne, int crouching, int adx, int ady) {
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
    written = draw_player_catalog(ctx, written);
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

void r01_player_hit_get(int8_t *dx, int8_t *dy, uint8_t *w, uint8_t *h) {
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
    int16_t sx;
    int16_t sy;
    (void)airborne;
    (void)crouching;
    (void)adx;
    (void)ady;
    if (!ctx) {
        return;
    }
    sx = (int16_t)((int16_t)ctx->player_x - (int16_t)ctx->cam_x);
    sy = (int16_t)((int16_t)ctx->player_y - (int16_t)ctx->cam_y);
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

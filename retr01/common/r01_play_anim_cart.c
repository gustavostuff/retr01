#include "r01_play_anim_cart.h"

static const uint8_t *state_ptr_at(const R01CartPlayerAnim *anim, int state_idx) {
    const uint8_t *p;
    int si;
    if (!anim || !anim->blob || state_idx < 0 || state_idx >= anim->state_count) {
        return NULL;
    }
    p = anim->blob + 3;
    for (si = 0; si < anim->state_count; si++) {
        int dc;
        int fi;
        if (p + 7 > anim->blob + anim->len) {
            return NULL;
        }
        if (si == state_idx) {
            return p;
        }
        dc = (int)p[6];
        p += 7;
        for (fi = 0; fi < dc; fi++) {
            int pc;
            if (p >= anim->blob + anim->len) {
                return NULL;
            }
            pc = (int)p[0];
            p += 1 + pc * 4;
        }
    }
    return NULL;
}

static const uint8_t *state_end(const R01CartPlayerAnim *anim, const uint8_t *st) {
    int dc;
    int fi;
    const uint8_t *p;
    if (!anim || !st || st + 7 > anim->blob + anim->len) {
        return NULL;
    }
    dc = (int)st[6];
    p = st + 7;
    for (fi = 0; fi < dc; fi++) {
        int pc;
        if (p >= anim->blob + anim->len) {
            return NULL;
        }
        pc = (int)p[0];
        p += 1 + pc * 4;
    }
    return p;
}

int r01_cart_player_anim_parse(const uint8_t *blob, size_t len, R01CartPlayerAnim *out) {
    if (!out) {
        return -1;
    }
    out->blob = NULL;
    out->len = 0;
    out->state_count = 0;
    if (!blob || len < 3u) {
        return -1;
    }
    if (blob[0] != R01_CART_PLAYER_ANIM_MAGIC0 || blob[1] != R01_CART_PLAYER_ANIM_MAGIC1) {
        return -1;
    }
    out->blob = blob;
    out->len = len;
    out->state_count = (int)blob[2];
    if (out->state_count < 1 || out->state_count > R01_PLAY_ANIM_STATES_MAX) {
        return -1;
    }
    {
        const uint8_t *last = state_ptr_at(out, out->state_count - 1);
        if (!last || !state_end(out, last) || state_end(out, last) > out->blob + out->len) {
            return -1;
        }
    }
    return 0;
}

int r01_cart_player_anim_state_count(const R01CartPlayerAnim *anim) {
    return anim ? anim->state_count : 0;
}

int r01_cart_player_anim_state_hdr(const R01CartPlayerAnim *anim, int state_idx, const uint8_t **out_state) {
    const uint8_t *st = state_ptr_at(anim, state_idx);
    if (!st) {
        return -1;
    }
    if (out_state) {
        *out_state = st;
    }
    return 0;
}

int r01_cart_player_anim_drawable_count(const R01CartPlayerAnim *anim, int state_idx) {
    const uint8_t *st = state_ptr_at(anim, state_idx);
    if (!st) {
        return 0;
    }
    return (int)st[6];
}

const uint8_t *r01_cart_player_anim_frame_parts(const R01CartPlayerAnim *anim, int state_idx, int frame_slot,
                                                int *out_part_count) {
    const uint8_t *st;
    const uint8_t *p;
    int dc;
    int fi;
    if (out_part_count) {
        *out_part_count = 0;
    }
    st = state_ptr_at(anim, state_idx);
    if (!st || frame_slot < 0) {
        return NULL;
    }
    dc = (int)st[6];
    if (frame_slot >= dc) {
        return NULL;
    }
    p = st + 7;
    for (fi = 0; fi < dc; fi++) {
        int pc;
        if (p >= anim->blob + anim->len) {
            return NULL;
        }
        pc = (int)p[0];
        if (fi == frame_slot) {
            if (out_part_count) {
                *out_part_count = pc;
            }
            return p + 1;
        }
        p += 1 + pc * 4;
    }
    return NULL;
}

void r01_cart_part_pose(int origin_x, int origin_y, int part_dx, int part_dy, uint8_t attr, int inst_flip_h,
                        int inst_flip_v, int *out_dx, int *out_dy, uint8_t *out_attr) {
    int dx = part_dx;
    int dy = part_dy;
    uint8_t a = attr;
    if (inst_flip_h) {
        dx = 2 * origin_x - dx - 8;
        a = (uint8_t)(a ^ 0x10u);
    }
    if (inst_flip_v) {
        dy = 2 * origin_y - dy - 8;
        a = (uint8_t)(a ^ 0x20u);
    }
    if (out_dx) {
        *out_dx = dx;
    }
    if (out_dy) {
        *out_dy = dy;
    }
    if (out_attr) {
        *out_attr = a;
    }
}

void r01_play_anim_tick_cart(R01PlayAnimCtx *ctx, const R01CartPlayerAnim *anim) {
    int delay;
    int frame_count;
    const uint8_t *st;
    if (!ctx || !anim) {
        return;
    }
    if (ctx->player_anim_state < 0 || ctx->player_anim_state >= anim->state_count) {
        return;
    }
    st = state_ptr_at(anim, ctx->player_anim_state);
    if (!st) {
        return;
    }
    frame_count = (int)st[6];
    if (frame_count < 1) {
        return;
    }
    if (frame_count <= 1) {
        ctx->player_anim_frame = 0;
        return;
    }
    delay = ctx->player_state_delay[ctx->player_anim_state];
    if (delay < 1) {
        delay = 1;
    }
    ctx->player_anim_ctr++;
    if (ctx->player_anim_ctr < delay) {
        return;
    }
    ctx->player_anim_ctr = 0;
    ctx->player_anim_frame++;
    if (ctx->player_anim_frame >= frame_count) {
        ctx->player_anim_frame = 0;
    }
}

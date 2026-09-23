#include "r01_play_anim_cart.h"

#define FRAME_HDR 8

static int frame_span(const uint8_t *p, const uint8_t *end) {
    int pc;
    if (!p || p + FRAME_HDR > end) {
        return -1;
    }
    pc = (int)p[6];
    if (pc < 0) {
        return -1;
    }
    if (p + FRAME_HDR + pc * 4 > end) {
        return -1;
    }
    return FRAME_HDR + pc * 4;
}

static const uint8_t *state_ptr_at(const R01CartPlayerAnim *anim, int state_idx) {
    const uint8_t *p;
    const uint8_t *end;
    int si;
    if (!anim || !anim->blob || state_idx < 0 || state_idx >= anim->state_count) {
        return NULL;
    }
    end = anim->blob + anim->len;
    p = anim->blob + 3;
    for (si = 0; si < anim->state_count; si++) {
        int dc;
        int fi;
        if (p + 1 > end) {
            return NULL;
        }
        if (si == state_idx) {
            return p;
        }
        dc = (int)p[0];
        p += 1;
        for (fi = 0; fi < dc; fi++) {
            int n = frame_span(p, end);
            if (n < 0) {
                return NULL;
            }
            p += n;
        }
    }
    return NULL;
}

static const uint8_t *state_end(const R01CartPlayerAnim *anim, const uint8_t *st) {
    int dc;
    int fi;
    const uint8_t *p;
    const uint8_t *end;
    if (!anim || !st) {
        return NULL;
    }
    end = anim->blob + anim->len;
    if (st + 1 > end) {
        return NULL;
    }
    dc = (int)st[0];
    p = st + 1;
    for (fi = 0; fi < dc; fi++) {
        int n = frame_span(p, end);
        if (n < 0) {
            return NULL;
        }
        p += n;
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
    {
        int si;
        for (si = 0; si < R01_PLAY_ANIM_STATES_MAX; si++) {
            int fi;
            out->frame_count[si] = 0;
            for (fi = 0; fi < R01_CART_ENTITY_FRAMES_MAX; fi++) {
                out->frame_hdr[si][fi] = NULL;
            }
        }
        for (si = 0; si < out->state_count; si++) {
            const uint8_t *st = state_ptr_at(out, si);
            const uint8_t *end = out->blob + out->len;
            const uint8_t *p;
            int dc;
            int fi;
            if (!st) {
                return -1;
            }
            dc = (int)st[0];
            if (dc < 0) {
                dc = 0;
            }
            if (dc > R01_CART_ENTITY_FRAMES_MAX) {
                dc = R01_CART_ENTITY_FRAMES_MAX;
            }
            out->frame_count[si] = (uint8_t)dc;
            p = st + 1;
            for (fi = 0; fi < dc; fi++) {
                int n = frame_span(p, end);
                if (n < 0) {
                    return -1;
                }
                out->frame_hdr[si][fi] = p;
                p += n;
            }
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
    if (!anim || state_idx < 0 || state_idx >= anim->state_count) {
        return 0;
    }
    return (int)anim->frame_count[state_idx];
}

const uint8_t *r01_cart_player_anim_frame_hdr(const R01CartPlayerAnim *anim, int state_idx, int frame_slot) {
    if (!anim || state_idx < 0 || state_idx >= anim->state_count || frame_slot < 0) {
        return NULL;
    }
    if (frame_slot >= (int)anim->frame_count[state_idx]) {
        return NULL;
    }
    return anim->frame_hdr[state_idx][frame_slot];
}

const uint8_t *r01_cart_player_anim_frame_parts(const R01CartPlayerAnim *anim, int state_idx, int frame_slot,
                                                int *out_part_count) {
    const uint8_t *fh = r01_cart_player_anim_frame_hdr(anim, state_idx, frame_slot);
    if (out_part_count) {
        *out_part_count = 0;
    }
    if (!fh) {
        return NULL;
    }
    if (out_part_count) {
        *out_part_count = (int)fh[6];
    }
    return fh + FRAME_HDR;
}

void r01_cart_part_pose(int origin_x, int origin_y, int part_dx, int part_dy, uint8_t attr, int inst_flip_h,
                        int inst_flip_v, int *out_dx, int *out_dy, uint8_t *out_attr) {
    int dx = part_dx;
    int dy = part_dy;
    uint8_t a = attr;
    if (inst_flip_h) {
        dx = 2 * origin_x - dx - 8;
        a = (uint8_t)(a ^ R01_CART_OAM_FLIP_H);
    }
    if (inst_flip_v) {
        dy = 2 * origin_y - dy - 8;
        a = (uint8_t)(a ^ R01_CART_OAM_FLIP_V);
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
    uint8_t delay;
    int frame_count;
    if (!ctx || !anim) {
        return;
    }
    if (ctx->player_idle_state == (uint8_t)R01_PLAY_ANIM_UNMAPPED && ctx->player_anim_state == 0u) {
        ctx->player_anim_frame = 0;
        return;
    }
    if (ctx->player_anim_state >= (uint8_t)anim->state_count) {
        return;
    }
    frame_count = (int)anim->frame_count[ctx->player_anim_state];
    if (frame_count < 1) {
        return;
    }
    if (frame_count <= 1) {
        ctx->player_anim_frame = 0;
        return;
    }
    {
        const uint8_t *fh = r01_cart_player_anim_frame_hdr(anim, (int)ctx->player_anim_state,
                                                           (int)ctx->player_anim_frame);
        delay = 0;
        if (fh && fh[7] > 0u) {
            delay = fh[7];
        } else if (ctx->player_anim_state < (uint8_t)R01_PLAY_ANIM_STATES_MAX) {
            delay = ctx->player_state_delay[ctx->player_anim_state];
        }
    }
    delay = r01_play_anim_frame_delay(ctx, delay);
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

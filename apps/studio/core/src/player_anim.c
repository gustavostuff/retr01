#include "retr01_studio/player_anim.h"
#include "retr01_studio/game_runtime.h"
#include "retr01_studio/entities.h"
#include "r01_play_anim.h"

#define R01_PLAYER_ANIM_WRAP(call)                                                                 \
    do {                                                                                           \
        R01PlayAnimCtx _a;                                                                         \
        if (!ctx) {                                                                                \
            return;                                                                                \
        }                                                                                          \
        R01_PLAY_ANIM_PULL(&_a, ctx);                                                              \
        call;                                                                                      \
        R01_PLAY_ANIM_PUSH(&_a, ctx);                                                              \
    } while (0)

void r01_player_anim_init(R01GameCtx *ctx) {
    R01PlayAnimCtx a;
    if (!ctx) {
        return;
    }
    r01_play_anim_init(&a);
    R01_PLAY_ANIM_PUSH(&a, ctx);
}

void r01_player_anim_set_idle_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_idle_state(&_a, entity_state_idx));
}

void r01_player_anim_set_walk_state(R01GameCtx *ctx, int dir8, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_walk_state(&_a, dir8, entity_state_idx));
}

void r01_player_anim_set_walk_all(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_walk_all(&_a, entity_state_idx));
}

void r01_player_anim_set_crouch_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_crouch_state(&_a, entity_state_idx));
}

void r01_player_anim_set_jump_state(R01GameCtx *ctx, int entity_state_idx) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_jump_state(&_a, entity_state_idx));
}

void r01_player_anim_set_frame_delay(R01GameCtx *ctx, int ticks) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_set_frame_delay(&_a, ticks));
}

int r01_player_anim_frame_delay(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_delay_override : 0;
}

void r01_player_default_face_set(R01GameCtx *ctx, int face) {
    R01_PLAYER_ANIM_WRAP(r01_play_default_face_set(&_a, face));
}

void r01_entity_state_frame_delay_set(R01GameCtx *ctx, int entity_state_idx, int ticks) {
    R01_PLAYER_ANIM_WRAP(r01_play_state_frame_delay_set(&_a, entity_state_idx, ticks));
}

void r01_player_anim_update(R01GameCtx *ctx, int dx, int dy) {
    R01_PLAYER_ANIM_WRAP(r01_play_anim_update(&_a, dx, dy));
}

void r01_player_anim_tick(R01GameCtx *ctx, const R01Project *p, int player_type) {
    const R01EntityType *ent;
    const R01EntityState *st;
    int delay;
    int frame_count;
    if (!ctx || !p || player_type < 0 || player_type >= p->entity_count) {
        return;
    }
    if (ctx->player_idle_state < 0 && ctx->player_anim_state == 0) {
        ctx->player_anim_frame = 0;
        return;
    }
    ent = &p->entities[player_type];
    if (ctx->player_anim_state < 0 || ctx->player_anim_state >= ent->state_count) {
        return;
    }
    st = &ent->states[ctx->player_anim_state];
    frame_count = r01_entity_state_drawable_frame_count(st);
    if (frame_count < 1) {
        return;
    }
    if (frame_count <= 1) {
        ctx->player_anim_frame = 0;
        return;
    }
    {
        int fi = r01_entity_state_drawable_frame_index(st, ctx->player_anim_frame);
        delay = ctx->player_state_delay[ctx->player_anim_state];
        if (fi >= 0 && fi < st->frame_count && st->frames[fi].delay > 0) {
            delay = st->frames[fi].delay;
        }
    }
    if (delay < 1) {
        delay = 1;
    }
    if (ctx->player_anim_delay_override > 0) {
        delay = ctx->player_anim_delay_override;
        if (delay < 1) {
            delay = 1;
        }
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

int r01_player_anim_entity_state(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_state : 0;
}

int r01_player_anim_frame(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_frame : 0;
}

int r01_player_anim_flip_h(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_flip_h : 0;
}

int r01_player_anim_dir(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_dir : R01_PLAYER_DIR_RIGHT;
}

int r01_player_anim_moving(const R01GameCtx *ctx) {
    return ctx ? ctx->player_anim_moving : 0;
}

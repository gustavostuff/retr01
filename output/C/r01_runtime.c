/* Generated runtime stubs for custom_logic (host-side, Phase 5D). */
#include "include/r01_engine.h"

#define R01_EVENT_SLOTS 4
static struct { uint8_t btn; R01EventFn fn; } s_events[R01_EVENT_SLOTS];

uint8_t r01_pad_pressed(const R01GameCtx *ctx, uint8_t btn) {
    if (!ctx) return 0;
    return (uint8_t)((ctx->pad >> btn) & 1u);
}

uint8_t r01_pad_just_pressed(R01GameCtx *ctx, uint8_t btn) {
    if (!ctx) return 0;
    return (uint8_t)(((ctx->pad ^ ctx->pad_prev) & ctx->pad) >> btn) & 1u;
}

void r01_player_warp(R01GameCtx *ctx, int col, int row) {
    if (!ctx) return;
    ctx->player_x = col * 128 + (128 - 8) / 2;
    ctx->player_y = row * 120 + (120 - 8) / 2;
}

void r01_player_set_type(uint8_t type_id) { (void)type_id; }
int r01_entity_spawn(uint8_t type, int wx, int wy) {
    (void)type; (void)wx; (void)wy; return -1;
}
void r01_entity_remove(int inst) { (void)inst; }
void r01_world_warp_screen(int col, int row) { (void)col; (void)row; }
int r01_event_on_button(uint8_t btn, R01EventFn fn) {
    int i;
    if (!fn) return -1;
    for (i = 0; i < R01_EVENT_SLOTS; i++) {
        if (!s_events[i].fn) {
            s_events[i].btn = btn;
            s_events[i].fn = fn;
            return i;
        }
    }
    return -1;
}
void r01_runtime_dispatch_buttons(R01GameCtx *ctx) {
    int i;
    if (!ctx) return;
    for (i = 0; i < R01_EVENT_SLOTS; i++) {
        if (s_events[i].fn && r01_pad_just_pressed(ctx, s_events[i].btn)) {
            s_events[i].fn(ctx);
        }
    }
}

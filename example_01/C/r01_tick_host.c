/* Host Play author tick plugin. Regenerated on export. */
#include "include/r01_engine.h"
#include <string.h>

typedef struct {
    const char *id;
    int sc, sr, tc, tr;
} R01WarpEntRec;
typedef struct {
    int ent;
    int dsc, dsr, dtc, dtr;
    uint8_t flags;
} R01WarpExitRec;

const R01WarpEntRec warp_ents[1] = {{0}};
const R01WarpExitRec warp_exits[1] = {{0}};
const int warp_ent_count = 0;
const int warp_exit_count = 0;
const int player_state_frames[4] = {1, 1, 1, 1};

void r01_host_custom_tick(uint8_t pad, int *move_mul, int *frame_delay) {
    R01GameCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pad = pad;
    r01_player_set_move_mul(&ctx, 1);
    r01_player_anim_set_frame_delay(&ctx, 0);
    r01_custom_on_tick(&ctx);
    if (move_mul) {
        *move_mul = r01_player_move_mul(&ctx);
    }
    if (frame_delay) {
        *frame_delay = r01_player_anim_frame_delay(&ctx);
    }
}

#include "r01_engine.h"

R01GameCtx g_ctx;

void r01_nmi(void) {
    r01_game_on_vblank(&g_ctx);
}

int main(void) {
    r01_boot_copy_solids();
    r01_game_ctx_init(&g_ctx);
    r01_game_spawn(&g_ctx);
    r01_game_on_init(&g_ctx);
    r01_game_camera_snap(&g_ctx);
    r01_boot_map_stream();
    r01_sys_publish(&g_ctx);
    r01_game_draw_player(&g_ctx);
    for (;;) {
        r01_ppu_wait_vblank();
        r01_pad_poll(&g_ctx);
        g_ctx.player_move_mul = 1;
        r01_game_on_tick(&g_ctx);
        r01_game_play_tick(&g_ctx);
    }
}

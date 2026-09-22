#include "r01_engine.h"

R01GameCtx g_ctx;

void r01_nmi(void) {
    r01_tracker_nmi();
    r01_game_on_vblank(&g_ctx);
}

int main(void) {
    r01_boot_copy_solids();
    r01_game_ctx_init(&g_ctx);
    r01_game_spawn(&g_ctx);
    r01_game_on_init(&g_ctx);
    r01_game_camera_snap(&g_ctx);
    r01_boot_map_stream();
    r01_world_cache_boot();
    r01_map_load_window(g_ctx.cam_x, g_ctx.cam_y);
    r01_bg0_publish(&g_ctx);
    r01_tracker_boot();
    r01_pa_boot();
    r01_oam_boot_hide();
    r01_game_draw_sprites(&g_ctx, 0, 0, 0, 0);
    r01_game_draw_player(&g_ctx);
    r01_scroll_publish(&g_ctx);
    r01_sys_publish(&g_ctx);
    r01_irq_enable();
    for (;;) {
        r01_ppu_wait_vblank();
        r01_game_draw_player(&g_ctx);
        /* Scroll + MAP use last tick's cam (host follow of last RAM publish). */
        r01_scroll_publish(&g_ctx);
        r01_map_load_window(g_ctx.cam_x, g_ctx.cam_y);
        r01_bg0_publish(&g_ctx);
        r01_pad_poll(&g_ctx);
        g_ctx.player_move_mul = 1;
        g_ctx.player_anim_delay_override = 0;
        r01_game_on_tick(&g_ctx);
        r01_game_play_tick(&g_ctx);
        r01_sys_publish(&g_ctx);
    }
}

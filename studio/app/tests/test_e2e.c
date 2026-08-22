#include "e2e_harness.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include "third_party/greatest.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

static AppShell g_app;
static char g_workdir[512];

static int cell_x(int col) {
    return UI_WORLD_GRID_X + col * UI_WORLD_CELL + UI_WORLD_CELL / 2;
}
static int cell_y(int row) {
    return UI_WORLD_GRID_Y + row * UI_WORLD_CELL + UI_WORLD_CELL / 2;
}
static int screen_px(int px, int py, int *ox, int *oy) {
    *ox = UI_LEFT_W + 8 + px * 2; /* screen_zoom default 2 */
    *oy = 28 + py * 2;
    return 0;
}

static void begin_case(void) {
    e2e_reset_project(&g_app);
    e2e_set_workdir(&g_app, g_workdir);
}

TEST cold_start_layout(void) {
    begin_case();
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "01_cold_start"));
    ASSERT_EQ(1, g_app.ui.world_tab);
    ASSERT_EQ(UI_LAYER_BG, g_app.ui.layer);
    PASS();
}

TEST place_and_select_screen(void) {
    R01World *w;
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(1), cell_y(2));
    w = &g_app.ui.project->worlds[0];
    ASSERT_EQ(1, w->screen_count);
    ASSERT_EQ(1, w->screens[0].col);
    ASSERT_EQ(2, w->screens[0].row);
    ASSERT(g_app.ui.project->active_screen >= 0);
    e2e_click(&g_app, cell_x(1), cell_y(2));
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "02_screen_placed"));
    PASS();
}

TEST paint_bg_pixels(void) {
    R01Screen *s;
    int x, y;
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_key(&g_app, SDLK_2, KMOD_NONE); /* color 1 */
    screen_px(10, 12, &x, &y);
    e2e_drag(&g_app, x, y, x + 16, y + 16);
    s = r01_project_active_screen(g_app.ui.project);
    ASSERT(s != NULL);
    ASSERT_EQ(1, (int)r01_screen_get_pixel(s, 10, 12));
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "03_paint_bg"));
    PASS();
}

TEST attr_mode_flags(void) {
    R01EditSurface surf;
    uint8_t a;
    int x, y;
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_key(&g_app, SDLK_TAB, KMOD_NONE);
    ASSERT_EQ(UI_MODE_ATTR, g_app.ui.edit_mode);
    screen_px(8, 8, &x, &y);
    e2e_click(&g_app, x, y);
    e2e_key(&g_app, SDLK_n, KMOD_NONE); /* ANIM */
    e2e_key(&g_app, SDLK_o, KMOD_NONE); /* SOLID */
    ASSERT_EQ(0, r01_project_edit_surface(g_app.ui.project, &surf));
    a = r01_tilemap_get_attr(surf.attrs, g_app.ui.attr_tx, g_app.ui.attr_ty);
    ASSERT(r01_attr_anim(a));
    ASSERT(r01_attr_solid(a));
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "04_attr_mode"));
    PASS();
}

TEST layer_sprite_place_oam(void) {
    R01Screen *s;
    int x, y;
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_key(&g_app, SDLK_l, KMOD_NONE);
    ASSERT_EQ(UI_LAYER_SPR, g_app.ui.layer);
    screen_px(40, 40, &x, &y);
    e2e_click(&g_app, x, y);
    s = r01_project_active_screen(g_app.ui.project);
    ASSERT(s != NULL);
    ASSERT_EQ(1, s->oam_count);
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "05_sprite_oam"));
    PASS();
}

TEST generate_bg_bank(void) {
    R01World *w;
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_key(&g_app, SDLK_3, KMOD_NONE);
    {
        int x, y;
        screen_px(0, 0, &x, &y);
        e2e_drag(&g_app, x, y, x + 20, y + 20);
    }
    e2e_key(&g_app, SDLK_g, KMOD_CTRL);
    w = &g_app.ui.project->worlds[0];
    ASSERT(w->bg_banks[g_app.ui.project->generate_bank].tile_count > 0);
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "06_generate_bg"));
    PASS();
}

TEST constraints_and_play(void) {
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_ctrl_click(&g_app, cell_x(1), cell_y(0));
    e2e_scroll_left_bottom(&g_app);
    {
        int before = g_app.ui.project->constraints.scroll_mode;
        e2e_click(&g_app, 90, UI_CONSTRAINTS_Y - g_app.ui.left_scroll_y + 26);
        ASSERT_EQ((before + 1) % 4, g_app.ui.project->constraints.scroll_mode);
    }
    e2e_key(&g_app, SDLK_SPACE, KMOD_NONE);
    ASSERT_EQ(1, g_app.ui.play.active);
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "07_play_mode"));
    /* nudge via tick API (keyboard state is process-global; inject ticks) */
    r01_play_tick(&g_app.ui.play, g_app.ui.project, 1, 0);
    r01_play_tick(&g_app.ui.play, g_app.ui.project, 1, 0);
    ASSERT(g_app.ui.play.player_x > R01_SCREEN_PX_W / 2);
    e2e_key(&g_app, SDLK_ESCAPE, KMOD_NONE);
    ASSERT_EQ(0, g_app.ui.play.active);
    PASS();
}

TEST save_load_roundtrip(void) {
    char err[128];
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(3), cell_y(4));
    g_app.ui.project->paint_color = 2;
    e2e_key(&g_app, SDLK_s, KMOD_CTRL);
    ASSERT(strstr(g_app.ui.status, "saved") != NULL);
    e2e_reset_project(&g_app);
    e2e_set_workdir(&g_app, g_workdir);
    e2e_key(&g_app, SDLK_o, KMOD_CTRL);
    ASSERT(strstr(g_app.ui.status, "loaded") != NULL);
    ASSERT_EQ(1, g_app.ui.project->worlds[0].screen_count);
    ASSERT_EQ(3, g_app.ui.project->worlds[0].screens[0].col);
    ASSERT_EQ(4, g_app.ui.project->worlds[0].screens[0].row);
    (void)err;
    PASS();
}

TEST export_cart_bundle(void) {
    char path[768];
    begin_case();
    e2e_ctrl_click(&g_app, cell_x(0), cell_y(0));
    e2e_key(&g_app, SDLK_e, KMOD_CTRL);
    ASSERT(strstr(g_app.ui.status, "exported") != NULL);
    snprintf(path, sizeof(path), "%s.retr01", g_workdir);
    ASSERT_EQ(0, access(path, R_OK));
    snprintf(path, sizeof(path), "%s_prom.bin", g_workdir);
    ASSERT_EQ(0, access(path, R_OK));
    snprintf(path, sizeof(path), "%s_boot.s", g_workdir);
    ASSERT_EQ(0, access(path, R_OK));
    snprintf(path, sizeof(path), "%s_flash.bin", g_workdir);
    ASSERT_EQ(0, access(path, R_OK));
    PASS();
}

TEST planes_toggle(void) {
    begin_case();
    /* P0 button around PLANES_Y+18 */
    e2e_ctrl_click(&g_app, 20, UI_PLANES_Y + 24);
    ASSERT_EQ(1, g_app.ui.project->worlds[0].planes[0].present);
    ASSERT(g_app.ui.project->active_plane == 0);
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "08_plane_edit"));
    PASS();
}

TEST grid_toggle_and_palette(void) {
    begin_case();
    e2e_key(&g_app, SDLK_g, KMOD_NONE);
    ASSERT_EQ(0, g_app.ui.show_grid);
    e2e_scroll_left_bottom(&g_app);
    /* scroll back up toward palettes: left_scroll so PAL is visible */
    g_app.ui.left_scroll_y = UI_PAL_Y - 20;
    e2e_click(&g_app, 20, 30); /* B0 tab approx after scroll */
    ASSERT_EQ(0, e2e_assert_golden(&g_app, "09_grid_off_palette"));
    PASS();
}

SUITE(studio_e2e) {
    RUN_TEST(cold_start_layout);
    RUN_TEST(place_and_select_screen);
    RUN_TEST(paint_bg_pixels);
    RUN_TEST(attr_mode_flags);
    RUN_TEST(layer_sprite_place_oam);
    RUN_TEST(generate_bg_bank);
    RUN_TEST(constraints_and_play);
    RUN_TEST(save_load_roundtrip);
    RUN_TEST(export_cart_bundle);
    RUN_TEST(planes_toggle);
    RUN_TEST(grid_toggle_and_palette);
}

GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    int watch;
    GREATEST_MAIN_BEGIN();

    watch = e2e_watch();
    if (watch) {
        /* CTest sets SDL_VIDEODRIVER=offscreen; clear it so a real window appears. */
        unsetenv("SDL_VIDEODRIVER");
        unsetenv("SDL_RENDER_DRIVER");
        fprintf(stderr, "E2E_WATCH=1 — showing Studio window (delay %sms, override with E2E_WATCH_MS)\n",
                getenv("E2E_WATCH_MS") && getenv("E2E_WATCH_MS")[0] ? getenv("E2E_WATCH_MS") : "120");
    }

    snprintf(g_workdir, sizeof(g_workdir), "%s/e2e_project", R01_E2E_OUT_DIR);
    /* Watch: visible window. CI/ctest: headless offscreen. Scale stays 1 either way
     * so injected logic coords match window pixels. */
    if (app_shell_init(&g_app, watch ? 0 : 1) != 0) {
        fprintf(stderr, "app_shell_init failed (need a display, or use headless ctest)\n");
        return 1;
    }
    if (watch) {
        g_app.scale = 1;
        g_app.ui.scale = 1;
        SDL_SetWindowSize(g_app.win, UI_LOGIC_W, UI_LOGIC_H);
        SDL_SetWindowTitle(g_app.win, "Retr01 Studio — E2E watch");
        SDL_ShowWindow(g_app.win);
        SDL_RaiseWindow(g_app.win);
        e2e_pump(&g_app);
    }

    RUN_SUITE(studio_e2e);

    if (watch) {
        fprintf(stderr, "E2E watch done — closing in 1s\n");
        SDL_Delay(1000);
    }

    app_shell_shutdown(&g_app);
    GREATEST_MAIN_END();
}

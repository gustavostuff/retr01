#include "e2e_harness.h"

#include "retr01_studio/project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define FRAME_BYTES ((size_t)UI_LOGIC_W * (size_t)UI_LOGIC_H * 4u)

int e2e_update_goldens(void) {
    const char *v = getenv("UPDATE_GOLDENS");
    return v && v[0] == '1';
}

int e2e_watch(void) {
    const char *v = getenv("E2E_WATCH");
    return v && v[0] == '1';
}

static int watch_delay_ms(void) {
    const char *v = getenv("E2E_WATCH_MS");
    if (v && v[0]) {
        int ms = atoi(v);
        return ms > 0 ? ms : 0;
    }
    return 120;
}

void e2e_pump(AppShell *app) {
    SDL_Event e;
    if (!app) {
        return;
    }
    /* Keep the window responsive while tests inject synthetic events. */
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            /* Ignore quit during watch so the suite finishes. */
        }
    }
    app_shell_frame(app);
    if (e2e_watch()) {
        SDL_Delay((Uint32)watch_delay_ms());
    }
}

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    return mkdir(path, 0755);
}

void e2e_reset_project(AppShell *app) {
    r01_project_init(app->ui.project, "e2e");
    app->ui.world_tab = 1;
    app->ui.bg_bank_tab = 0;
    app->ui.spr_bank_tab = 0;
    app->ui.spr_tile = 0;
    app->ui.spr_tool = UI_SPR_TOOL_PLACE;
    app->ui.spr_size16 = 0;
    app->ui.oam_sel = -1;
    app->ui.meta_sel = -1;
    app->ui.layer = UI_LAYER_BG;
    app->ui.screen_zoom = 2;
    app->ui.screen_pan_x = 0;
    app->ui.screen_pan_y = 0;
    app->ui.left_scroll_y = 0;
    app->ui.edit_mode = UI_MODE_PIXEL;
    app->ui.attr_tx = 0;
    app->ui.attr_ty = 0;
    app->ui.pal_row_tab = 0;
    app->ui.pal_slot = 0;
    app->ui.show_grid = 1;
    app->ui.brush_down = 0;
    app->ui.play_last_tick = 0;
    memset(&app->ui.play, 0, sizeof(app->ui.play));
    snprintf(app->ui.status, sizeof(app->ui.status), "e2e");
}

void e2e_set_workdir(AppShell *app, const char *stem_path) {
    char json[R01_PATH_MAX];
    snprintf(json, sizeof(json), "%s.json", stem_path);
    strncpy(app->ui.project_path, json, R01_PATH_MAX - 1);
    app->ui.project_path[R01_PATH_MAX - 1] = 0;
}

static void mouse_button(AppShell *app, int lx, int ly, Uint8 button, Uint8 state, Uint16 mod) {
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = state == SDL_PRESSED ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.button = button;
    e.button.state = state;
    e.button.clicks = 1;
    e.button.x = lx;
    e.button.y = ly;
    SDL_SetModState(mod);
    app_shell_handle_event(app, &e);
}

void e2e_key(AppShell *app, SDL_Keycode sym, Uint16 mod) {
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_KEYDOWN;
    e.key.state = SDL_PRESSED;
    e.key.keysym.sym = sym;
    e.key.keysym.mod = mod;
    SDL_SetModState(mod);
    app_shell_handle_event(app, &e);
    SDL_SetModState(KMOD_NONE);
    e2e_pump(app);
}

void e2e_click(AppShell *app, int lx, int ly) {
    mouse_button(app, lx, ly, SDL_BUTTON_LEFT, SDL_PRESSED, KMOD_NONE);
    mouse_button(app, lx, ly, SDL_BUTTON_LEFT, SDL_RELEASED, KMOD_NONE);
    e2e_pump(app);
}

void e2e_ctrl_click(AppShell *app, int lx, int ly) {
    mouse_button(app, lx, ly, SDL_BUTTON_LEFT, SDL_PRESSED, KMOD_CTRL);
    mouse_button(app, lx, ly, SDL_BUTTON_LEFT, SDL_RELEASED, KMOD_CTRL);
    SDL_SetModState(KMOD_NONE);
    e2e_pump(app);
}

void e2e_drag(AppShell *app, int x0, int y0, int x1, int y1) {
    SDL_Event e;
    int x, y, steps, i;
    mouse_button(app, x0, y0, SDL_BUTTON_LEFT, SDL_PRESSED, KMOD_NONE);
    steps = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    if (steps < 1) {
        steps = 1;
    }
    for (i = 1; i <= steps; i++) {
        x = x0 + ((x1 - x0) * i) / steps;
        y = y0 + ((y1 - y0) * i) / steps;
        memset(&e, 0, sizeof(e));
        e.type = SDL_MOUSEMOTION;
        e.motion.state = SDL_BUTTON_LMASK;
        e.motion.x = x;
        e.motion.y = y;
        app_shell_handle_event(app, &e);
        if (e2e_watch() && (i % 4) == 0) {
            e2e_pump(app);
        }
    }
    mouse_button(app, x1, y1, SDL_BUTTON_LEFT, SDL_RELEASED, KMOD_NONE);
    e2e_pump(app);
}

void e2e_wheel(AppShell *app, int lx, int ly, int wheel_y) {
    SDL_Event e;
    memset(&e, 0, sizeof(e));
    e.type = SDL_MOUSEWHEEL;
    e.wheel.y = wheel_y;
    /* harness uses logic coords as window coords at scale 1 */
    SDL_WarpMouseInWindow(app->win, lx, ly);
    app_shell_handle_event(app, &e);
    e2e_pump(app);
}

void e2e_scroll_left_bottom(AppShell *app) {
    int guard = 0;
    while (app->ui.left_scroll_y < UI_LEFT_CONTENT_H - UI_LOGIC_H && guard < 64) {
        e2e_wheel(app, 40, 100, -10);
        guard++;
    }
}

static SDL_Surface *rgba_to_surface(const uint8_t *rgba) {
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormat(0, UI_LOGIC_W, UI_LOGIC_H, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) {
        return NULL;
    }
    memcpy(s->pixels, rgba, FRAME_BYTES);
    return s;
}

static int load_rgba_bmp(const char *path, uint8_t *out_rgba) {
    SDL_Surface *loaded;
    SDL_Surface *conv;
    loaded = SDL_LoadBMP(path);
    if (!loaded) {
        return -1;
    }
    conv = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!conv || conv->w != UI_LOGIC_W || conv->h != UI_LOGIC_H) {
        if (conv) {
            SDL_FreeSurface(conv);
        }
        return -1;
    }
    memcpy(out_rgba, conv->pixels, FRAME_BYTES);
    SDL_FreeSurface(conv);
    return 0;
}

static int save_rgba_bmp(const char *path, const uint8_t *rgba) {
    SDL_Surface *s = rgba_to_surface(rgba);
    int rc;
    if (!s) {
        return -1;
    }
    rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc == 0 ? 0 : -1;
}

static int pixel_diff_count(const uint8_t *a, const uint8_t *b, size_t n, int *first_i) {
    size_t i;
    int diffs = 0;
    *first_i = -1;
    for (i = 0; i < n; i += 4) {
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) {
            if (*first_i < 0) {
                *first_i = (int)(i / 4);
            }
            diffs++;
        }
    }
    return diffs;
}

int e2e_dump(AppShell *app, const char *name) {
    uint8_t *rgba;
    char path[512];
    int rc;
    ensure_dir(R01_E2E_OUT_DIR);
    rgba = (uint8_t *)malloc(FRAME_BYTES);
    if (!rgba) {
        return -1;
    }
    if (app_shell_read_rgba(app, rgba) != 0) {
        free(rgba);
        return -1;
    }
    snprintf(path, sizeof(path), "%s/%s.bmp", R01_E2E_OUT_DIR, name);
    rc = save_rgba_bmp(path, rgba);
    free(rgba);
    return rc;
}

int e2e_assert_golden(AppShell *app, const char *name) {
    uint8_t *got;
    uint8_t *want;
    char golden[512];
    char got_path[512];
    char diff_path[512];
    int first = -1, diffs;
    ensure_dir(R01_E2E_GOLDEN_DIR);
    ensure_dir(R01_E2E_OUT_DIR);

    got = (uint8_t *)malloc(FRAME_BYTES);
    want = (uint8_t *)malloc(FRAME_BYTES);
    if (!got || !want) {
        free(got);
        free(want);
        fprintf(stderr, "e2e golden oom\n");
        return -1;
    }
    if (app_shell_read_rgba(app, got) != 0) {
        free(got);
        free(want);
        fprintf(stderr, "e2e capture failed: %s\n", SDL_GetError());
        return -1;
    }

    snprintf(golden, sizeof(golden), "%s/%s.bmp", R01_E2E_GOLDEN_DIR, name);
    snprintf(got_path, sizeof(got_path), "%s/%s_got.bmp", R01_E2E_OUT_DIR, name);
    snprintf(diff_path, sizeof(diff_path), "%s/%s_diff.bmp", R01_E2E_OUT_DIR, name);

    if (e2e_update_goldens()) {
        if (save_rgba_bmp(golden, got) != 0) {
            fprintf(stderr, "failed writing golden %s\n", golden);
            free(got);
            free(want);
            return -1;
        }
        fprintf(stderr, "updated golden %s\n", golden);
        free(got);
        free(want);
        return 0;
    }

    if (load_rgba_bmp(golden, want) != 0) {
        save_rgba_bmp(got_path, got);
        fprintf(stderr, "missing golden %s (wrote %s); run with UPDATE_GOLDENS=1\n", golden, got_path);
        free(got);
        free(want);
        return -1;
    }

    diffs = pixel_diff_count(got, want, FRAME_BYTES, &first);
    if (diffs != 0) {
        size_t i;
        uint8_t *diff = (uint8_t *)malloc(FRAME_BYTES);
        save_rgba_bmp(got_path, got);
        if (diff) {
            memcpy(diff, want, FRAME_BYTES);
            for (i = 0; i < FRAME_BYTES; i += 4) {
                if (got[i] != want[i] || got[i + 1] != want[i + 1] || got[i + 2] != want[i + 2]) {
                    diff[i] = 255;
                    diff[i + 1] = 0;
                    diff[i + 2] = 0;
                    diff[i + 3] = 255;
                }
            }
            save_rgba_bmp(diff_path, diff);
            free(diff);
        }
        fprintf(stderr, "golden mismatch %s: %d px (first=%d,%d) got=%s diff=%s\n", name, diffs,
                first >= 0 ? first % UI_LOGIC_W : -1, first >= 0 ? first / UI_LOGIC_W : -1, got_path,
                diff_path);
        free(got);
        free(want);
        return -1;
    }

    free(got);
    free(want);
    if (e2e_watch()) {
        fprintf(stderr, "golden ok: %s\n", name);
        e2e_pump(app);
    }
    return 0;
}

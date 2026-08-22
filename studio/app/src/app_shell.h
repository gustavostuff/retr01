#ifndef RETR01_STUDIO_APP_SHELL_H
#define RETR01_STUDIO_APP_SHELL_H

#include "ui.h"

typedef struct AppShell {
    UiState ui;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *target;
    int scale;
} AppShell;

/*
 * Init SDL + UI. If headless != 0, prefer offscreen driver + hidden window +
 * software renderer (deterministic for E2E).
 */
int app_shell_init(AppShell *app, int headless);
void app_shell_shutdown(AppShell *app);

/* Draw one frame into the logical render target (does not Present). */
void app_shell_draw(AppShell *app);

/* Tick play + draw + present (interactive main loop). */
void app_shell_frame(AppShell *app);

/* Map window coords to logical canvas; feed ui_handle_event. Returns ui result. */
int app_shell_handle_event(AppShell *app, const SDL_Event *e);

/* Read logical canvas RGBA32 (UI_LOGIC_W * UI_LOGIC_H * 4 bytes). */
int app_shell_read_rgba(AppShell *app, uint8_t *out_rgba);

#endif

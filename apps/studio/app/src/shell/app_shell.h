#ifndef retr01_STUDIO_APP_SHELL_H
#define retr01_STUDIO_APP_SHELL_H

#include "ui/ui.h"

typedef struct AppShell {
    UiState ui;
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Texture *target;
    int scale;        /* present integer scale (from window or hotkey) */
    int render_scale; /* preferred 1 or 2 via Ctrl+1 / Ctrl+2 */
} AppShell;

int app_shell_init(AppShell *app, int headless);
void app_shell_shutdown(AppShell *app);
void app_shell_draw(AppShell *app);
void app_shell_frame(AppShell *app);
void app_shell_apply_logic_scale(AppShell *app);
void app_shell_set_render_scale(AppShell *app, int scale);
int app_shell_handle_event(AppShell *app, const SDL_Event *e);

#endif

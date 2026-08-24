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

int app_shell_init(AppShell *app, int headless);
void app_shell_shutdown(AppShell *app);
void app_shell_draw(AppShell *app);
void app_shell_frame(AppShell *app);
int app_shell_handle_event(AppShell *app, const SDL_Event *e);

#endif

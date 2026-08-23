#include "app_shell.h"

#include <stdio.h>

int main(int argc, char **argv) {
    AppShell app;
    int running = 1;

    (void)argc;
    (void)argv;

    if (app_shell_init(&app, 0) != 0) {
        return 1;
    }

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else {
                app_shell_handle_event(&app, &e);
            }
        }
        app_shell_frame(&app);
    }

    app_shell_shutdown(&app);
    return 0;
}

#include "shell/app_shell.h"
#include "ui/internal.h"

#include "retr01_studio/json_io.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    AppShell app;
    int running = 1;

    if (app_shell_init(&app, 0) != 0) {
        return 1;
    }

    if (argc >= 2) {
        char err[128];
        if (r01_project_load_json(app.ui.project, argv[1], err, sizeof(err)) != 0) {
            fprintf(stderr, "load project %s: %s\n", argv[1], err);
            app_shell_shutdown(&app);
            return 1;
        }
        snprintf(app.ui.project_path, sizeof(app.ui.project_path), "%s", argv[1]);
        ui_reset_after_project_load(&app.ui);
    }

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = 0;
            } else {
                int rc = app_shell_handle_event(&app, &e);
                if (rc == 3) {
                    running = 0;
                }
            }
        }
        app_shell_frame(&app);
    }

    app_shell_shutdown(&app);
    return 0;
}

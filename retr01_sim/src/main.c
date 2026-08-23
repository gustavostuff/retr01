#include "app.h"

int main(int argc, char **argv) {
    R01sApp app;
    (void)argc;
    (void)argv;

    if (r01s_app_init(&app, 0) != 0) {
        return 1;
    }

    while (app.running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            r01s_app_handle_event(&app, &e);
        }
        r01s_app_frame(&app);
    }

    r01s_app_shutdown(&app);
    return 0;
}

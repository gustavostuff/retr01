#include "shell/app_shell.h"
#include "ui/internal.h"

#include "retr01_studio/json_io.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int path_is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int find_r01proj_in_dir(const char *dir, char *out, size_t out_cap) {
    DIR *d;
    struct dirent *ent;
    if (!dir || !out || out_cap < 2) {
        return -1;
    }
    d = opendir(dir);
    if (!d) {
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        size_t n;
        const char *name = ent->d_name;
        if (!name || name[0] == '.') {
            continue;
        }
        n = strlen(name);
        if (n > 8 && strcmp(name + n - 8, ".r01proj") == 0) {
            if (snprintf(out, out_cap, "%s/%s", dir, name) < (int)out_cap) {
                closedir(d);
                return 0;
            }
        }
    }
    closedir(d);
    return -1;
}

int main(int argc, char **argv) {
    AppShell app;
    int running = 1;
    char proj_buf[512];

    if (app_shell_init(&app, 0) != 0) {
        return 1;
    }

    if (argc >= 2) {
        char err[128];
        const char *path = argv[1];
        if (path_is_dir(path)) {
            if (find_r01proj_in_dir(path, proj_buf, sizeof(proj_buf)) != 0) {
                fprintf(stderr, "no .r01proj in folder: %s\n", path);
                app_shell_shutdown(&app);
                return 1;
            }
            path = proj_buf;
        }
        if (r01_project_load_json(app.ui.project, path, err, sizeof(err)) != 0) {
            fprintf(stderr, "load project %s: %s\n", path, err);
            app_shell_shutdown(&app);
            return 1;
        }
        snprintf(app.ui.project_path, sizeof(app.ui.project_path), "%s", path);
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

#ifndef RETR01_STUDIO_H
#define RETR01_STUDIO_H

#include "retr01/project.h"

#include <SDL.h>
#include <stdbool.h>

typedef enum {
    STUDIO_TOOL_PEN = 0,
    STUDIO_TOOL_FILL = 1,
} studio_tool_t;

typedef struct retr01_studio_app {
    retr01_project_t project;
    bool dirty;
    char status[256];

    studio_tool_t tool;
    int paint_ci;
    int zoom;
    bool show_grid;
    bool show_attr_overlay;
    int pattern_page; /* 0 = BG, 1 = sprites */

    SDL_Renderer *renderer;
    SDL_Texture *sketch_tex;
    SDL_Texture *preview_tex;
    bool sketch_tex_dirty;
    bool preview_tex_dirty;

    int last_px;
    int last_py;

    char path_input[RETR01_PROJECT_PATH_MAX];
    char export_input[RETR01_PROJECT_PATH_MAX];
    bool modal_save_as;
    bool modal_open_proj;
    bool modal_export;
} retr01_studio_app_t;

void retr01_studio_init(retr01_studio_app_t *app, SDL_Renderer *renderer,
                        const char *palette_v01_path);
void retr01_studio_shutdown(retr01_studio_app_t *app);
void retr01_studio_frame(retr01_studio_app_t *app);

#endif

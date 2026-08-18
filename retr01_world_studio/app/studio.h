#ifndef RETR01_STUDIO_H
#define RETR01_STUDIO_H

#include "retr01/project.h"

#include <stdbool.h>

typedef enum {
    STUDIO_PAINT_TILE = 0,
    STUDIO_PAINT_PIXEL = 1,
    STUDIO_PAINT_ATTR = 2,
} studio_paint_mode_t;

typedef struct retr01_studio_app {
    retr01_project_t project;
    bool dirty;
    char status[256];

    studio_paint_mode_t paint_mode;
    int paint_ci;
    int zoom;
    bool show_attr_overlay;

    uint8_t ci_canvas[256 * 240];
    bool ci_valid;

    char path_input[RETR01_PROJECT_PATH_MAX];
    char export_input[RETR01_PROJECT_PATH_MAX];
    bool modal_open;
    bool modal_save_as;
    bool modal_open_proj;
    bool modal_export;
} retr01_studio_app_t;

void retr01_studio_init(retr01_studio_app_t *app, const char *palette_v01_path);
void retr01_studio_shutdown(retr01_studio_app_t *app);
void retr01_studio_frame(retr01_studio_app_t *app);

#endif

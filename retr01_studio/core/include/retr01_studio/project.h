#ifndef RETR01_STUDIO_PROJECT_H
#define RETR01_STUDIO_PROJECT_H

#include "retr01_studio/types.h"

void r01_project_init(R01Project *p, const char *name);
void r01_world_init_phase1(R01World *w);
int r01_world_set_grid(R01World *w, int cols, int rows);
int r01_world_screen_index(const R01World *w, int col, int row);

R01World *r01_project_world0(R01Project *p);
const R01World *r01_project_world0_const(const R01Project *p);
R01Screen *r01_project_active_screen(R01Project *p);
/* Select world start screen (R01_START_COL/ROW) if present, else first present. */
void r01_project_select_start_screen(R01Project *p);
int r01_world_find_screen(const R01World *w, int col, int row);
R01Screen *r01_world_screen_at(R01World *w, int col, int row);

int r01_project_import_png(R01Project *p, const char *path, char *err_buf, size_t err_cap);

int r01_path_ensure_parent(const char *path, char *err_buf, size_t err_cap);

#endif

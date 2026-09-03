#ifndef retr01_STUDIO_PROJECT_H
#define retr01_STUDIO_PROJECT_H

#include "retr01_studio/types.h"

void r01_project_init(R01Project *p, const char *name);
void r01_other_screen_init(R01OtherScreen *s);
void r01_project_init_other_screens(R01Project *p);
void r01_world_init_phase1(R01World *w);
void r01_world_init_empty(R01World *w);
int r01_world_set_grid(R01World *w, int cols, int rows);
int r01_world_screen_index(const R01World *w, int col, int row);

R01World *r01_project_world0(R01Project *p);
const R01World *r01_project_world0_const(const R01Project *p);
R01World *r01_project_active_world(R01Project *p);
const R01World *r01_project_active_world_const(const R01Project *p);
R01Screen *r01_project_active_screen(R01Project *p);
/* Resolve default screen index for a world (stored or start/first present). */
int r01_world_default_screen(const R01World *w);
void r01_world_sync_default_screen(R01World *w);
void r01_project_begin_play(R01Project *p);
/* Select world start screen (R01_START_COL/ROW) if present, else first present. */
void r01_project_select_start_screen(R01Project *p);
int r01_project_set_active_world(R01Project *p, int world_idx);
int r01_world_create_screen(R01World *w, int col, int row);
int r01_world_remove_screen(R01World *w, int col, int row);
int r01_world_find_screen(const R01World *w, int col, int row);
R01Screen *r01_world_screen_at(R01World *w, int col, int row);
/* Count screens with present=1 (cart export uses this; max R01_MAX_PRESENT_SCREENS). */
int r01_world_present_count(const R01World *w);

/* BG0: free layout on 8x8, max R01_BG0_SCREENS_MAX present screens. */
void r01_world_bg0_clear(R01World *w);
void r01_world_bg0_recompute_extent(R01World *w);
int r01_world_bg0_screen_index(const R01World *w, int col, int row);
R01Screen *r01_world_bg0_screen_at(R01World *w, int col, int row);
int r01_world_bg0_create_screen(R01World *w, int col, int row);
int r01_world_bg0_remove_screen(R01World *w, int col, int row);
int r01_world_bg0_present_count(const R01World *w);
R01Screen *r01_project_active_bg0_screen(R01Project *p);

int r01_project_import_png(R01Project *p, const char *path, char *err_buf, size_t err_cap);

int r01_path_ensure_parent(const char *path, char *err_buf, size_t err_cap);
int r01_path_mkdir_p(const char *path, char *err_buf, size_t err_cap);

#endif

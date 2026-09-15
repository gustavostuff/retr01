#ifndef R01_STUDIO_PROJECT_IO_H
#define R01_STUDIO_PROJECT_IO_H

#include "ui/ui.h"

#include <SDL.h>

#define UI_PROJ_IO_NONE 0
#define UI_PROJ_IO_SAVE 1
#define UI_PROJ_IO_OPEN 2

#define UI_PROJ_IO_FIELD_NAME 90
#define UI_PROJ_IO_FIELD_DIR 91

void ui_project_io_init(UiState *ui);
void ui_project_io_open_save(UiState *ui, int force_as);
void ui_project_io_open_browse(UiState *ui);
void ui_project_io_close(UiState *ui);

int ui_project_io_is_open(const UiState *ui);

void ui_project_io_draw(UiState *ui, SDL_Renderer *r);
int ui_project_io_event(UiState *ui, const SDL_Event *e, int lx, int ly);

/* Ctrl+S / Shift+S entry: quiet save or open save modal. */
void ui_project_io_request_save(UiState *ui, int force_as);

#endif

#ifndef R01_STUDIO_PROJECT_IO_INTERNAL_H
#define R01_STUDIO_PROJECT_IO_INTERNAL_H

#include "ui/modals/project_io.h"
#include "ui/internal.h"

#include <stddef.h>

void project_io_refresh_listing(UiState *ui);
void project_io_set_dir(UiState *ui, const char *dir);
void project_io_parent_dir(char *path);
int project_io_selection_is_openable(const UiState *ui, char *out_path, size_t out_cap);
void project_io_enter_selected_dir(UiState *ui);
int project_io_commit_save(UiState *ui);
int project_io_commit_open(UiState *ui);
void project_io_modal_layout(const UiState *ui, ProjectIoModalLayout *lo);
void project_io_default_projects_dir(char *out, size_t cap);
void project_io_sanitize_name(const char *in, char *out, size_t cap);
int project_io_mkdir_p(const char *path);
int project_io_find_proj_in_dir(const char *dir, char *out, size_t out_cap);
int project_io_path_is_dir(const char *path);
int project_io_path_is_file(const char *path);

#endif

#include "ui/modals/project_io_internal.h"
#include "ui/sound/bgm_edit.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/project.h"

#include <stdio.h>
#include <string.h>

void ui_project_io_init(UiState *ui) {
    if (!ui) {
        return;
    }
    memset(&ui->project_io, 0, sizeof(ui->project_io));
    ui->project_io.sel = -1;
}

void ui_project_io_close(UiState *ui) {
    if (!ui) {
        return;
    }
    ui_text_blur(&ui->text);
    ui->project_io.mode = UI_PROJ_IO_NONE;
}

int ui_project_io_is_open(const UiState *ui) {
    return ui && ui->project_io.mode != UI_PROJ_IO_NONE;
}

void ui_project_io_open_save(UiState *ui, int force_as) {
    char dir[R01_PATH_MAX];
    const char *src_name;
    (void)force_as;
    if (!ui) {
        return;
    }
    ui_project_io_close(ui);
    ui->tile_edit.open = 0;
    ui->pal_edit.open = 0;
    ui->sprite_edit.open = 0;
    ui->metasprite_edit.open = 0;
    ui->entity_edit.open = 0;
    ui->menu.open = 0;

    if (ui->project_path[0]) {
        char tmp[R01_PATH_MAX];
        char *slash;
        snprintf(tmp, sizeof(tmp), "%s", ui->project_path);
        slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            project_io_parent_dir(tmp);
            snprintf(dir, sizeof(dir), "%s", tmp);
        } else {
            project_io_default_projects_dir(dir, sizeof(dir));
        }
    } else {
        project_io_default_projects_dir(dir, sizeof(dir));
    }
    project_io_mkdir_p(dir);
    project_io_set_dir(ui, dir);

    src_name = (ui->project && ui->project->name[0]) ? ui->project->name : "untitled";
    project_io_sanitize_name(src_name, ui->project_io.name, sizeof(ui->project_io.name));
    if (!ui->project_io.name[0]) {
        snprintf(ui->project_io.name, sizeof(ui->project_io.name), "untitled");
    }
    ui->project_io.mode = UI_PROJ_IO_SAVE;
    ui->project_io.sel = -1;
    ui_text_focus(&ui->text, ui->project_io.name, (int)sizeof(ui->project_io.name), UI_PROJ_IO_FIELD_NAME);
}

void ui_project_io_open_browse(UiState *ui) {
    char dir[R01_PATH_MAX];
    if (!ui) {
        return;
    }
    ui_project_io_close(ui);
    ui->tile_edit.open = 0;
    ui->pal_edit.open = 0;
    ui->sprite_edit.open = 0;
    ui->metasprite_edit.open = 0;
    ui->entity_edit.open = 0;
    ui->menu.open = 0;

    if (ui->project_path[0]) {
        char tmp[R01_PATH_MAX];
        char *slash;
        snprintf(tmp, sizeof(tmp), "%s", ui->project_path);
        slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            project_io_parent_dir(tmp);
            snprintf(dir, sizeof(dir), "%s", tmp);
        } else {
            project_io_default_projects_dir(dir, sizeof(dir));
        }
    } else {
        project_io_default_projects_dir(dir, sizeof(dir));
    }
    project_io_mkdir_p(dir);
    project_io_set_dir(ui, dir);
    ui->project_io.mode = UI_PROJ_IO_OPEN;
    ui->project_io.sel = -1;
    ui_text_blur(&ui->text);
}

void ui_project_io_request_save(UiState *ui, int force_as) {
    if (!ui) {
        return;
    }
    if (!force_as && ui->project_path[0]) {
        ui_save(ui);
        return;
    }
    ui_project_io_open_save(ui, force_as);
}

int project_io_commit_save(UiState *ui) {
    char folder[R01_PATH_MAX];
    char proj_path[R01_PATH_MAX];
    char name[R01_NAME_MAX];
    char err[128];

    project_io_sanitize_name(ui->project_io.name, name, sizeof(name));
    if (!name[0]) {
        ui_toast(ui, "enter a project name", 1);
        return -1;
    }
    if (!ui->project_io.dir[0]) {
        ui_toast(ui, "pick a parent folder", 1);
        return -1;
    }
    if (snprintf(folder, sizeof(folder), "%s/%s", ui->project_io.dir, name) >= (int)sizeof(folder)) {
        ui_toast(ui, "path too long", 1);
        return -1;
    }
    if (project_io_mkdir_p(folder) != 0) {
        ui_toast(ui, "could not create project folder", 1);
        return -1;
    }
    if (snprintf(proj_path, sizeof(proj_path), "%s/%s.r01proj", folder, name) >= (int)sizeof(proj_path)) {
        ui_toast(ui, "path too long", 1);
        return -1;
    }
    snprintf(ui->project->name, sizeof(ui->project->name), "%s", name);
    ui_bgm_sync_to_project(ui);
    if (r01_project_save_json(ui->project, proj_path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return -1;
    }
    snprintf(ui->project_path, sizeof(ui->project_path), "%s", proj_path);
    ui_project_io_close(ui);
    ui_toast(ui, "project saved", 0);
    return 0;
}

int project_io_commit_open(UiState *ui) {
    char path[R01_PATH_MAX];
    char err[128];
    if (!project_io_selection_is_openable(ui, path, sizeof(path))) {
        ui_toast(ui, "select a project folder or .r01proj", 1);
        return -1;
    }
    if (r01_project_load_json(ui->project, path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return -1;
    }
    snprintf(ui->project_path, sizeof(ui->project_path), "%s", path);
    ui_project_io_close(ui);
    ui_reset_after_project_load(ui);
    ui_toast(ui, "project loaded", 0);
    return 0;
}


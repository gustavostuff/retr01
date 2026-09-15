#include "ui/modals/project_io.h"
#include "ui/internal.h"
#include "ui/sound/bgm_edit.h"
#include "font/font.h"

#include "retr01_studio/json_io.h"
#include "retr01_studio/paths.h"
#include "retr01_studio/project.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef R01_STUDIO_PROJECTS_DIR
#define R01_STUDIO_PROJECTS_DIR "apps/studio/projects"
#endif

static int path_is_dir(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int path_is_file(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int ends_with_ci(const char *path, const char *suffix) {
    size_t pl, sl;
    const char *p;
    if (!path || !suffix) {
        return 0;
    }
    pl = strlen(path);
    sl = strlen(suffix);
    if (pl < sl) {
        return 0;
    }
    p = path + pl - sl;
    while (*suffix) {
        char a = *p++;
        char b = *suffix++;
        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }
        if (a != b) {
            return 0;
        }
    }
    return 1;
}

static void default_projects_dir(char *out, size_t cap) {
    if (r01_path_resolve(R01_STUDIO_PROJECTS_DIR, out, cap) != 0) {
        snprintf(out, cap, "%s", R01_STUDIO_PROJECTS_DIR);
    }
}

static void sanitize_project_name(const char *in, char *out, size_t cap) {
    size_t w = 0;
    if (!out || cap < 2) {
        return;
    }
    out[0] = '\0';
    if (!in) {
        return;
    }
    while (*in && w + 1 < cap) {
        unsigned char c = (unsigned char)*in++;
        if (isalnum(c) || c == '-' || c == '_') {
            out[w++] = (char)c;
        } else if (c == ' ' || c == '.') {
            out[w++] = '_';
        }
    }
    out[w] = '\0';
    while (w > 0 && out[w - 1] == '_') {
        out[--w] = '\0';
    }
}

static int mkdir_p(const char *path) {
    char tmp[R01_PATH_MAX];
    size_t len;
    size_t i;
    if (!path || !path[0]) {
        return -1;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len == 0) {
        return -1;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            if (tmp[0] && !path_is_dir(tmp)) {
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return -1;
                }
            }
            tmp[i] = '/';
        }
    }
    if (!path_is_dir(tmp)) {
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

static int cmp_entry(const void *a, const void *b) {
    const char *sa = (const char *)a;
    const char *sb = (const char *)b;
    int da = (sa[0] == '/');
    int db = (sb[0] == '/');
    if (da != db) {
        return db - da;
    }
    return strcasecmp(sa + da, sb + db);
}

static void refresh_listing(UiState *ui) {
    UiProjectIo *pio;
    DIR *d;
    struct dirent *ent;
    char path[R01_PATH_MAX];
    int n = 0;

    if (!ui) {
        return;
    }
    pio = &ui->project_io;
    pio->entry_count = 0;
    pio->sel = -1;
    pio->scroll = 0;
    if (!pio->dir[0] || !path_is_dir(pio->dir)) {
        return;
    }
    d = opendir(pio->dir);
    if (!d) {
        return;
    }
    while ((ent = readdir(d)) != NULL && n < UI_PROJ_IO_MAX_ENTRIES) {
        const char *name = ent->d_name;
        int is_dir;
        if (!name || strcmp(name, ".") == 0) {
            continue;
        }
        if (strcmp(name, "..") == 0) {
            snprintf(pio->entries[n], sizeof(pio->entries[n]), "/..");
            pio->entry_is_dir[n] = 1;
            n++;
            continue;
        }
        if (name[0] == '.') {
            continue;
        }
        if (snprintf(path, sizeof(path), "%s/%s", pio->dir, name) >= (int)sizeof(path)) {
            continue;
        }
        is_dir = path_is_dir(path);
        if (!is_dir && !ends_with_ci(name, ".r01proj")) {
            continue;
        }
        if (is_dir) {
            snprintf(pio->entries[n], sizeof(pio->entries[n]), "/%s", name);
        } else {
            snprintf(pio->entries[n], sizeof(pio->entries[n]), "%s", name);
        }
        pio->entry_is_dir[n] = is_dir;
        n++;
    }
    closedir(d);
    qsort(pio->entries, (size_t)n, sizeof(pio->entries[0]), cmp_entry);
    for (int i = 0; i < n; i++) {
        pio->entry_is_dir[i] = (pio->entries[i][0] == '/');
    }
    pio->entry_count = n;
}

static void set_dir(UiState *ui, const char *dir) {
    char resolved[R01_PATH_MAX];
    if (!ui || !dir) {
        return;
    }
    if (realpath(dir, resolved)) {
        snprintf(ui->project_io.dir, sizeof(ui->project_io.dir), "%s", resolved);
    } else {
        snprintf(ui->project_io.dir, sizeof(ui->project_io.dir), "%s", dir);
    }
    refresh_listing(ui);
}

static void parent_dir(char *path) {
    char *slash;
    size_t n;
    if (!path || !path[0]) {
        return;
    }
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        path[--n] = '\0';
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return;
    }
    if (slash == path) {
        path[1] = '\0';
        return;
    }
    *slash = '\0';
}

static int find_proj_in_dir(const char *dir, char *out, size_t out_cap) {
    DIR *d;
    struct dirent *ent;
    char preferred[R01_PATH_MAX];
    const char *base;
    if (!dir || !out || out_cap < 2) {
        return -1;
    }
    base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    if (snprintf(preferred, sizeof(preferred), "%s/%s.r01proj", dir, base) < (int)sizeof(preferred) &&
        path_is_file(preferred)) {
        snprintf(out, out_cap, "%s", preferred);
        return 0;
    }
    d = opendir(dir);
    if (!d) {
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n > 8 && strcmp(ent->d_name + n - 8, ".r01proj") == 0) {
            snprintf(out, out_cap, "%s/%s", dir, ent->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -1;
}

static int selection_is_openable(const UiState *ui, char *out_path, size_t out_cap) {
    const UiProjectIo *pio;
    const char *ent;
    char path[R01_PATH_MAX];
    if (!ui || ui->project_io.sel < 0 || ui->project_io.sel >= ui->project_io.entry_count) {
        return 0;
    }
    pio = &ui->project_io;
    ent = pio->entries[pio->sel];
    if (pio->entry_is_dir[pio->sel]) {
        if (strcmp(ent, "/..") == 0) {
            return 0;
        }
        if (snprintf(path, sizeof(path), "%s/%s", pio->dir, ent + 1) >= (int)sizeof(path)) {
            return 0;
        }
        return find_proj_in_dir(path, out_path, out_cap) == 0;
    }
    if (snprintf(path, sizeof(path), "%s/%s", pio->dir, ent) >= (int)sizeof(path)) {
        return 0;
    }
    if (!ends_with_ci(path, ".r01proj") || !path_is_file(path)) {
        return 0;
    }
    snprintf(out_path, out_cap, "%s", path);
    return 1;
}

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
    ui_text_blur(ui);
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
            parent_dir(tmp);
            snprintf(dir, sizeof(dir), "%s", tmp);
        } else {
            default_projects_dir(dir, sizeof(dir));
        }
    } else {
        default_projects_dir(dir, sizeof(dir));
    }
    mkdir_p(dir);
    set_dir(ui, dir);

    src_name = (ui->project && ui->project->name[0]) ? ui->project->name : "untitled";
    sanitize_project_name(src_name, ui->project_io.name, sizeof(ui->project_io.name));
    if (!ui->project_io.name[0]) {
        snprintf(ui->project_io.name, sizeof(ui->project_io.name), "untitled");
    }
    ui->project_io.mode = UI_PROJ_IO_SAVE;
    ui->project_io.sel = -1;
    ui_text_focus(ui, ui->project_io.name, (int)sizeof(ui->project_io.name), UI_PROJ_IO_FIELD_NAME);
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
            parent_dir(tmp);
            snprintf(dir, sizeof(dir), "%s", tmp);
        } else {
            default_projects_dir(dir, sizeof(dir));
        }
    } else {
        default_projects_dir(dir, sizeof(dir));
    }
    mkdir_p(dir);
    set_dir(ui, dir);
    ui->project_io.mode = UI_PROJ_IO_OPEN;
    ui->project_io.sel = -1;
    ui_text_blur(ui);
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

static int commit_save(UiState *ui) {
    char folder[R01_PATH_MAX];
    char proj_path[R01_PATH_MAX];
    char name[R01_NAME_MAX];
    char err[128];

    sanitize_project_name(ui->project_io.name, name, sizeof(name));
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
    if (mkdir_p(folder) != 0) {
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

static int commit_open(UiState *ui) {
    char path[R01_PATH_MAX];
    char err[128];
    if (!selection_is_openable(ui, path, sizeof(path))) {
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

static void enter_selected_dir(UiState *ui) {
    UiProjectIo *pio = &ui->project_io;
    char next[R01_PATH_MAX];
    const char *ent;
    if (pio->sel < 0 || pio->sel >= pio->entry_count || !pio->entry_is_dir[pio->sel]) {
        return;
    }
    ent = pio->entries[pio->sel];
    if (strcmp(ent, "/..") == 0) {
        snprintf(next, sizeof(next), "%s", pio->dir);
        parent_dir(next);
        set_dir(ui, next);
        return;
    }
    if (snprintf(next, sizeof(next), "%s/%s", pio->dir, ent + 1) >= (int)sizeof(next)) {
        return;
    }
    set_dir(ui, next);
}

void ui_project_io_draw(UiState *ui, SDL_Renderer *r) {
    UiProjectIo *pio;
    int mx, my, mw, mh;
    int list_y, list_h, row_h, visible;
    int btn_y, save_w = 72, cancel_w = 72, open_w = 72;
    int name_y, dir_y;
    int i;
    char openable[R01_PATH_MAX];
    int can_open;

    if (!ui || !r || !ui_project_io_is_open(ui)) {
        return;
    }
    pio = &ui->project_io;
    mw = 420;
    mh = 280;
    mx = (ui_logic_w(ui) - mw) / 2;
    my = (ui_logic_h(ui) - mh) / 2;

    ui_modal_scrim(r, ui);
    ui_modal_panel(r, mx, my, mw, mh, pio->mode == UI_PROJ_IO_SAVE ? "Save project" : "Open project");

    dir_y = my + UI_BTN_H + UI_UNIT;
    font_draw(r, mx + UI_UNIT, dir_y, "Folder", 180, 180, 180);
    ui_text_draw(ui, r, mx + UI_UNIT + 48, dir_y, mw - UI_UNIT * 2 - 48, pio->dir, UI_PROJ_IO_FIELD_DIR);

    name_y = dir_y + UI_BTN_H + UI_UNIT / 2;
    if (pio->mode == UI_PROJ_IO_SAVE) {
        font_draw(r, mx + UI_UNIT, name_y, "Name", 180, 180, 180);
        ui_text_draw(ui, r, mx + UI_UNIT + 48, name_y, mw - UI_UNIT * 2 - 48, pio->name, UI_PROJ_IO_FIELD_NAME);
        list_y = name_y + UI_BTN_H + UI_UNIT / 2;
    } else {
        list_y = name_y;
    }

    btn_y = my + mh - UI_BTN_H - UI_UNIT;
    list_h = btn_y - list_y - UI_UNIT;
    row_h = UI_BTN_H - 2;
    visible = list_h / row_h;
    if (visible < 1) {
        visible = 1;
    }
    if (pio->scroll > pio->entry_count - visible) {
        pio->scroll = pio->entry_count - visible;
    }
    if (pio->scroll < 0) {
        pio->scroll = 0;
    }

    fill_rect(r, mx + UI_UNIT, list_y, mw - UI_UNIT * 2, list_h, 20, 24, 28);
    draw_rect(r, mx + UI_UNIT, list_y, mw - UI_UNIT * 2, list_h, 60, 60, 60);
    for (i = 0; i < visible; i++) {
        int idx = pio->scroll + i;
        int y = list_y + i * row_h;
        const char *label;
        int sel;
        if (idx >= pio->entry_count) {
            break;
        }
        sel = (idx == pio->sel);
        if (sel) {
            fill_rect(r, mx + UI_UNIT + 1, y + 1, mw - UI_UNIT * 2 - 2, row_h - 1, 40, 70, 100);
        }
        label = pio->entry_is_dir[idx] ? pio->entries[idx] + 1 : pio->entries[idx];
        if (pio->entry_is_dir[idx]) {
            font_draw(r, mx + UI_UNIT * 2, y + 4, label[0] ? label : "..", 220, 200, 120);
        } else {
            font_draw(r, mx + UI_UNIT * 2, y + 4, label, 220, 220, 220);
        }
    }

    can_open = selection_is_openable(ui, openable, sizeof(openable));
    if (pio->mode == UI_PROJ_IO_SAVE) {
        ui_modal_save_cancel(r, mx + UI_UNIT, btn_y, save_w, cancel_w, ui->mouse_x, ui->mouse_y);
    } else {
        int open_hover = point_in_rect(ui->mouse_x, ui->mouse_y, mx + UI_UNIT, btn_y, open_w, UI_BTN_H);
        int cancel_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, mx + UI_UNIT + open_w + UI_UNIT, btn_y, cancel_w, UI_BTN_H);
        ui_button_draw(r, mx + UI_UNIT, btn_y, open_w, "Open", can_open, open_hover && can_open);
        ui_button_draw(r, mx + UI_UNIT + open_w + UI_UNIT, btn_y, cancel_w, "Cancel", 0, cancel_hover);
    }
}

int ui_project_io_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    UiProjectIo *pio;
    int mx, my, mw, mh;
    int list_y, list_h, row_h, visible;
    int btn_y, save_w = 72, cancel_w = 72, open_w = 72;
    int name_y, dir_y;
    int dir_field_y;

    if (!ui || !e || !ui_project_io_is_open(ui)) {
        return 0;
    }
    pio = &ui->project_io;
    mw = 420;
    mh = 280;
    mx = (ui_logic_w(ui) - mw) / 2;
    my = (ui_logic_h(ui) - mh) / 2;
    dir_y = my + UI_BTN_H + UI_UNIT;
    dir_field_y = dir_y;
    name_y = dir_y + UI_BTN_H + UI_UNIT / 2;
    btn_y = my + mh - UI_BTN_H - UI_UNIT;
    list_y = (pio->mode == UI_PROJ_IO_SAVE) ? (name_y + UI_BTN_H + UI_UNIT / 2) : name_y;
    list_h = btn_y - list_y - UI_UNIT;
    row_h = UI_BTN_H - 2;
    visible = list_h / row_h;
    if (visible < 1) {
        visible = 1;
    }

    if (e->type == SDL_TEXTINPUT) {
        if (ui->text.field_id == UI_PROJ_IO_FIELD_NAME || ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
            ui_text_input(ui, e->text.text);
            if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
                refresh_listing(ui);
            }
            return 1;
        }
        return 1;
    }

    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            ui_project_io_close(ui);
            return 1;
        }
        if (ui->text.field_id > 0) {
            if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) {
                if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
                    set_dir(ui, pio->dir);
                    ui_text_blur(ui);
                } else if (pio->mode == UI_PROJ_IO_SAVE) {
                    (void)commit_save(ui);
                }
                return 1;
            }
            ui_text_key(ui, e->key.keysym.sym, e->key.keysym.mod);
            if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
                refresh_listing(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) {
            if (pio->mode == UI_PROJ_IO_SAVE) {
                (void)commit_save(ui);
            } else if (pio->sel >= 0 && pio->entry_is_dir[pio->sel]) {
                enter_selected_dir(ui);
            } else {
                (void)commit_open(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_UP) {
            if (pio->sel > 0) {
                pio->sel--;
            } else if (pio->entry_count > 0) {
                pio->sel = 0;
            }
            if (pio->sel < pio->scroll) {
                pio->scroll = pio->sel;
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_DOWN) {
            if (pio->sel + 1 < pio->entry_count) {
                pio->sel++;
            } else if (pio->entry_count > 0 && pio->sel < 0) {
                pio->sel = 0;
            }
            if (pio->sel >= pio->scroll + visible) {
                pio->scroll = pio->sel - visible + 1;
            }
            return 1;
        }
        return 1;
    }

    if (e->type == SDL_MOUSEWHEEL) {
        pio->scroll -= e->wheel.y;
        if (pio->scroll < 0) {
            pio->scroll = 0;
        }
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        ui_text_mouse_up(ui);
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int i;
        if (ui_modal_overlay_hit(lx, ly, mx, my, mw, mh)) {
            ui_project_io_close(ui);
            return 1;
        }
        if (ui_text_mouse_down(ui, lx, ly, mx + UI_UNIT + 48, dir_field_y, mw - UI_UNIT * 2 - 48, pio->dir,
                               (int)sizeof(pio->dir), UI_PROJ_IO_FIELD_DIR)) {
            return 1;
        }
        if (pio->mode == UI_PROJ_IO_SAVE &&
            ui_text_mouse_down(ui, lx, ly, mx + UI_UNIT + 48, name_y, mw - UI_UNIT * 2 - 48, pio->name,
                               (int)sizeof(pio->name), UI_PROJ_IO_FIELD_NAME)) {
            return 1;
        }
        if (point_in_rect(lx, ly, mx + UI_UNIT, list_y, mw - UI_UNIT * 2, list_h)) {
            int row = (ly - list_y) / row_h;
            int idx = pio->scroll + row;
            if (idx >= 0 && idx < pio->entry_count) {
                Uint32 now = SDL_GetTicks();
                if (pio->sel == idx && now - pio->last_click_ms < 400) {
                    if (pio->entry_is_dir[idx]) {
                        enter_selected_dir(ui);
                    } else if (pio->mode == UI_PROJ_IO_OPEN) {
                        (void)commit_open(ui);
                    }
                } else {
                    pio->sel = idx;
                }
                pio->last_click_ms = now;
            }
            ui_text_blur(ui);
            return 1;
        }
        if (pio->mode == UI_PROJ_IO_SAVE) {
            if (ui_modal_save_hit(lx, ly, mx + UI_UNIT, btn_y, save_w)) {
                (void)commit_save(ui);
                return 1;
            }
            if (ui_modal_cancel_hit(lx, ly, mx + UI_UNIT, btn_y, save_w, cancel_w)) {
                ui_project_io_close(ui);
                return 1;
            }
        } else {
            if (point_in_rect(lx, ly, mx + UI_UNIT, btn_y, open_w, UI_BTN_H)) {
                (void)commit_open(ui);
                return 1;
            }
            if (point_in_rect(lx, ly, mx + UI_UNIT + open_w + UI_UNIT, btn_y, cancel_w, UI_BTN_H)) {
                ui_project_io_close(ui);
                return 1;
            }
        }
        (void)i;
        ui_text_blur(ui);
        return 1;
    }

    if (e->type == SDL_MOUSEMOTION && ui->text.drag) {
        if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
            ui_text_mouse_drag(ui, lx, mx + UI_UNIT + 48, mw - UI_UNIT * 2 - 48);
        } else if (ui->text.field_id == UI_PROJ_IO_FIELD_NAME) {
            ui_text_mouse_drag(ui, lx, mx + UI_UNIT + 48, mw - UI_UNIT * 2 - 48);
        }
        return 1;
    }

    return 1;
}

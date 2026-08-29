#include "ui/ui.h"
#include "ui/internal.h"
#include "font/font.h"

#include "retr01_studio/cart.h"
#include "retr01_studio/paths.h"
#include "retr01_studio/chr_pack.h"
#include "retr01_studio/json_io.h"
#include "retr01_studio/palette.h"
#include "retr01_studio/project.h"

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int uri_hex(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void uri_decode_inplace(char *s) {
    char *r = s;
    char *w = s;
    while (*r) {
        if (r[0] == '%' && r[1] && r[2]) {
            int hi = uri_hex((unsigned char)r[1]);
            int lo = uri_hex((unsigned char)r[2]);
            if (hi >= 0 && lo >= 0) {
                *w++ = (char)(hi * 16 + lo);
                r += 3;
                continue;
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static void normalize_drop_path(char *path) {
    char *p;
    size_t n;
    if (!path || !path[0]) {
        return;
    }
    n = strlen(path);
    while (n > 0 && (path[n - 1] == '\r' || path[n - 1] == '\n' || path[n - 1] == ' ')) {
        path[--n] = '\0';
    }
    if (strncmp(path, "file://", 7) == 0) {
        p = path + 7;
        if (*p == '/') {
            memmove(path, p, strlen(p) + 1u);
        } else {
            char *slash = strchr(p, '/');
            if (slash) {
                memmove(path, slash, strlen(slash) + 1u);
            } else {
                memmove(path, p, strlen(p) + 1u);
            }
        }
    } else if (strncmp(path, "file:/", 6) == 0) {
        memmove(path, path + 5, strlen(path + 5) + 1u);
    }
    uri_decode_inplace(path);
}

static void resolve_drop_path(const char *in, char *out, size_t out_cap) {
    char tmp[R01_PATH_MAX];
    char resolved[R01_PATH_MAX];
    if (!out || out_cap < 1) {
        return;
    }
    snprintf(tmp, sizeof(tmp), "%s", in ? in : "");
    normalize_drop_path(tmp);
    if (realpath(tmp, resolved)) {
        snprintf(out, out_cap, "%s", resolved);
        return;
    }
    snprintf(out, out_cap, "%s", tmp);
}

static int path_ends_with_ci(const char *path, const char *suffix) {
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

void ui_reset_after_project_load(UiState *ui) {
    if (!ui) {
        return;
    }
    if (ui->play.active) {
        r01_play_stop(&ui->play);
    }
    ui->tile_edit.open = 0;
    ui->pal_edit.open = 0;
    ui->sprite_edit.open = 0;
    ui->metasprite_edit.open = 0;
    ui->entity_edit.open = 0;
    ui->menu.open = 0;
    ui->menu.submenu = UI_MENU_SUB_NONE;
    ui->sprites_scroll = 0;
    ui->metasprites_scroll = 0;
    ui->entities_scroll = 0;
    ui->sel_instance = -1;
    ui->catalog_drag.active = 0;
    screen_sel_clear(ui);
    ui->paint_stamp_valid = 0;
    ui->last_paint_tx = -1;
    ui->last_paint_ty = -1;
}

void ui_save(UiState *ui) {
    char err[128];
    if (!ui->project_path[0]) {
        ui_toast(ui, "drop a .r01proj to save", 1);
        return;
    }
    if (r01_project_save_json(ui->project, ui->project_path, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "%s saved", ui->project_path);
        ui_toast(ui, msg, 0);
    }
}

void ui_export(UiState *ui) {
    char stem[R01_PATH_MAX];
    char err[128];
    if (r01_path_resolve(R01_DEFAULT_CART_STEM, stem, sizeof(stem)) != 0) {
        ui_toast(ui, "export path failed", 1);
        return;
    }
    if (r01_export_bundle(ui->project, stem, err, sizeof(err)) != 0) {
        ui_toast(ui, err, 1);
        return;
    }
    ui_toast(ui, R01_ROM_DIR "/test.retr01 exported", 0);
}

int ui_handle_drop_file(UiState *ui, const char *path, int lx, int ly) {
    char err[128];
    char local[R01_PATH_MAX];
    (void)lx;
    (void)ly;
    if (!ui || !path) {
        return 0;
    }
    resolve_drop_path(path, local, sizeof(local));
    if (path_ends_with_ci(local, ".r01proj") || path_ends_with_ci(local, ".json")) {
        if (r01_project_load_json(ui->project, local, err, sizeof(err)) != 0) {
            ui_toast(ui, err, 1);
            return 1;
        }
        snprintf(ui->project_path, sizeof(ui->project_path), "%s", local);
        ui_reset_after_project_load(ui);
        ui_toast(ui, "project loaded", 0);
        return 1;
    }
    if (path_ends_with_ci(local, ".png")) {
        if (r01_project_import_png(ui->project, local, err, sizeof(err)) != 0) {
            ui_toast(ui, err, 1);
            return 1;
        }
        r01_project_select_start_screen(ui->project);
        ui_toast(ui, "png imported", 0);
        return 1;
    }
    ui_toast(ui, "drop a .r01proj or .png", 1);
    return 1;
}

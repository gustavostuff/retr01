#include "ui/modals/project_io_internal.h"
#include "font/font.h"

#include "retr01_studio/project.h"

#include <stdio.h>
#include <string.h>

void project_io_modal_layout(const UiState *ui, ProjectIoModalLayout *lo) {
    enum { C_DIR = 1, C_NAME, C_LIST, C_FOOTER };
    const int is_save = ui && ui->project_io.mode == UI_PROJ_IO_SAVE;
    static const UiPanelCell cells_save[] = {
        {C_DIR, 0, 0, 1, 1},
        {C_NAME, 0, 2, 1, 1},
        {C_LIST, 0, 4, 1, 1},
        {C_FOOTER, 0, 6, 1, 1},
    };
    static const UiPanelCell cells_open[] = {
        {C_DIR, 0, 0, 1, 1},
        {C_LIST, 0, 2, 1, 1},
        {C_FOOTER, 0, 4, 1, 1},
    };
    static const int row_hs_save[] = {UI_BTN_H, UI_UNIT, UI_BTN_H, UI_UNIT, 144, UI_UNIT, UI_BTN_H};
    static const int row_hs_open[] = {UI_BTN_H, UI_UNIT, 176, UI_UNIT, UI_BTN_H};
    UiPanel panel;
    int pad = UI_UNIT;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int content_w = 420 - pad * 2;
    int i;
    int rows;
    const UiPanelCell *cells;
    int cell_n;
    const int *row_hs;

    if (is_save) {
        cells = cells_save;
        cell_n = (int)(sizeof(cells_save) / sizeof(cells_save[0]));
        row_hs = row_hs_save;
        rows = 7;
    } else {
        cells = cells_open;
        cell_n = (int)(sizeof(cells_open) / sizeof(cells_open[0]));
        row_hs = row_hs_open;
        rows = 5;
    }

    ui_panel_init(&panel, 1, rows, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, cell_n);
    ui_panel_set_col_w(&panel, 0, content_w);
    for (i = 0; i < rows; i++) {
        ui_panel_set_row_h(&panel, i, row_hs[i]);
    }
    ui_panel_layout(&panel, 0, 0);

    lo->mw = pad + panel.total_w + pad;
    lo->mh = UI_BTN_H + pad + panel.total_h + pad;
    lo->mx = (ui_logic_w(ui) - lo->mw) / 2;
    lo->my = (ui_logic_h(ui) - lo->mh) / 2;
    content_x = lo->mx + pad;
    content_y = lo->my + UI_BTN_H + pad;
    ui_panel_layout(&panel, content_x, content_y);

    ui_panel_cell(&panel, C_DIR, &cx, &cy, &cw, &ch);
    lo->dir_y = cy;
    lo->dir_x = cx + 48;
    lo->dir_w = cw - 48;
    if (lo->dir_w < UI_UNIT * 8) {
        lo->dir_w = UI_UNIT * 8;
    }

    if (is_save) {
        ui_panel_cell(&panel, C_NAME, &cx, &cy, &cw, &ch);
        lo->name_y = cy;
        lo->name_x = cx + 48;
        lo->name_w = cw - 48;
        if (lo->name_w < UI_UNIT * 8) {
            lo->name_w = UI_UNIT * 8;
        }
    } else {
        lo->name_x = lo->dir_x;
        lo->name_y = lo->dir_y;
        lo->name_w = lo->dir_w;
    }

    ui_panel_cell(&panel, C_LIST, &cx, &cy, &cw, &ch);
    lo->list_x = cx;
    lo->list_y = cy;
    lo->list_w = cw;
    lo->list_h = ch;

    ui_panel_cell(&panel, C_FOOTER, &cx, &cy, &cw, &ch);
    lo->btn_y = cy;
    lo->left_btn_x = content_x;
    lo->save_w = label_width("Save");
    lo->cancel_w = label_width("Cancel");
    lo->open_w = label_width("Open");
#if UI_PANEL_DEBUG_GRID
    lo->dbg_panel = panel;
#endif
}

void ui_project_io_draw(UiState *ui, SDL_Renderer *r) {
    UiProjectIo *pio;
    ProjectIoModalLayout lo;
    int row_h, visible;
    int i;
    char openable[R01_PATH_MAX];
    int can_open;

    if (!ui || !r || !ui_project_io_is_open(ui)) {
        return;
    }
    pio = &ui->project_io;
    project_io_modal_layout(ui, &lo);

    ui_modal_scrim(r, ui_logic_w(ui), ui_logic_h(ui));
    ui_modal_panel(r, lo.mx, lo.my, lo.mw, lo.mh, pio->mode == UI_PROJ_IO_SAVE ? "Save project" : "Open project");
#if UI_PANEL_DEBUG_GRID
    ui_panel_debug_draw(r, &lo.dbg_panel);
#endif

    font_draw(r, lo.left_btn_x, lo.dir_y + 4, "Folder", 180, 180, 180);
    ui_text_draw(&ui->text, r, lo.dir_x, lo.dir_y, lo.dir_w, pio->dir, UI_PROJ_IO_FIELD_DIR);

    if (pio->mode == UI_PROJ_IO_SAVE) {
        font_draw(r, lo.left_btn_x, lo.name_y + 4, "Name", 180, 180, 180);
        ui_text_draw(&ui->text, r, lo.name_x, lo.name_y, lo.name_w, pio->name, UI_PROJ_IO_FIELD_NAME);
    }

    row_h = UI_BTN_H - 2;
    visible = lo.list_h / row_h;
    if (visible < 1) {
        visible = 1;
    }
    if (pio->scroll > pio->entry_count - visible) {
        pio->scroll = pio->entry_count - visible;
    }
    if (pio->scroll < 0) {
        pio->scroll = 0;
    }

    fill_rect(r, lo.list_x, lo.list_y, lo.list_w, lo.list_h, 20, 24, 28);
    for (i = 0; i < visible; i++) {
        int idx = pio->scroll + i;
        int y = lo.list_y + i * row_h;
        const char *label;
        int sel;
        if (idx >= pio->entry_count) {
            break;
        }
        sel = (idx == pio->sel);
        if (sel) {
            fill_rect(r, lo.list_x + 1, y + 1, lo.list_w - 2, row_h - 1, 40, 70, 100);
        }
        label = pio->entry_is_dir[idx] ? pio->entries[idx] + 1 : pio->entries[idx];
        if (pio->entry_is_dir[idx]) {
            font_draw(r, lo.list_x + UI_UNIT, y + 4, label[0] ? label : "..", 220, 200, 120);
        } else {
            font_draw(r, lo.list_x + UI_UNIT, y + 4, label, 220, 220, 220);
        }
    }

    can_open = project_io_selection_is_openable(ui, openable, sizeof(openable));
    if (pio->mode == UI_PROJ_IO_SAVE) {
        ui_modal_save_cancel(r, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w, ui->mouse_x, ui->mouse_y);
    } else {
        int open_hover = point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_btn_x, lo.btn_y, lo.open_w, UI_BTN_H);
        int cancel_hover =
            point_in_rect(ui->mouse_x, ui->mouse_y, lo.left_btn_x + lo.open_w + UI_UNIT, lo.btn_y, lo.cancel_w,
                          UI_BTN_H);
        ui_button_draw(r, lo.left_btn_x, lo.btn_y, lo.open_w, "Open", can_open, open_hover && can_open);
        ui_button_draw(r, lo.left_btn_x + lo.open_w + UI_UNIT, lo.btn_y, lo.cancel_w, "Cancel", 0, cancel_hover);
    }
}

int ui_project_io_event(UiState *ui, const SDL_Event *e, int lx, int ly) {
    UiProjectIo *pio;
    ProjectIoModalLayout lo;
    int row_h, visible;

    if (!ui || !e || !ui_project_io_is_open(ui)) {
        return 0;
    }
    pio = &ui->project_io;
    project_io_modal_layout(ui, &lo);
    row_h = UI_BTN_H - 2;
    visible = lo.list_h / row_h;
    if (visible < 1) {
        visible = 1;
    }

    if (e->type == SDL_TEXTINPUT) {
        if (ui->text.field_id == UI_PROJ_IO_FIELD_NAME || ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
            ui_text_input(&ui->text, e->text.text);
            if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
                project_io_refresh_listing(ui);
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
                    project_io_set_dir(ui, pio->dir);
                    ui_text_blur(&ui->text);
                } else if (pio->mode == UI_PROJ_IO_SAVE) {
                    (void)project_io_commit_save(ui);
                }
                return 1;
            }
            ui_text_key(&ui->text, e->key.keysym.sym, e->key.keysym.mod);
            if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
                project_io_refresh_listing(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) {
            if (pio->mode == UI_PROJ_IO_SAVE) {
                (void)project_io_commit_save(ui);
            } else if (pio->sel >= 0 && pio->entry_is_dir[pio->sel]) {
                project_io_enter_selected_dir(ui);
            } else {
                (void)project_io_commit_open(ui);
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
        ui_text_mouse_up(&ui->text);
        return 1;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        if (ui_modal_overlay_hit(lx, ly, lo.mx, lo.my, lo.mw, lo.mh)) {
            ui_project_io_close(ui);
            return 1;
        }
        if (ui_text_mouse_down(&ui->text, lx, ly, lo.dir_x, lo.dir_y, lo.dir_w, pio->dir, (int)sizeof(pio->dir),
                               UI_PROJ_IO_FIELD_DIR)) {
            return 1;
        }
        if (pio->mode == UI_PROJ_IO_SAVE &&
            ui_text_mouse_down(&ui->text, lx, ly, lo.name_x, lo.name_y, lo.name_w, pio->name, (int)sizeof(pio->name),
                               UI_PROJ_IO_FIELD_NAME)) {
            return 1;
        }
        if (point_in_rect(lx, ly, lo.list_x, lo.list_y, lo.list_w, lo.list_h)) {
            int row = (ly - lo.list_y) / row_h;
            int idx = pio->scroll + row;
            if (idx >= 0 && idx < pio->entry_count) {
                Uint32 now = SDL_GetTicks();
                if (pio->sel == idx && now - pio->last_click_ms < 400) {
                    if (pio->entry_is_dir[idx]) {
                        project_io_enter_selected_dir(ui);
                    } else if (pio->mode == UI_PROJ_IO_OPEN) {
                        (void)project_io_commit_open(ui);
                    }
                } else {
                    pio->sel = idx;
                }
                pio->last_click_ms = now;
            }
            ui_text_blur(&ui->text);
            return 1;
        }
        if (pio->mode == UI_PROJ_IO_SAVE) {
            if (ui_modal_save_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w)) {
                (void)project_io_commit_save(ui);
                return 1;
            }
            if (ui_modal_cancel_hit(lx, ly, lo.left_btn_x, lo.btn_y, lo.save_w, lo.cancel_w)) {
                ui_project_io_close(ui);
                return 1;
            }
        } else {
            if (point_in_rect(lx, ly, lo.left_btn_x, lo.btn_y, lo.open_w, UI_BTN_H)) {
                (void)project_io_commit_open(ui);
                return 1;
            }
            if (point_in_rect(lx, ly, lo.left_btn_x + lo.open_w + UI_UNIT, lo.btn_y, lo.cancel_w, UI_BTN_H)) {
                ui_project_io_close(ui);
                return 1;
            }
        }
        ui_text_blur(&ui->text);
        return 1;
    }

    if (e->type == SDL_MOUSEMOTION && ui->text.drag) {
        if (ui->text.field_id == UI_PROJ_IO_FIELD_DIR) {
            ui_text_mouse_drag(&ui->text, lx, lo.dir_x, lo.dir_w);
        } else if (ui->text.field_id == UI_PROJ_IO_FIELD_NAME) {
            ui_text_mouse_drag(&ui->text, lx, lo.name_x, lo.name_w);
        }
        return 1;
    }

    return 1;
}

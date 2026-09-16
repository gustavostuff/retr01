#include "ui/modals/entity_edit_internal.h"

void entity_modal_layout(const UiState *ui, EntityModalLayout *lo) {
    /* Single panel: 3 cols (left 80 | gap 8 | right 160). Row heights follow shared
     * vertical boundaries of the left tools column and the right canvas column. */
    enum {
        E_NAME = 1,
        E_STATE_NAME,
        E_FOOTER,
        E_PAL,
        E_STATE,
        E_FRAME,
        E_ADD,
        E_REM,
        E_HIGHLIGHT,
        E_BRUSH_LAB,
        E_BRUSH,
        E_FRAME_ID,
        E_CANVAS,
        E_GUIDES,
        E_SPACER
    };
    /* Body-local row map (after header rows 0..3):
     *  0:16  1:8  2:40  3:8  4:16  5:16  6:8  7:16  8:8  9:16 10:8
     * 11:16 12:8 13:8 14:8 15:8 16:8  => 216px
     * Global body starts at row 4. */
    static const UiPanelCell cells[] = {
        {E_NAME, 0, 0, 3, 1},
        {E_STATE_NAME, 0, 2, 3, 1},
        {E_PAL, 0, 4, 1, 3},       /* 16+8+40 = 64 */
        {E_STATE, 0, 8, 1, 1},     /* global 4+4 */
        {E_FRAME, 0, 9, 1, 1},
        {E_ADD, 0, 11, 1, 1},
        {E_REM, 0, 13, 1, 1},
        {E_HIGHLIGHT, 0, 15, 1, 1},
        {E_BRUSH_LAB, 0, 17, 1, 2}, /* 8+8 = 16 */
        {E_BRUSH, 0, 19, 1, 2},     /* 8+8 = 16 */
        {E_SPACER, 1, 4, 1, 17},    /* full body height */
        {E_FRAME_ID, 2, 4, 1, 1},
        {E_CANVAS, 2, 6, 1, 11},    /* from y24 through y184 */
        {E_GUIDES, 2, 18, 1, 2},    /* y192-208 */
        {E_FOOTER, 0, 22, 3, 1},
    };
    static const int row_hs[] = {
        /* 0-3 header */
        UI_BTN_H, UI_UNIT, UI_BTN_H, UI_UNIT,
        /* 4-20 body (17 rows) */
        16, 8, 40, 8, 16, 16, 8, 16, 8, 16, 8, 16, 8, 8, 8, 8, 8,
        /* 21 gap, 22 footer */
        UI_UNIT, UI_BTN_H,
    };
    UiPanel panel;
    int pad = UI_UNIT;
    int mx, my, mw, mh;
    int content_x, content_y;
    int cx, cy, cw, ch;
    int name_lab;
    int dots_w;
    int spr_btn_w;
    int left_right;
    int i;

    (void)ui;

    ui_panel_init(&panel, 3, 23, UI_PANEL_CELL_MIN, UI_PANEL_CELL_MIN);
    ui_panel_set_cells(&panel, cells, (int)(sizeof(cells) / sizeof(cells[0])));
    ui_panel_set_col_w(&panel, 0, UI_ENTITY_LEFT_W);
    ui_panel_set_col_w(&panel, 1, pad);
    ui_panel_set_col_w(&panel, 2, UI_ENTITY_COMPOSE);
    for (i = 0; i < (int)(sizeof(row_hs) / sizeof(row_hs[0])); i++) {
        ui_panel_set_row_h(&panel, i, row_hs[i]);
    }
    ui_panel_layout(&panel, 0, 0);

    mw = pad + panel.total_w + pad;
    mh = UI_BTN_H + pad + panel.total_h + pad;
    mx = (ui_logic_w(ui) - mw) / 2;
    my = (ui_logic_h(ui) - mh) / 2;
    content_x = mx + pad;
    content_y = my + UI_BTN_H + pad;
    ui_panel_layout(&panel, content_x, content_y);

    lo->mx = mx;
    lo->my = my;
    lo->mw = mw;
    lo->mh = mh;

    lo->left_x = content_x;
    lo->left_w = UI_ENTITY_LEFT_W;
    lo->right_x = content_x + UI_ENTITY_LEFT_W + pad;
    lo->right_w = UI_ENTITY_COMPOSE;
    left_right = lo->left_x + lo->left_w;

    name_lab = label_width("State name");
    if (name_lab < label_width("Name")) {
        name_lab = label_width("Name");
    }
    name_lab = ((name_lab + pad - 1) / pad) * pad + pad;

    ui_panel_cell(&panel, E_NAME, &cx, &cy, &cw, &ch);
    lo->name_y = cy;
    lo->name_x = content_x + name_lab + pad;
    lo->name_w = (content_x + panel.total_w) - lo->name_x;
    lo->name_w = (lo->name_w / pad) * pad;
    if (lo->name_w < UI_UNIT * 8) {
        lo->name_w = UI_UNIT * 8;
    }

    ui_panel_cell(&panel, E_STATE_NAME, &cx, &cy, &cw, &ch);
    lo->state_name_y = cy;
    lo->state_name_x = lo->name_x;
    lo->state_name_w = lo->name_w;

    ui_panel_cell(&panel, E_FRAME_ID, &cx, &cy, &cw, &ch);
    lo->frame_id_x = cx;
    lo->frame_id_y = cy;
    lo->frame_id_w = cw;

    ui_panel_cell(&panel, E_CANVAS, &cx, &cy, &cw, &ch);
    lo->right_grid_x = cx;
    lo->right_grid_y = cy;

    ui_panel_cell(&panel, E_PAL, &cx, &cy, &cw, &ch);
    lo->pal_y = cy;
    lo->pal_x = left_right - UI_PAL_GRID_SIZE;

    dots_w = UI_DOT_STRIP_N * UI_DOT_SIZE + (UI_DOT_STRIP_N > 0 ? (UI_DOT_STRIP_N - 1) * UI_DOT_GAP : 0);
    ui_panel_cell(&panel, E_STATE, &cx, &cy, &cw, &ch);
    lo->state_y = cy;
    lo->state_dots_x = left_right - dots_w;
    lo->state_dots_y = cy + (UI_BTN_H - UI_DOT_SIZE) / 2;

    ui_panel_cell(&panel, E_FRAME, &cx, &cy, &cw, &ch);
    lo->frame_y = cy;
    lo->frame_dots_x = left_right - dots_w;
    lo->frame_dots_y = cy + (UI_BTN_H - UI_DOT_SIZE) / 2;

    spr_btn_w = label_width("Remove");
    if (spr_btn_w < label_width("Add")) {
        spr_btn_w = label_width("Add");
    }
    spr_btn_w = ((spr_btn_w + pad - 1) / pad) * pad;
    if (spr_btn_w > lo->left_w) {
        spr_btn_w = lo->left_w;
    }

    ui_panel_cell(&panel, E_ADD, &cx, &cy, &cw, &ch);
    lo->add_spr_y = cy;
    lo->add_spr_w = spr_btn_w;
    lo->add_spr_x = left_right - spr_btn_w;

    ui_panel_cell(&panel, E_REM, &cx, &cy, &cw, &ch);
    lo->rem_spr_y = cy;
    lo->rem_spr_w = spr_btn_w;
    lo->rem_spr_x = left_right - spr_btn_w;

    ui_panel_cell(&panel, E_HIGHLIGHT, &cx, &cy, &cw, &ch);
    lo->highlight_y = cy;
    lo->highlight_w = UI_CHECKBOX + UI_MODE_GAP + label_width("Highlight");
    lo->highlight_w = ((lo->highlight_w + pad - 1) / pad) * pad;
    if (lo->highlight_w > lo->left_w) {
        lo->highlight_w = lo->left_w;
    }
    lo->highlight_x = left_right - lo->highlight_w;

    ui_panel_cell(&panel, E_BRUSH_LAB, &cx, &cy, &cw, &ch);
    lo->brush_lab_y = cy;
    lo->brush_lab_x = left_right - label_width("Brush");

    ui_panel_cell(&panel, E_BRUSH, &cx, &cy, &cw, &ch);
    lo->brush_y = cy;
    lo->brush_w = lo->left_w;
    lo->brush_x = left_right - lo->brush_w;

    ui_panel_cell(&panel, E_GUIDES, &cx, &cy, &cw, &ch);
    lo->guides_y = cy;
    lo->mode_y = cy;
    lo->guides_x = cx;
    {
        static const char *const mode_labels[] = {"Sprite select", "Sprite paint", "Origin/hitbox"};
        lo->mode_w = ui_multi_state_pref_width(mode_labels, 3);
        lo->mode_w = ((lo->mode_w + pad - 1) / pad) * pad;
        lo->mode_x = cx + cw - lo->mode_w;
    }

    ui_panel_cell(&panel, E_FOOTER, &cx, &cy, &cw, &ch);
    lo->btn_y = cy;
    lo->left_btn_x = content_x;
    lo->save_w = ((label_width("Save") + pad - 1) / pad) * pad;
    lo->cancel_w = ((label_width("Cancel") + pad - 1) / pad) * pad;

#if UI_PANEL_DEBUG_GRID
    lo->dbg_panel = panel;
#endif
}


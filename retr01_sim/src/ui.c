#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/board_layout.h"
#include "retr01_sim/bus.h"
#include "ui_assets.h"
#include "video_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int r01s_ui_init(R01sUi *ui) {
    if (!ui) {
        return -1;
    }
    memset(ui, 0, sizeof(*ui));
    ui->selected = -1;
    ui->drag_chip = -1;
    ui->drag_island = -1;
    ui->resize_island = -1;
    ui->drag_stick = -1;
    ui->drag_btn = -1;
    ui->ctx_chip = -1;
    ui->box_sel = 0;
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    ui->show_cart_flash = 1;
    ui->show_cart_eeprom = 1;
    (void)font_ensure();
    snprintf(ui->status, sizeof(ui->status),
             "SPACE pause. S save layout. R rotate. G scale. Sidebar: SCALE / CART / pads. COMPACT/ISLANDS");
    return 0;
}

void r01s_ui_shutdown(R01sUi *ui) {
    if (ui) {
        if (ui->lcd_tex) {
            SDL_DestroyTexture(ui->lcd_tex);
            ui->lcd_tex = NULL;
        }
        memset(ui, 0, sizeof(*ui));
    }
    font_shutdown();
}

int r01s_ui_rotate_selected(R01sUi *ui) {
    int i;
    int n = 0;
    const char *last_ref = NULL;
    R01sPkgOrient last_orient = R01S_ORIENT_H;

    if (!ui) {
        return 0;
    }

    /* Compact multi-select: rotate every selected IC. */
    if (ui_layout_flat(ui)) {
        for (i = 0; i < ui->chip_count; i++) {
            R01sEntity *te;
            if (!ui->chip_sel[i]) {
                continue;
            }
            te = ui->chips[i];
            if (!te || te->visual != R01S_ENTITY_VIS_IC) {
                continue;
            }
            r01s_entity_set_orient(te, te->orient == R01S_ORIENT_V ? R01S_ORIENT_H : R01S_ORIENT_V);
            clamp_chip(ui, te, ui->chip_island[i]);
            last_ref = te->refdes;
            last_orient = te->orient;
            n++;
        }
        if (n > 0) {
            ui->layout_dirty = 1;
            if (n == 1) {
                snprintf(ui->status, sizeof(ui->status), "%s -> %s", last_ref ? last_ref : "?",
                         last_orient == R01S_ORIENT_V ? "VERTICAL" : "HORIZONTAL");
            } else {
                snprintf(ui->status, sizeof(ui->status), "rotated %d chips", n);
            }
            return 1;
        }
    }

    {
        R01sEntity *te;
        int idx = ui->selected;
        if (idx < 0 || idx >= ui->chip_count) {
            idx = ui->ctx_chip;
        }
        if (idx < 0 || idx >= ui->chip_count) {
            return 0;
        }
        te = ui->chips[idx];
        if (!te || te->visual != R01S_ENTITY_VIS_IC) {
            return 0;
        }
        r01s_entity_set_orient(te, te->orient == R01S_ORIENT_V ? R01S_ORIENT_H : R01S_ORIENT_V);
        clamp_chip(ui, te, ui->chip_island[idx]);
        ui->layout_dirty = 1;
        snprintf(ui->status, sizeof(ui->status), "%s -> %s", te->refdes ? te->refdes : "?",
                 te->orient == R01S_ORIENT_V ? "VERTICAL" : "HORIZONTAL");
        return 1;
    }
}

void r01s_ui_bind_group(R01sUi *ui, R01sIslandGroup *group) {
    if (ui) {
        ui->group = group;
        r01s_ui_island_z_init(ui);
    }
}

void r01s_ui_island_z_init(R01sUi *ui) {
    int n;
    int i;
    if (!ui) {
        return;
    }
    n = ui->group ? r01s_island_group_count(ui->group) : 0;
    if (n > R01S_MAX_ISLANDS) {
        n = R01S_MAX_ISLANDS;
    }
    ui->island_z_count = n;
    for (i = 0; i < n; i++) {
        ui->island_z_order[i] = (uint8_t)i;
    }
}

void r01s_ui_island_z_apply(R01sUi *ui, const int *z_by_index, int n) {
    int rank;
    int i;
    int seen[R01S_MAX_ISLANDS];

    if (!ui || n <= 0 || n > R01S_MAX_ISLANDS) {
        r01s_ui_island_z_init(ui);
        return;
    }
    memset(seen, 0, sizeof(seen));
    ui->island_z_count = n;
    for (rank = 0; rank < n; rank++) {
        int found = -1;
        for (i = 0; i < n; i++) {
            if (!z_by_index || z_by_index[i] != rank) {
                continue;
            }
            if (seen[i]) {
                r01s_ui_island_z_init(ui);
                return;
            }
            seen[i] = 1;
            found = i;
            break;
        }
        if (found < 0) {
            r01s_ui_island_z_init(ui);
            return;
        }
        ui->island_z_order[rank] = (uint8_t)found;
    }
}

int r01s_ui_island_z_rank(const R01sUi *ui, int island_index) {
    int p;
    if (!ui || island_index < 0) {
        return 0;
    }
    for (p = 0; p < ui->island_z_count; p++) {
        if (ui->island_z_order[p] == (uint8_t)island_index) {
            return p;
        }
    }
    return island_index;
}

void r01s_ui_island_z_raise(R01sUi *ui, int island_index) {
    int n;
    int p;
    int i;

    if (!ui || island_index < 0) {
        return;
    }
    n = ui->island_z_count;
    if (n <= 1 || island_index >= n) {
        return;
    }
    for (p = 0; p < n; p++) {
        if (ui->island_z_order[p] == (uint8_t)island_index) {
            break;
        }
    }
    if (p < 0 || p >= n - 1) {
        return;
    }
    for (i = p; i < n - 1; i++) {
        ui->island_z_order[i] = ui->island_z_order[i + 1];
    }
    ui->island_z_order[n - 1] = (uint8_t)island_index;
}

static int chip_visual_draw_layer(const R01sEntity *e) {
    if (!e) {
        return 0;
    }
    switch (e->visual) {
    case R01S_ENTITY_VIS_IC:
        return 0;
    case R01S_ENTITY_VIS_PWR:
    case R01S_ENTITY_VIS_OSC:
        return 1;
    case R01S_ENTITY_VIS_DISPLAY:
        return 2;
    default:
        return 0;
    }
}

void r01s_ui_chip_z_init(R01sUi *ui) {
    int n;
    int i;
    int j;

    if (!ui) {
        return;
    }
    n = ui->chip_count;
    if (n > R01S_BOARD_MAX_CHIPS) {
        n = R01S_BOARD_MAX_CHIPS;
    }
    ui->chip_z_count = n;
    for (i = 0; i < n; i++) {
        ui->chip_z_order[i] = (uint8_t)i;
    }
    /* ICs back, display/LCD front (stable by chip index). */
    for (i = 1; i < n; i++) {
        uint8_t key = ui->chip_z_order[i];
        int key_layer = chip_visual_draw_layer(ui->chips[key]);
        j = i - 1;
        while (j >= 0 && chip_visual_draw_layer(ui->chips[ui->chip_z_order[j]]) > key_layer) {
            ui->chip_z_order[j + 1] = ui->chip_z_order[j];
            j--;
        }
        ui->chip_z_order[j + 1] = key;
    }
}

void r01s_ui_chip_z_apply(R01sUi *ui, const int *z_by_index, int n) {
    int rank;
    int i;
    int seen[R01S_BOARD_MAX_CHIPS];

    if (!ui || n <= 0 || n > R01S_BOARD_MAX_CHIPS) {
        r01s_ui_chip_z_init(ui);
        return;
    }
    memset(seen, 0, sizeof(seen));
    ui->chip_z_count = n;
    for (rank = 0; rank < n; rank++) {
        int found = -1;
        for (i = 0; i < n; i++) {
            if (!z_by_index || z_by_index[i] != rank) {
                continue;
            }
            if (seen[i]) {
                r01s_ui_chip_z_init(ui);
                return;
            }
            seen[i] = 1;
            found = i;
            break;
        }
        if (found < 0) {
            r01s_ui_chip_z_init(ui);
            return;
        }
        ui->chip_z_order[rank] = (uint8_t)found;
    }
}

int r01s_ui_chip_z_rank(const R01sUi *ui, int chip_index) {
    int p;
    if (!ui || chip_index < 0) {
        return 0;
    }
    for (p = 0; p < ui->chip_z_count; p++) {
        if (ui->chip_z_order[p] == (uint8_t)chip_index) {
            return p;
        }
    }
    return chip_index;
}

void r01s_ui_chip_z_raise(R01sUi *ui, int chip_index) {
    int n;
    int p;
    int i;

    if (!ui || chip_index < 0) {
        return;
    }
    n = ui->chip_z_count;
    if (n <= 1 || chip_index >= n) {
        return;
    }
    for (p = 0; p < n; p++) {
        if (ui->chip_z_order[p] == (uint8_t)chip_index) {
            break;
        }
    }
    if (p < 0 || p >= n - 1) {
        return;
    }
    for (i = p; i < n - 1; i++) {
        ui->chip_z_order[i] = ui->chip_z_order[i + 1];
    }
    ui->chip_z_order[n - 1] = (uint8_t)chip_index;
}

int r01s_ui_add_chip(R01sUi *ui, R01sEntity *chip, int island_index) {
    if (!ui || !chip || ui->chip_count >= R01S_BOARD_MAX_CHIPS) {
        return -1;
    }
    if (!ui->group || island_index < 0 || island_index >= r01s_island_group_count(ui->group)) {
        return -1;
    }
    ui->chips[ui->chip_count] = chip;
    ui->chip_island[ui->chip_count] = (uint8_t)island_index;
    clamp_chip(ui, chip, island_index);
    ui->chip_count++;
    return 0;
}

void r01s_ui_clamp_pan(R01sUi *ui) {
    int max_x = R01S_BOARD_W - R01S_UI_VIEW_W;
    int max_y = R01S_BOARD_H - R01S_UI_VIEW_H;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    if (ui->pan_x < 0) {
        ui->pan_x = 0;
    }
    if (ui->pan_y < 0) {
        ui->pan_y = 0;
    }
    if (ui->pan_x > max_x) {
        ui->pan_x = max_x;
    }
    if (ui->pan_y > max_y) {
        ui->pan_y = max_y;
    }
}

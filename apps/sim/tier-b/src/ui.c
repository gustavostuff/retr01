#include "ui.h"
#include "ui_font.h"

#include "r01a_board.h"
#include "r01a_layout.h"
#include "r01a_netlist.h"

#include "discrete_ic/board_layout.h"
#include "discrete_ic/breadboard.h"
#include "discrete_ic/entity.h"
#include "discrete_ic/island.h"
#include "discrete_ic/island_group.h"
#include "discrete_ic/outline.h"
#include "discrete_ic/passive.h"
#include "discrete_ic/types.h"
#include "discrete_ic/ui_assets.h"
#include "discrete_ic/video_sink.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef R01A_LAYOUT_FILE
#define R01A_LAYOUT_FILE "ui_layout.json"
#endif

#define R01A_UI_SCALE 2
#define R01A_SIM_BUDGET_MS 8
#define R01A_SIM_MAX_STEPS_PER_FRAME 24
#define R01A_DOTS_PER_STEP 32
#define R01A_BOARD_MAX_CHIPS 64
#define R01A_PAN_OVERSCROLL (NS_LOGIC_W / 2)
#define R01A_UI_TOOLTIP_DELAY_MS 400
#define R01A_SEL_YELLOW_R 255
#define R01A_SEL_YELLOW_G 220
#define R01A_SEL_YELLOW_B 80
#define R01A_PIN_HIT_PX 5
#define R01A_PIN_NET_SLOTS 512
#define R01A_HOVER_JOG 8
#define R01A_JUMPER_SEG_HALF_PX 1 /* 1px stroke + 1px each side => 3px hit */
#define R01A_JUMPER_HANDLE_PX 3
#define R01A_JUMPER_COLORS 10
#define R01A_PULSE_OK_MS 420
#define R01A_PULSE_SHORT_MS 140
#define R01A_AIR_PULSE_MS 600
#define R01A_AIR_ELBOWS 0 /* 1 = 2-elbow Manhattan, 0 = straight A-B */
#define R01A_ANTS_PERIOD 4
#define R01A_ANTS_MS 80
#define R01A_ZOOM_MAX 8

static const Uint8 k_jumper_rgb[R01A_JUMPER_COLORS][3] = {
    {220, 50, 50},   {230, 140, 40},  {230, 200, 50}, {50, 180, 70},  {50, 120, 220},
    {160, 70, 200},  {50, 50, 50},    {230, 230, 230}, {150, 95, 55}, {150, 150, 155},
};

#define R01A_JUMPER_PAL_PWR 0
#define R01A_JUMPER_PAL_DATA 3
#define R01A_JUMPER_PAL_CLK 4
#define R01A_JUMPER_PAL_GND 6
#define R01A_UNDO_MAX 32
#define R01A_UNDO_REF 16

enum { R01A_UNDO_MOVE = 0, R01A_UNDO_JMP_ADD, R01A_UNDO_JMP_DEL, R01A_UNDO_BB_DEL };

typedef struct R01aUndoPose {
    char ref[R01A_UNDO_REF];
    int x;
    int y;
    int px;
    int py;
    int e1;
    int e2;
} R01aUndoPose;

typedef struct R01aUndoCmd {
    int kind;
    int npose;
    R01aUndoPose before[R01A_BOARD_MAX_CHIPS];
    R01aUndoPose after[R01A_BOARD_MAX_CHIPS];
    int njmp;
    R01aJumper jmps[R01A_JUMPER_MAX];
    int nmid;
    int16_t mid_before[R01A_JUMPER_MAX];
    int16_t mid_after[R01A_JUMPER_MAX];
} R01aUndoCmd;

typedef struct R01aUi {
    SDL_Window *win;
    SDL_Renderer *rend;
    SDL_Texture *target;
    SDL_Texture *lcd_tex;
    int scale;
    NsEntity *chips[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_z[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_sel[R01A_BOARD_MAX_CHIPS];
    int sel_start_x[R01A_BOARD_MAX_CHIPS];
    int sel_start_y[R01A_BOARD_MAX_CHIPS];
    uint8_t chip_follow[R01A_BOARD_MAX_CHIPS];
    int chip_follow_from[R01A_BOARD_MAX_CHIPS];
    int chip_count;
    int selected;
    int pan_x;
    int pan_y;
    int zoom;
    int pan_grab_bx;
    int pan_grab_by;
    int drag_pan;
    int drag_chip;
    int drag_leg_chip;
    int drag_leg_pin;
    int drag_grab_bx;
    int drag_grab_by;
    int drag_last_x;
    int drag_last_y;
    int sel_drag_ox;
    int sel_drag_oy;
    int box_sel;
    int box_bx0, box_by0, box_bx1, box_by1;
    int mouse_lx;
    int mouse_ly;
    int jumper_arm;
    NsPbHole jumper_from;
    NsBreadboard *jumper_bb;
    int jumper_mode;
    int jumper_color_i;
    uint8_t jumper_sel[R01A_JUMPER_MAX];
    uint8_t jumper_z[R01A_JUMPER_MAX];
    int jumper_z_n;
    int hover_jumper;
    int drag_jumper;
    int drag_jumper_end;
    int drag_jumper_preview_ok;
    NsPbHole drag_jumper_preview;
    NsBreadboard *drag_jumper_preview_bb;
    int drag_jumper_elbow;
    int drag_elbow_hf;
    int drag_elbow_mid;
    int hover_chip;
    int hover_pin;
    int tip_stable_mx;
    int tip_stable_my;
    Uint32 tip_show_at;
    int right_armed;
    int right_pan;
    int right_lx;
    int right_ly;
    int ctx_open;
    int ctx_lx;
    int ctx_ly;
    int ctx_bx;
    int ctx_by;
    int fps;
    int fps_n;
    Uint32 fps_t0;
    int pin_gray;
    int air_always;
    R01aUndoCmd *undo;
    int undo_n;
    int undo_i;
    R01aUndoPose drag_pose[R01A_BOARD_MAX_CHIPS];
    int drag_pose_n;
    int jumper_mid_start[R01A_JUMPER_MAX];
    int jumper_mid_n;
} R01aUi;

static NsBreadboard *ui_bb_named(const R01aUi *ui, const char *ref);
static NsBreadboard *hit_breadboard(const R01aUi *ui, int mx, int my, NsPbHole *hole);
static NsBreadboard *ui_breadboard(const R01aUi *ui);
static int jumper_world_ends(const R01aUi *ui, const R01aJumper *j, int *x0, int *y0, int *x1, int *y1);
static int manhattan_h_first(int ax, int ay, int bx, int by);
static void mark_bb_followers(R01aUi *ui);
static void apply_bb_followers(R01aUi *ui, R01aBoard *board);
static void bind_chips(R01aUi *ui, R01aBoard *board);
static void pin_net_build(R01aUi *ui);
static void sel_clear(R01aUi *ui);

static void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

static void fill_rect_a(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_Rect rc = {x, y, w, h};
    if (A == 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

static int div_floor(int a, int b) {
    if (b <= 0) {
        return a;
    }
    if (a >= 0) {
        return a / b;
    }
    return -((-a + b - 1) / b);
}

static int canvas_zoom(const R01aUi *ui) {
    int z = (ui && ui->zoom > 0) ? ui->zoom : 1;
    if (z > R01A_ZOOM_MAX) {
        z = R01A_ZOOM_MAX;
    }
    return z;
}

static int view_mouse_x(const R01aUi *ui) {
    return div_floor(ui->mouse_lx, canvas_zoom(ui));
}

static int view_mouse_y(const R01aUi *ui) {
    return div_floor(ui->mouse_ly, canvas_zoom(ui));
}

static int board_sx(const R01aUi *ui, int bx) {
    return bx - ui->pan_x;
}

static int board_sy(const R01aUi *ui, int by) {
    return by - ui->pan_y;
}

static void logic_to_board(const R01aUi *ui, int lx, int ly, int *bx, int *by) {
    int z = canvas_zoom(ui);
    *bx = div_floor(lx, z) + ui->pan_x;
    *by = div_floor(ly, z) + ui->pan_y;
}

static void clamp_pan(R01aUi *ui) {
    int over = R01A_PAN_OVERSCROLL;
    int z = canvas_zoom(ui);
    int vw = NS_LOGIC_W / z;
    int vh = NS_LOGIC_H / z;
    int min_x = -over;
    int min_y = -over;
    int max_x;
    int max_y;
    if (vw < 1) {
        vw = 1;
    }
    if (vh < 1) {
        vh = 1;
    }
    max_x = NS_BOARD_W + over - vw;
    max_y = NS_BOARD_H + over - vh;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }
    if (ui->pan_x < min_x) {
        ui->pan_x = min_x;
    }
    if (ui->pan_y < min_y) {
        ui->pan_y = min_y;
    }
    if (ui->pan_x > max_x) {
        ui->pan_x = max_x;
    }
    if (ui->pan_y > max_y) {
        ui->pan_y = max_y;
    }
}

static void canvas_zoom_by(R01aUi *ui, int delta, int lx, int ly) {
    int z0 = canvas_zoom(ui);
    int z1 = z0 + delta;
    int bx;
    int by;
    if (z1 < 1) {
        z1 = 1;
    }
    if (z1 > R01A_ZOOM_MAX) {
        z1 = R01A_ZOOM_MAX;
    }
    if (z1 == z0) {
        return;
    }
    logic_to_board(ui, lx, ly, &bx, &by);
    ui->zoom = z1;
    ui->pan_x = bx - div_floor(lx, z1);
    ui->pan_y = by - div_floor(ly, z1);
    clamp_pan(ui);
}

static void pan_grab_begin(R01aUi *ui, int lx, int ly) {
    logic_to_board(ui, lx, ly, &ui->pan_grab_bx, &ui->pan_grab_by);
    ui->drag_last_x = lx;
    ui->drag_last_y = ly;
}

static void pan_grab_to(R01aUi *ui, int lx, int ly) {
    int z = canvas_zoom(ui);
    ui->pan_x = ui->pan_grab_bx - div_floor(lx, z);
    ui->pan_y = ui->pan_grab_by - div_floor(ly, z);
    ui->drag_last_x = lx;
    ui->drag_last_y = ly;
    clamp_pan(ui);
}

static void move_entity(NsEntity *e, int bx, int by) {
    if (!e) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        NsPassive *p = (NsPassive *)e;
        ns_passive_set_pivot(p, p->pivot_x + (bx - e->board_x), p->pivot_y + (by - e->board_y));
        return;
    }
    ns_entity_place(e, bx, by);
}

static void clamp_chip(NsEntity *e) {
    int over = NS_BOARD_W / 2;
    int min_x = -over;
    int min_y = -over;
    int max_x;
    int max_y;
    int bx;
    int by;

    if (!e) {
        return;
    }
    max_x = NS_BOARD_W + over - e->body_w;
    max_y = NS_BOARD_H + over - e->body_h;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }
    bx = e->board_x;
    by = e->board_y;
    if (bx < min_x) {
        bx = min_x;
    }
    if (by < min_y) {
        by = min_y;
    }
    if (bx > max_x) {
        bx = max_x;
    }
    if (by > max_y) {
        by = max_y;
    }
    move_entity(e, bx, by);
}

static int chip_hidden(const NsEntity *e) {
    (void)e;
    return 0;
}

static void bind_chips(R01aUi *ui, R01aBoard *board) {
    NsIsland *island = ns_island_group_at_mut(r01a_board_group(board), 0);
    int i;

    ui->chip_count = 0;
    if (!island) {
        return;
    }
    for (i = 0; i < island->entity_count && ui->chip_count < R01A_BOARD_MAX_CHIPS; i++) {
        NsEntity *e = island->entities[i];
        int zi;
        if (!e) {
            continue;
        }
        zi = ui->chip_count;
        ui->chips[zi] = e;
        ui->chip_z[zi] = (uint8_t)zi;
        ui->chip_count++;
    }
    {
        uint8_t ordered[R01A_BOARD_MAX_CHIPS];
        int n = 0;
        int k;
        for (k = 0; k < ui->chip_count; k++) {
            if (ui->chips[k] && ui->chips[k]->visual == NS_ENTITY_VIS_BREADBOARD) {
                ordered[n++] = (uint8_t)k;
            }
        }
        for (k = 0; k < ui->chip_count; k++) {
            if (!ui->chips[k] || ui->chips[k]->visual != NS_ENTITY_VIS_BREADBOARD) {
                ordered[n++] = (uint8_t)k;
            }
        }
        for (k = 0; k < n; k++) {
            ui->chip_z[k] = ordered[k];
        }
    }
}

static void pose_from_entity(R01aUndoPose *p, const NsEntity *e) {
    const char *ref;
    if (!p) {
        return;
    }
    memset(p, 0, sizeof(*p));
    if (!e) {
        return;
    }
    ref = e->refdes ? e->refdes : "";
    snprintf(p->ref, sizeof(p->ref), "%s", ref);
    p->x = e->board_x;
    p->y = e->board_y;
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        const NsPassive *pa = (const NsPassive *)e;
        p->px = pa->pivot_x;
        p->py = pa->pivot_y;
        p->e1 = pa->leg_ext[0];
        p->e2 = pa->leg_ext[1];
    } else {
        p->px = e->board_x;
        p->py = e->board_y;
    }
}

static int pose_eq(const R01aUndoPose *a, const R01aUndoPose *b) {
    return a && b && strcmp(a->ref, b->ref) == 0 && a->x == b->x && a->y == b->y && a->px == b->px &&
           a->py == b->py && a->e1 == b->e1 && a->e2 == b->e2;
}

static void pose_apply(R01aBoard *board, const R01aUndoPose *p) {
    NsEntity *e;
    if (!board || !p || !p->ref[0]) {
        return;
    }
    e = r01a_board_entity_by_refdes(board, p->ref);
    if (!e) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        ns_passive_set_pivot((NsPassive *)e, p->px, p->py);
        ns_passive_set_leg_ext((NsPassive *)e, 1, p->e1);
        ns_passive_set_leg_ext((NsPassive *)e, 2, p->e2);
    } else {
        ns_entity_place(e, p->x, p->y);
    }
    clamp_chip(e);
}

static void undo_capture_all(R01aUi *ui, R01aUndoPose *out, int *n_out) {
    int i;
    int n = 0;
    if (!ui || !out || !n_out) {
        return;
    }
    for (i = 0; i < ui->chip_count && n < R01A_BOARD_MAX_CHIPS; i++) {
        if (!ui->chips[i] || !ui->chips[i]->refdes) {
            continue;
        }
        pose_from_entity(&out[n], ui->chips[i]);
        n++;
    }
    *n_out = n;
}

static void undo_push(R01aUi *ui, const R01aUndoCmd *cmd) {
    if (!ui || !ui->undo || !cmd) {
        return;
    }
    if (ui->undo_i < ui->undo_n) {
        ui->undo_n = ui->undo_i;
    }
    if (ui->undo_n >= R01A_UNDO_MAX) {
        memmove(&ui->undo[0], &ui->undo[1], sizeof(ui->undo[0]) * (R01A_UNDO_MAX - 1));
        ui->undo_n = R01A_UNDO_MAX - 1;
        ui->undo_i = ui->undo_n;
    }
    ui->undo[ui->undo_n] = *cmd;
    ui->undo_n++;
    ui->undo_i = ui->undo_n;
}

static int jumper_index_of(const R01aBoard *board, const R01aJumper *j) {
    int i;
    if (!board || !j) {
        return -1;
    }
    for (i = 0; i < board->jumper_count; i++) {
        const R01aJumper *a = &board->jumpers[i];
        if (strcmp(r01a_jumper_a_ref(a), r01a_jumper_a_ref(j)) == 0 &&
            strcmp(r01a_jumper_b_ref(a), r01a_jumper_b_ref(j)) == 0 && a->a.col == j->a.col &&
            a->a.lane == j->a.lane && a->b.col == j->b.col && a->b.lane == j->b.lane) {
            return i;
        }
        if (strcmp(r01a_jumper_a_ref(a), r01a_jumper_b_ref(j)) == 0 &&
            strcmp(r01a_jumper_b_ref(a), r01a_jumper_a_ref(j)) == 0 && a->a.col == j->b.col &&
            a->a.lane == j->b.lane && a->b.col == j->a.col && a->b.lane == j->a.lane) {
            return i;
        }
    }
    return -1;
}

static int jumper_put(R01aBoard *board, const R01aJumper *j) {
    NsBreadboard *ba;
    NsBreadboard *bb;
    if (!board || !j) {
        return 0;
    }
    if (jumper_index_of(board, j) >= 0) {
        return 1;
    }
    ba = (NsBreadboard *)r01a_board_entity_by_refdes(board, r01a_jumper_a_ref(j));
    bb = (NsBreadboard *)r01a_board_entity_by_refdes(board, r01a_jumper_b_ref(j));
    if (!ba || !bb || ba->base.visual != NS_ENTITY_VIS_BREADBOARD ||
        bb->base.visual != NS_ENTITY_VIS_BREADBOARD) {
        return 0;
    }
    if (!r01a_board_jumper_add_across(board, ba, j->a, bb, j->b, j->r, j->g, j->bcol)) {
        return 0;
    }
    board->jumpers[board->jumper_count - 1] = *j;
    return 1;
}

static int bb_restore(R01aUi *ui, R01aBoard *board, const R01aUndoPose *p) {
    NsEntity *e;
    NsBreadboard *bb;
    if (!ui || !board || !p || !p->ref[0]) {
        return 0;
    }
    e = r01a_board_entity_by_refdes(board, p->ref);
    if (e && e->visual == NS_ENTITY_VIS_BREADBOARD) {
        ns_entity_place(e, p->x, p->y);
        return 1;
    }
    bb = r01a_board_add_breadboard(board, p->x, p->y);
    if (!bb) {
        return 0;
    }
    snprintf(bb->refdes_buf, sizeof(bb->refdes_buf), "%s", p->ref);
    bb->base.refdes = bb->refdes_buf;
    ns_entity_place(&bb->base, p->x, p->y);
    bind_chips(ui, board);
    return 1;
}

static void undo_push_move(R01aUi *ui, R01aBoard *board) {
    R01aUndoCmd cmd;
    R01aUndoPose now[R01A_BOARD_MAX_CHIPS];
    int nnow = 0;
    int i;
    int j;
    if (!ui || ui->drag_pose_n <= 0) {
        return;
    }
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = R01A_UNDO_MOVE;
    undo_capture_all(ui, now, &nnow);
    for (i = 0; i < ui->drag_pose_n && cmd.npose < R01A_BOARD_MAX_CHIPS; i++) {
        const R01aUndoPose *b = &ui->drag_pose[i];
        const R01aUndoPose *a = NULL;
        for (j = 0; j < nnow; j++) {
            if (strcmp(now[j].ref, b->ref) == 0) {
                a = &now[j];
                break;
            }
        }
        if (!a || pose_eq(b, a)) {
            continue;
        }
        cmd.before[cmd.npose] = *b;
        cmd.after[cmd.npose] = *a;
        cmd.npose++;
    }
    if (board) {
        int n = board->jumper_count;
        if (n > R01A_JUMPER_MAX) {
            n = R01A_JUMPER_MAX;
        }
        if (ui->jumper_mid_n < n) {
            n = ui->jumper_mid_n;
        }
        cmd.nmid = n;
        for (i = 0; i < n; i++) {
            cmd.mid_before[i] = (int16_t)ui->jumper_mid_start[i];
            cmd.mid_after[i] = board->jumpers[i].mid;
        }
    }
    {
        int changed = cmd.npose > 0;
        if (!changed) {
            for (i = 0; i < cmd.nmid; i++) {
                if (cmd.mid_before[i] != cmd.mid_after[i]) {
                    changed = 1;
                    break;
                }
            }
        }
        if (changed) {
            undo_push(ui, &cmd);
        }
    }
}

static void undo_push_jmp_add(R01aUi *ui, const R01aJumper *j) {
    R01aUndoCmd cmd;
    if (!ui || !j) {
        return;
    }
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = R01A_UNDO_JMP_ADD;
    cmd.njmp = 1;
    cmd.jmps[0] = *j;
    undo_push(ui, &cmd);
}

static void undo_push_jmp_del(R01aUi *ui, const R01aJumper *list, int n) {
    R01aUndoCmd cmd;
    int i;
    if (!ui || !list || n <= 0) {
        return;
    }
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = R01A_UNDO_JMP_DEL;
    if (n > R01A_JUMPER_MAX) {
        n = R01A_JUMPER_MAX;
    }
    cmd.njmp = n;
    for (i = 0; i < n; i++) {
        cmd.jmps[i] = list[i];
    }
    undo_push(ui, &cmd);
}

static int undo_apply_cmd(R01aUi *ui, R01aBoard *board, const R01aUndoCmd *cmd, int redo) {
    int i;
    if (!ui || !board || !cmd) {
        return 0;
    }
    sel_clear(ui);
    ui->drag_chip = -1;
    ui->drag_leg_chip = -1;
    ui->drag_jumper = -1;
    ui->drag_jumper_elbow = -1;
    ui->drag_jumper_preview_ok = 0;
    ui->jumper_arm = 0;
    ui->jumper_bb = NULL;
    ui->hover_jumper = -1;
    ui->drag_pose_n = 0;
    if (cmd->kind == R01A_UNDO_MOVE) {
        const R01aUndoPose *p = redo ? cmd->after : cmd->before;
        const int16_t *mids = redo ? cmd->mid_after : cmd->mid_before;
        for (i = 0; i < cmd->npose; i++) {
            pose_apply(board, &p[i]);
        }
        {
            int n = cmd->nmid;
            if (n > board->jumper_count) {
                n = board->jumper_count;
            }
            for (i = 0; i < n; i++) {
                if (board->jumpers[i].route) {
                    board->jumpers[i].mid = mids[i];
                }
            }
        }
        return 1;
    }
    if (cmd->kind == R01A_UNDO_JMP_ADD) {
        if (redo) {
            for (i = 0; i < cmd->njmp; i++) {
                jumper_put(board, &cmd->jmps[i]);
            }
        } else {
            for (i = 0; i < cmd->njmp; i++) {
                int ji = jumper_index_of(board, &cmd->jmps[i]);
                if (ji >= 0) {
                    r01a_board_jumper_remove(board, ji);
                }
            }
        }
        return 1;
    }
    if (cmd->kind == R01A_UNDO_JMP_DEL) {
        if (redo) {
            for (i = 0; i < cmd->njmp; i++) {
                int ji = jumper_index_of(board, &cmd->jmps[i]);
                if (ji >= 0) {
                    r01a_board_jumper_remove(board, ji);
                }
            }
        } else {
            for (i = 0; i < cmd->njmp; i++) {
                jumper_put(board, &cmd->jmps[i]);
            }
        }
        return 1;
    }
    if (cmd->kind == R01A_UNDO_BB_DEL) {
        if (redo) {
            for (i = cmd->npose - 1; i >= 0; i--) {
                NsEntity *e = r01a_board_entity_by_refdes(board, cmd->before[i].ref);
                if (e && e->visual == NS_ENTITY_VIS_BREADBOARD) {
                    r01a_board_remove_breadboard(board, (NsBreadboard *)e);
                }
            }
        } else {
            for (i = 0; i < cmd->npose; i++) {
                bb_restore(ui, board, &cmd->before[i]);
            }
            for (i = 0; i < cmd->njmp; i++) {
                jumper_put(board, &cmd->jmps[i]);
            }
        }
        bind_chips(ui, board);
        pin_net_build(ui);
        return 1;
    }
    return 0;
}

static int undo_do(R01aUi *ui, R01aBoard *board) {
    if (!ui || !board || !ui->undo || ui->undo_i <= 0) {
        return 0;
    }
    ui->undo_i--;
    return undo_apply_cmd(ui, board, &ui->undo[ui->undo_i], 0);
}

static int redo_do(R01aUi *ui, R01aBoard *board) {
    if (!ui || !board || !ui->undo || ui->undo_i >= ui->undo_n) {
        return 0;
    }
    if (!undo_apply_cmd(ui, board, &ui->undo[ui->undo_i], 1)) {
        return 0;
    }
    ui->undo_i++;
    return 1;
}

static void chip_z_raise(R01aUi *ui, int chip_i) {
    int r;
    int dst = 0;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    if (ui->chips[chip_i] && ui->chips[chip_i]->visual == NS_ENTITY_VIS_BREADBOARD) {
        return;
    }
    for (r = 0; r < ui->chip_count; r++) {
        if (ui->chip_z[r] != (uint8_t)chip_i) {
            ui->chip_z[dst++] = ui->chip_z[r];
        }
    }
    ui->chip_z[dst] = (uint8_t)chip_i;
}

static void jumper_z_sync(R01aUi *ui, const R01aBoard *board) {
    uint8_t seen[R01A_JUMPER_MAX];
    int n;
    int i;
    int dst;
    if (!ui || !board) {
        return;
    }
    n = board->jumper_count;
    if (n < 0) {
        n = 0;
    }
    if (n > R01A_JUMPER_MAX) {
        n = R01A_JUMPER_MAX;
    }
    memset(seen, 0, (size_t)n);
    dst = 0;
    for (i = 0; i < ui->jumper_z_n && dst < n; i++) {
        int ji = ui->jumper_z[i];
        if (ji >= 0 && ji < n && !seen[ji]) {
            seen[ji] = 1;
            ui->jumper_z[dst++] = (uint8_t)ji;
        }
    }
    for (i = 0; i < n; i++) {
        if (!seen[i]) {
            ui->jumper_z[dst++] = (uint8_t)i;
        }
    }
    ui->jumper_z_n = n;
}

static void jumper_z_raise(R01aUi *ui, const R01aBoard *board, int ji) {
    int r;
    int dst = 0;
    jumper_z_sync(ui, board);
    if (!ui || !board || ji < 0 || ji >= board->jumper_count) {
        return;
    }
    for (r = 0; r < ui->jumper_z_n; r++) {
        if (ui->jumper_z[r] != (uint8_t)ji) {
            ui->jumper_z[dst++] = ui->jumper_z[r];
        }
    }
    ui->jumper_z[dst] = (uint8_t)ji;
    ui->jumper_z_n = dst + 1;
}

static int sel_count(const R01aUi *ui) {
    int i;
    int n = 0;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chip_sel[i]) {
            n++;
        }
    }
    return n;
}

static void sel_clear(R01aUi *ui) {
    memset(ui->chip_sel, 0, sizeof(ui->chip_sel));
    memset(ui->jumper_sel, 0, sizeof(ui->jumper_sel));
    ui->selected = -1;
}

static void sel_set_one(R01aUi *ui, int chip_i) {
    sel_clear(ui);
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    ui->chip_sel[chip_i] = 1;
    ui->selected = chip_i;
}

static void sel_toggle(R01aUi *ui, int chip_i) {
    int i;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    ui->chip_sel[chip_i] = ui->chip_sel[chip_i] ? 0 : 1;
    if (ui->chip_sel[chip_i]) {
        ui->selected = chip_i;
        return;
    }
    if (ui->selected == chip_i) {
        ui->selected = -1;
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chip_sel[i]) {
                ui->selected = i;
                break;
            }
        }
    }
}

static int chip_in_box(const NsEntity *e, int x0, int y0, int x1, int y1) {
    int tmp;
    if (!e) {
        return 0;
    }
    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    return e->board_x < x1 && e->board_x + e->body_w > x0 && e->board_y < y1 && e->board_y + e->body_h > y0;
}

static void sel_from_box(R01aUi *ui, R01aBoard *board, int additive) {
    int i;
    int first = -1;
    int x0 = ui->box_bx0;
    int y0 = ui->box_by0;
    int x1 = ui->box_bx1;
    int y1 = ui->box_by1;
    int tmp;
    if (!additive) {
        sel_clear(ui);
    }
    if (x0 > x1) {
        tmp = x0;
        x0 = x1;
        x1 = tmp;
    }
    if (y0 > y1) {
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (chip_hidden(ui->chips[i]) || !chip_in_box(ui->chips[i], ui->box_bx0, ui->box_by0, ui->box_bx1, ui->box_by1)) {
            continue;
        }
        ui->chip_sel[i] = 1;
        if (first < 0) {
            first = i;
        }
    }
    if (board) {
        for (i = 0; i < board->jumper_count; i++) {
            int ax;
            int ay;
            int bx;
            int by;
            if (!jumper_world_ends(ui, &board->jumpers[i], &ax, &ay, &bx, &by)) {
                continue;
            }
            if ((ax >= x0 && ax <= x1 && ay >= y0 && ay <= y1) || (bx >= x0 && bx <= x1 && by >= y0 && by <= y1)) {
                ui->jumper_sel[i] = 1;
            }
        }
    }
    if (first >= 0) {
        ui->selected = first;
    }
}

static void begin_sel_drag(R01aUi *ui, R01aBoard *board, int board_mx, int board_my) {
    int i;
    ui->sel_drag_ox = board_mx;
    ui->sel_drag_oy = board_my;
    for (i = 0; i < ui->chip_count; i++) {
        ui->sel_start_x[i] = ui->chips[i] ? ui->chips[i]->board_x : 0;
        ui->sel_start_y[i] = ui->chips[i] ? ui->chips[i]->board_y : 0;
        ui->chip_follow[i] = 0;
        ui->chip_follow_from[i] = -1;
    }
    mark_bb_followers(ui);
    undo_capture_all(ui, ui->drag_pose, &ui->drag_pose_n);
    ui->jumper_mid_n = 0;
    if (board) {
        int n = board->jumper_count;
        if (n > R01A_JUMPER_MAX) {
            n = R01A_JUMPER_MAX;
        }
        for (i = 0; i < n; i++) {
            ui->jumper_mid_start[i] = board->jumpers[i].mid;
        }
        ui->jumper_mid_n = n;
    }
}

static void move_chip_drag(R01aUi *ui, R01aBoard *board, int chip_i, int board_mx, int board_my) {
    NsEntity *e;
    if (chip_i < 0 || chip_i >= ui->chip_count) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    move_entity(e, board_mx - ui->drag_grab_bx, board_my - ui->drag_grab_by);
    clamp_chip(e);
    apply_bb_followers(ui, board);
}

static void move_selection_drag(R01aUi *ui, R01aBoard *board, int board_mx, int board_my) {
    int i;
    int dx = board_mx - ui->sel_drag_ox;
    int dy = board_my - ui->sel_drag_oy;
    for (i = 0; i < ui->chip_count; i++) {
        if (!ui->chip_sel[i] || !ui->chips[i]) {
            continue;
        }
        move_entity(ui->chips[i], ui->sel_start_x[i] + dx, ui->sel_start_y[i] + dy);
        clamp_chip(ui->chips[i]);
    }
    apply_bb_followers(ui, board);
}

static int entity_tip_board(const NsEntity *e, int pin_num, int *tx, int *ty) {
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return ns_passive_tip_board((const NsPassive *)e, pin_num, tx, ty);
    }
    return ns_entity_pin_tip_board(e, pin_num, tx, ty);
}

static int entity_pin_hi(const NsEntity *e) {
    int i;
    int hi = 0;
    if (!e) {
        return 0;
    }
    if (e->visual == NS_ENTITY_VIS_IC) {
        return e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return e->pin_count;
    }
    if (e->visual != NS_ENTITY_VIS_OSC) {
        return 0;
    }
    for (i = 0; i < e->pin_count; i++) {
        if (e->pins[i].number > hi) {
            hi = e->pins[i].number;
        }
    }
    return hi;
}

static int entity_has_pin_on_bb(const NsEntity *e, const NsBreadboard *bb) {
    int n;
    int pin_hi;

    if (!e || !bb || e->visual == NS_ENTITY_VIS_BREADBOARD) {
        return 0;
    }
    pin_hi = entity_pin_hi(e);
    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        NsPbHole h;
        int hx;
        int hy;
        if (!entity_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        if (!ns_breadboard_hit_hole(bb, tx, ty, &h)) {
            continue;
        }
        ns_breadboard_hole_world(bb, h, &hx, &hy);
        if (hx == tx && hy == ty) {
            return 1;
        }
    }
    return 0;
}

static int chip_is_dragged_bb(const R01aUi *ui, int i) {
    const NsEntity *e;
    if (i < 0 || i >= ui->chip_count) {
        return 0;
    }
    e = ui->chips[i];
    if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
        return 0;
    }
    return ui->chip_sel[i] || i == ui->drag_chip;
}

static void mark_bb_followers(R01aUi *ui) {
    int i;
    int j;
    if (!ui) {
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (!chip_is_dragged_bb(ui, i)) {
            continue;
        }
        for (j = 0; j < ui->chip_count; j++) {
            if (j == i || ui->chip_follow[j] || !ui->chips[j]) {
                continue;
            }
            if (entity_has_pin_on_bb(ui->chips[j], (const NsBreadboard *)ui->chips[i])) {
                ui->chip_follow[j] = 1;
                ui->chip_follow_from[j] = i;
            }
        }
    }
}

static int bb_drag_delta(const R01aUi *ui, const char *ref, int *dx, int *dy) {
    int i;
    if (dx) {
        *dx = 0;
    }
    if (dy) {
        *dy = 0;
    }
    if (!ui || !ref || !ref[0]) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        const NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD || !e->refdes) {
            continue;
        }
        if (strcmp(e->refdes, ref) != 0) {
            continue;
        }
        if (dx) {
            *dx = e->board_x - ui->sel_start_x[i];
        }
        if (dy) {
            *dy = e->board_y - ui->sel_start_y[i];
        }
        return 1;
    }
    return 0;
}

static void jumper_follow_boards(R01aUi *ui, R01aBoard *board) {
    int i;
    int n;
    if (!ui || !board) {
        return;
    }
    n = board->jumper_count;
    if (n > ui->jumper_mid_n) {
        n = ui->jumper_mid_n;
    }
    for (i = 0; i < n; i++) {
        R01aJumper *j = &board->jumpers[i];
        int dax = 0;
        int day = 0;
        int dbx = 0;
        int dby = 0;
        int dx;
        int dy;
        if (!j->route) {
            continue;
        }
        bb_drag_delta(ui, r01a_jumper_a_ref(j), &dax, &day);
        bb_drag_delta(ui, r01a_jumper_b_ref(j), &dbx, &dby);
        if (dax == dbx && day == dby) {
            dx = dax;
            dy = day;
        } else if (dax != 0 || day != 0) {
            if (dbx != 0 || dby != 0) {
                dx = (dax + dbx) / 2;
                dy = (day + dby) / 2;
            } else {
                dx = dax;
                dy = day;
            }
        } else {
            dx = dbx;
            dy = dby;
        }
        if (dx == 0 && dy == 0) {
            j->mid = (int16_t)ui->jumper_mid_start[i];
            continue;
        }
        if (j->h_first) {
            j->mid = (int16_t)(ui->jumper_mid_start[i] + dx);
        } else {
            j->mid = (int16_t)(ui->jumper_mid_start[i] + dy);
        }
    }
}

static void apply_bb_followers(R01aUi *ui, R01aBoard *board) {
    int j;
    if (!ui) {
        return;
    }
    for (j = 0; j < ui->chip_count; j++) {
        int bi;
        int dx;
        int dy;
        if (!ui->chip_follow[j] || ui->chip_sel[j] || !ui->chips[j]) {
            continue;
        }
        bi = ui->chip_follow_from[j];
        if (bi < 0 || bi >= ui->chip_count || !ui->chips[bi]) {
            continue;
        }
        dx = ui->chips[bi]->board_x - ui->sel_start_x[bi];
        dy = ui->chips[bi]->board_y - ui->sel_start_y[bi];
        move_entity(ui->chips[j], ui->sel_start_x[j] + dx, ui->sel_start_y[j] + dy);
        clamp_chip(ui->chips[j]);
    }
    jumper_follow_boards(ui, board);
}

static NsBreadboard *ui_breadboard(const R01aUi *ui) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->visual == NS_ENTITY_VIS_BREADBOARD) {
            return (NsBreadboard *)ui->chips[i];
        }
    }
    return NULL;
}

static int count_pins_on_holes(const R01aUi *ui, const NsEntity *e, int dx, int dy) {
    int n;
    int count = 0;
    int pin_hi = entity_pin_hi(e);
    int bi;

    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        if (!entity_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        tx += dx;
        ty += dy;
        for (bi = 0; bi < ui->chip_count; bi++) {
            const NsEntity *be = ui->chips[bi];
            NsPbHole h;
            int hx;
            int hy;
            if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD || be == e) {
                continue;
            }
            if (!ns_breadboard_hit_hole((const NsBreadboard *)(const void *)be, tx, ty, &h)) {
                continue;
            }
            ns_breadboard_hole_world((const NsBreadboard *)(const void *)be, h, &hx, &hy);
            if (hx == tx && hy == ty) {
                count++;
                break;
            }
        }
    }
    return count;
}

static int snap_chip_to_breadboard(R01aUi *ui, int chip_i) {
    NsEntity *e;
    int n;
    int bi;
    int best_count = 0;
    int best_dx = 0;
    int best_dy = 0;
    int pin_hi;

    if (!ui || chip_i < 0 || chip_i >= ui->chip_count) {
        return 0;
    }
    e = ui->chips[chip_i];
    pin_hi = entity_pin_hi(e);
    if (!e || pin_hi < 1) {
        return 0;
    }
    for (n = 1; n <= pin_hi; n++) {
        int tx;
        int ty;
        if (!entity_tip_board(e, n, &tx, &ty)) {
            continue;
        }
        for (bi = 0; bi < ui->chip_count; bi++) {
            NsEntity *be = ui->chips[bi];
            NsPbHole h;
            int hx;
            int hy;
            int dx;
            int dy;
            int count;
            if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD) {
                continue;
            }
            if (!ns_breadboard_hit_hole((const NsBreadboard *)(const void *)be, tx, ty, &h)) {
                continue;
            }
            ns_breadboard_hole_world((const NsBreadboard *)(const void *)be, h, &hx, &hy);
            dx = hx - tx;
            dy = hy - ty;
            count = count_pins_on_holes(ui, e, dx, dy);
            if (count > best_count) {
                best_count = count;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }
    if (best_count < 1) {
        return 0;
    }
    move_entity(e, e->board_x + best_dx, e->board_y + best_dy);
    clamp_chip(e);
    return best_count;
}

static void snap_selection_to_breadboard(R01aUi *ui) {
    int i;
    if (sel_count(ui) > 0) {
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chip_sel[i]) {
                snap_chip_to_breadboard(ui, i);
            }
        }
        return;
    }
    if (ui->drag_chip >= 0) {
        snap_chip_to_breadboard(ui, ui->drag_chip);
    }
}

static int hit_ic_body(const NsEntity *e, int bx, int by) {
    int x0 = e->board_x;
    int y0 = e->board_y;
    int x1 = x0 + e->body_w;
    int y1 = y0 + e->body_h;
    int pin = NS_UI_PIN_H;
    if (ns_orient_is_horiz(e->orient)) {
        return bx >= x0 && bx < x1 && by >= y0 - pin && by < y1 + pin;
    }
    return by >= y0 && by < y1 && bx >= x0 - pin && bx < x1 + pin;
}

static int hit_chip_body(const R01aUi *ui, const NsEntity *e, int lx, int ly) {
    int bx;
    int by;
    if (!e) {
        return 0;
    }
    logic_to_board(ui, lx, ly, &bx, &by);
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        return ns_passive_hit((const NsPassive *)e, bx, by);
    }
    if (e->visual == NS_ENTITY_VIS_OSC) {
        return ns_osc4legs_hit(e, bx, by);
    }
    if (e->visual == NS_ENTITY_VIS_IC) {
        return hit_ic_body(e, bx, by);
    }
    return bx >= e->board_x && bx < e->board_x + e->body_w && by >= e->board_y && by < e->board_y + e->body_h;
}

static int hit_top_chip(const R01aUi *ui, int lx, int ly) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        if (ci < 0 || ci >= ui->chip_count || !ui->chips[ci] || chip_hidden(ui->chips[ci])) {
            continue;
        }
        if (hit_chip_body(ui, ui->chips[ci], lx, ly)) {
            return ci;
        }
    }
    return -1;
}

static int rotate_selected(R01aUi *ui) {
    int i;
    int n = 0;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e;
        if (!ui->chip_sel[i] && i != ui->selected) {
            continue;
        }
        e = ui->chips[i];
        if (!e) {
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_IC) {
            ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_BREADBOARD) {
            ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
            ns_breadboard_sync_body((NsBreadboard *)e);
            clamp_chip(e);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_OSC) {
            ns_osc4legs_set_orient(e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_PASSIVE) {
            ns_passive_set_orient((NsPassive *)e, ns_orient_next_cw(e->orient));
            clamp_chip(e);
            snap_chip_to_breadboard(ui, i);
            n++;
        }
    }
    return n;
}

static void pin_level_rgb(NsLevel lvl, NsPinDir dir, Uint8 *pr, Uint8 *pg, Uint8 *pb) {
    if (dir == NS_PIN_PWR) {
        *pr = 220;
        *pg = 70;
        *pb = 70;
        return;
    }
    if (dir == NS_PIN_NC) {
        *pr = 70;
        *pg = 70;
        *pb = 70;
        return;
    }
    switch (lvl) {
    case NS_LVL_H:
        *pr = 70;
        *pg = 210;
        *pb = 90;
        break;
    case NS_LVL_L:
        *pr = 28;
        *pg = 32;
        *pb = 30;
        break;
    case NS_LVL_X:
        *pr = 220;
        *pg = 80;
        *pb = 200;
        break;
    default:
        *pr = 120;
        *pg = 125;
        *pb = 110;
        break;
    }
}

static void dip_pin_pos(const NsEntity *e, int pin_num, int *along, int *side_pin1) {
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int half = dip / 2;
    int idx;
    int span;
    int pitch = NS_DIP_PIN_PITCH_PX;
    int row_span;
    int margin;
    int reverse;

    if (dip <= 0 || pin_num <= 0 || pin_num > dip) {
        *side_pin1 = 1;
        *along = ns_orient_is_horiz(e->orient) ? (e->body_w / 2) : (e->body_h / 2);
        return;
    }
    *side_pin1 = pin_num <= half;
    idx = *side_pin1 ? (pin_num - 1) : (dip - pin_num);
    span = ns_orient_is_horiz(e->orient) ? e->body_w : e->body_h;
    row_span = (half > 1) ? (half - 1) * pitch : 0;
    margin = (span - row_span) / 2;
    if (margin < 1) {
        margin = 1;
    }
    reverse = (e->orient == NS_ORIENT_180 || e->orient == NS_ORIENT_270);
    if (reverse) {
        *along = margin + (half > 0 ? (half - 1 - idx) : 0) * pitch;
    } else {
        *along = margin + idx * pitch;
    }
}

static void blit_pin_rot(SDL_Renderer *r, int dx, int dy, int rot, const Uint8 *tint_rgb) {
    int w = NS_UI_PIN_W;
    int h = NS_UI_PIN_H;
    int ox0 = NS_UI_PIN_OX;
    int oy0 = NS_UI_PIN_OY;
    int sx, sy, ox, oy, ow, oh;
    int tl_x, tl_y;
    int org_x, org_y;

    if (!r) {
        return;
    }
    if (rot == 1 || rot == 3) {
        ow = h;
        oh = w;
    } else {
        ow = w;
        oh = h;
    }
    if (rot == 0) {
        org_x = ox0;
        org_y = oy0;
    } else if (rot == 1) {
        org_x = h - 1 - oy0;
        org_y = ox0;
    } else if (rot == 2) {
        org_x = w - 1 - ox0;
        org_y = h - 1 - oy0;
    } else {
        org_x = oy0;
        org_y = w - 1 - ox0;
    }
    tl_x = dx - org_x;
    tl_y = dy - org_y;
    for (oy = 0; oy < oh; oy++) {
        for (ox = 0; ox < ow; ox++) {
            const uint8_t *p;
            if (rot == 0) {
                sx = ox;
                sy = oy;
            } else if (rot == 1) {
                sx = oy;
                sy = h - 1 - ox;
            } else if (rot == 2) {
                sx = w - 1 - ox;
                sy = h - 1 - oy;
            } else {
                sx = w - 1 - oy;
                sy = ox;
            }
            p = NS_UI_PIN_RGBA + ((size_t)sy * (size_t)w + (size_t)sx) * 4u;
            if (p[3] == 0) {
                continue;
            }
            if (tint_rgb) {
                SDL_SetRenderDrawColor(r, tint_rgb[0], tint_rgb[1], tint_rgb[2], 255);
            } else {
                SDL_SetRenderDrawColor(r, p[0], p[1], p[2], 255);
            }
            SDL_RenderDrawPoint(r, tl_x + ox, tl_y + oy);
        }
    }
}

static int pin_tip_reach(void) {
    return NS_UI_PIN_H - 1 - NS_UI_PIN_OY;
}

static void draw_dip_pad_h(SDL_Renderer *r, int px, int body_edge_y, int outward_down, const Uint8 *tint_rgb) {
    int reach = pin_tip_reach();
    if (outward_down) {
        blit_pin_rot(r, px, body_edge_y + reach, 2, tint_rgb);
    } else {
        blit_pin_rot(r, px, body_edge_y - 1 - reach, 0, tint_rgb);
    }
}

static void draw_dip_pad_v(SDL_Renderer *r, int py, int body_edge_x, int outward_left, const Uint8 *tint_rgb) {
    int reach = pin_tip_reach();
    if (outward_left) {
        blit_pin_rot(r, body_edge_x - 1 - reach, py, 3, tint_rgb);
    } else {
        blit_pin_rot(r, body_edge_x + reach, py, 1, tint_rgb);
    }
}

static unsigned label_seed(const NsEntity *e) {
    unsigned seed = 0;
    const char *s = e->refdes ? e->refdes : (e->part ? e->part : "");
    for (; *s; s++) {
        seed = seed * 131u + (unsigned char)*s;
    }
    return seed;
}

static void draw_selection(SDL_Renderer *r, int x, int y, int w, int h, int selected) {
    if (selected) {
        draw_rect(r, x, y, w, h, R01A_SEL_YELLOW_R, R01A_SEL_YELLOW_G, R01A_SEL_YELLOW_B);
    }
}

static void draw_ic(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int x = board_sx(ui, e->board_x);
    int y = board_sy(ui, e->board_y);
    int i;
    int dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    int horiz = ns_orient_is_horiz(e->orient);
    Uint8 br = selected ? 40 : 28;
    Uint8 bg = selected ? 48 : 32;
    Uint8 bb = selected ? 36 : 28;
    NsOutlineRgb oc = ns_outline_rgb(e->health);

    for (i = 0; i < e->pin_count; i++) {
        int num = e->pins[i].number;
        int along;
        int side_pin1;
        Uint8 tint[3];
        if (num < 1 || num > dip) {
            continue;
        }
        dip_pin_pos(e, num, &along, &side_pin1);
        if (ui->pin_gray) {
            tint[0] = 120;
            tint[1] = 125;
            tint[2] = 110;
        } else {
            pin_level_rgb(e->pins[i].level, e->pins[i].dir, &tint[0], &tint[1], &tint[2]);
        }
        switch (e->orient) {
        case NS_ORIENT_90:
            draw_dip_pad_v(r, y + along, side_pin1 ? x : (x + e->body_w), side_pin1, tint);
            break;
        case NS_ORIENT_180:
            draw_dip_pad_h(r, x + along, side_pin1 ? y : (y + e->body_h), !side_pin1, tint);
            break;
        case NS_ORIENT_270:
            draw_dip_pad_v(r, y + along, side_pin1 ? (x + e->body_w) : x, !side_pin1, tint);
            break;
        case NS_ORIENT_0:
        default:
            draw_dip_pad_h(r, x + along, side_pin1 ? (y + e->body_h) : y, side_pin1, tint);
            break;
        }
    }
    fill_rect(r, x, y, e->body_w, e->body_h, br, bg, bb);
    if (selected) {
        draw_rect(r, x, y, e->body_w, e->body_h, R01A_SEL_YELLOW_R, R01A_SEL_YELLOW_G, R01A_SEL_YELLOW_B);
    } else {
        draw_rect(r, x, y, e->body_w, e->body_h, oc.r, oc.g, oc.b);
    }
    switch (e->orient) {
    case NS_ORIENT_90:
        fill_rect(r, x + e->body_w / 2 - 2, y - 1, 4, 2, 20, 22, 20);
        break;
    case NS_ORIENT_180:
        fill_rect(r, x + e->body_w - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    case NS_ORIENT_270:
        fill_rect(r, x + e->body_w / 2 - 2, y + e->body_h - 1, 4, 2, 20, 22, 20);
        break;
    case NS_ORIENT_0:
    default:
        fill_rect(r, x - 1, y + e->body_h / 2 - 2, 2, 4, 20, 22, 20);
        break;
    }
    {
        const char *label = e->part ? e->part : e->refdes;
        if (label && label[0]) {
            int pad = 2;
            int view_w = e->body_w - pad * 2;
            int view_h = e->body_h - pad * 2;
            if (view_w < 1) {
                view_w = e->body_w;
                pad = 0;
            }
            if (view_h < 1) {
                view_h = e->body_h;
                pad = 0;
            }
            if (horiz) {
                r01a_draw_label_bounce(r, x + pad, y + pad, view_w, view_h, label, label_seed(e), 255, 255, 255, 128);
            } else {
                r01a_draw_label_bounce_rot90ccw(r, x + pad, y + pad, view_w, view_h, label, label_seed(e), 255, 255,
                                                255, 128);
            }
        }
    }
}

static void draw_osc(SDL_Renderer *r, const R01aUi *ui, const NsEntity *e, int selected) {
    int p1x;
    int p1y;
    if (!ns_osc4legs_chip_tip(e, 14, &p1x, &p1y)) {
        return;
    }
    ns_passive_draw_kind(r, NS_PASSIVE_OSC4LEGS, e->orient, board_sx(ui, p1x), board_sy(ui, p1y),
                        selected);
}

static void draw_lcd(SDL_Renderer *r, R01aUi *ui, NsVideoSink *sink, int selected) {
    const uint8_t *rgb;
    SDL_Rect dst;
    int x = board_sx(ui, sink->base.board_x);
    int y = board_sy(ui, sink->base.board_y);
    int lcd_w;
    int lcd_h;

    ns_video_sink_lcd_size(sink, &lcd_w, &lcd_h);
    rgb = ns_video_sink_rgb(sink);
    if (!rgb) {
        return;
    }
    if (!ui->lcd_tex) {
        ui->lcd_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, NS_VIDEO_W,
                                        NS_VIDEO_H);
        if (ui->lcd_tex) {
            SDL_SetTextureScaleMode(ui->lcd_tex, SDL_ScaleModeNearest);
        }
    }
    if (!ui->lcd_tex) {
        return;
    }
    SDL_UpdateTexture(ui->lcd_tex, NULL, rgb, NS_VIDEO_W * 3);
    dst.x = x;
    dst.y = y;
    dst.w = lcd_w;
    dst.h = lcd_h;
    SDL_RenderCopy(r, ui->lcd_tex, NULL, &dst);
    draw_selection(r, x, y, sink->base.body_w, sink->base.body_h, selected);
}

typedef struct R01aPinNetSlot {
    NsEntity *entity;
    int pin_index;
} R01aPinNetSlot;

static R01aPinNetSlot g_pin_slots[R01A_PIN_NET_SLOTS];
static int g_pin_parent[R01A_PIN_NET_SLOTS];
static int g_pin_slot_count;

static int pin_net_find(NsEntity *e, int pin_index) {
    int i;
    for (i = 0; i < g_pin_slot_count; i++) {
        if (g_pin_slots[i].entity == e && g_pin_slots[i].pin_index == pin_index) {
            return i;
        }
    }
    return -1;
}

static int pin_net_add(NsEntity *e, int pin_index) {
    int i = pin_net_find(e, pin_index);
    if (i >= 0) {
        return i;
    }
    if (!e || pin_index < 0 || g_pin_slot_count >= R01A_PIN_NET_SLOTS) {
        return -1;
    }
    i = g_pin_slot_count++;
    g_pin_slots[i].entity = e;
    g_pin_slots[i].pin_index = pin_index;
    g_pin_parent[i] = i;
    return i;
}

static int pin_net_add_name(NsEntity *e, const char *name) {
    const NsPin *pin;
    int idx;
    if (!e || !name) {
        return -1;
    }
    pin = ns_entity_pin_named_const(e, name);
    if (!pin) {
        return -1;
    }
    idx = (int)(pin - e->pins);
    if (idx < 0 || idx >= e->pin_count) {
        return -1;
    }
    return pin_net_add(e, idx);
}

static int pin_net_root(int s) {
    while (s >= 0 && s < g_pin_slot_count && g_pin_parent[s] != s) {
        g_pin_parent[s] = g_pin_parent[g_pin_parent[s]];
        s = g_pin_parent[s];
    }
    return s;
}

static void pin_net_union(int a, int b) {
    int ra = pin_net_root(a);
    int rb = pin_net_root(b);
    if (ra >= 0 && rb >= 0 && ra != rb) {
        g_pin_parent[rb] = ra;
    }
}

static void pin_net_link(NsEntity *a, const char *an, NsEntity *b, const char *bn) {
    int sa = pin_net_add_name(a, an);
    int sb = pin_net_add_name(b, bn);
    if (sa >= 0 && sb >= 0) {
        pin_net_union(sa, sb);
    }
}

static NsEntity *ui_ent(const R01aUi *ui, const char *ref) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] && ui->chips[i]->refdes && strcmp(ui->chips[i]->refdes, ref) == 0) {
            return ui->chips[i];
        }
    }
    return NULL;
}

static void pin_net_build(R01aUi *ui) {
    NsEntity *y2 = ui_ent(ui, "Y2");
    NsEntity *y3 = ui_ent(ui, "Y3");
    NsEntity *bx = ui_ent(ui, "UPLDX");
    NsEntity *by = ui_ent(ui, "UPLDY");
    NsEntity *comp = ui_ent(ui, "UPLDC");
    NsEntity *u24 = ui_ent(ui, "U24");
    NsEntity *enc = ui_ent(ui, "UENC");
    int i;
    char iname[8];
    char aname[4];

    g_pin_slot_count = 0;
    pin_net_link(y2, "VDD", y2, "OE#");
    pin_net_link(y2, "VDD", y3, "VDD");
    pin_net_link(y2, "VDD", y3, "OE#");
    pin_net_link(y2, "VDD", bx, "VCC");
    pin_net_link(y2, "VDD", by, "VCC");
    pin_net_link(y2, "VDD", comp, "VCC");
    pin_net_link(y2, "VDD", u24, "VCC");
    pin_net_link(y2, "VDD", u24, "VPP");
    pin_net_link(y2, "VDD", enc, "APOS");
    pin_net_link(y2, "VDD", enc, "DPOS");
    pin_net_link(y2, "VDD", bx, "RES#");
    pin_net_link(y2, "VDD", by, "RES#");
    pin_net_link(y2, "VDD", comp, "RES#");
    pin_net_link(y2, "VDD", comp, "RWB");
    pin_net_link(y2, "VDD", u24, "PGM#");
    pin_net_link(y2, "VDD", enc, "ENCD");
    pin_net_link(y2, "VDD", enc, "STND");
    pin_net_link(y2, "VDD", enc, "VSYNC");
    pin_net_link(y2, "GND", y3, "GND");
    pin_net_link(y2, "GND", bx, "GND");
    pin_net_link(y2, "GND", by, "GND");
    pin_net_link(y2, "GND", comp, "GND");
    pin_net_link(y2, "GND", comp, "PHI2");
    pin_net_link(y2, "GND", u24, "GND");
    pin_net_link(y2, "GND", enc, "AGND");
    pin_net_link(y2, "GND", enc, "DGND");
    pin_net_link(y2, "GND", enc, "SELECT");
    pin_net_link(y2, "GND", u24, "CE#");
    pin_net_link(y2, "GND", u24, "OE#");
    pin_net_link(y2, "DOT", ui_ent(ui, "R12"), "1");
    pin_net_link(ui_ent(ui, "R12"), "2", bx, "CLK");
    pin_net_link(ui_ent(ui, "R12"), "2", comp, "CLK");
    pin_net_link(y3, "FSC", ui_ent(ui, "R13"), "1");
    pin_net_link(ui_ent(ui, "R13"), "2", enc, "FIN");
    pin_net_link(bx, "HWRAP", by, "CLK");
    pin_net_link(bx, "CSYNC", enc, "HSYNC");
    pin_net_link(bx, "HBLANK", comp, "HBLANK");
    pin_net_link(bx, "X5", comp, "X5");
    pin_net_link(bx, "X6", comp, "X6");
    pin_net_link(bx, "X7", comp, "X7");
    pin_net_link(by, "VBLANK", comp, "VBLANK");
    pin_net_link(by, "Y5", comp, "Y5");
    pin_net_link(by, "Y6", comp, "Y6");
    pin_net_link(by, "Y7", comp, "Y7");
    for (i = 0; i < 6; i++) {
        snprintf(iname, sizeof(iname), "INDEX%d", i);
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(comp, iname, u24, aname);
    }
    for (i = 6; i <= 13; i++) {
        snprintf(aname, sizeof(aname), "A%d", i);
        pin_net_link(y2, "GND", u24, aname);
    }
    pin_net_link(u24, "O7", ui_ent(ui, "R1"), "1");
    pin_net_link(ui_ent(ui, "R1"), "2", enc, "RIN");
    pin_net_link(u24, "O6", ui_ent(ui, "R2"), "1");
    pin_net_link(ui_ent(ui, "R2"), "2", enc, "RIN");
    pin_net_link(u24, "O5", ui_ent(ui, "R3"), "1");
    pin_net_link(ui_ent(ui, "R3"), "2", enc, "RIN");
    pin_net_link(u24, "O4", ui_ent(ui, "R4"), "1");
    pin_net_link(ui_ent(ui, "R4"), "2", enc, "GIN");
    pin_net_link(u24, "O3", ui_ent(ui, "R5"), "1");
    pin_net_link(ui_ent(ui, "R5"), "2", enc, "GIN");
    pin_net_link(u24, "O2", ui_ent(ui, "R6"), "1");
    pin_net_link(ui_ent(ui, "R6"), "2", enc, "GIN");
    pin_net_link(u24, "O1", ui_ent(ui, "R7"), "1");
    pin_net_link(ui_ent(ui, "R7"), "2", enc, "BIN");
    pin_net_link(u24, "O0", ui_ent(ui, "R8"), "1");
    pin_net_link(ui_ent(ui, "R8"), "2", enc, "BIN");
    pin_net_link(enc, "RIN", ui_ent(ui, "R9"), "1");
    pin_net_link(ui_ent(ui, "R9"), "2", y2, "GND");
    pin_net_link(enc, "GIN", ui_ent(ui, "R10"), "1");
    pin_net_link(ui_ent(ui, "R10"), "2", y2, "GND");
    pin_net_link(enc, "BIN", ui_ent(ui, "R11"), "1");
    pin_net_link(ui_ent(ui, "R11"), "2", y2, "GND");
}

static int hit_pin_at(const R01aUi *ui, int board_mx, int board_my, int *chip_out, int *pin_out) {
    int rank;
    int best_d = R01A_PIN_HIT_PX * R01A_PIN_HIT_PX + 1;
    int best_c = -1;
    int best_p = -1;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        int i;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (i = 0; i < e->pin_count; i++) {
            int tx;
            int ty;
            int dx;
            int dy;
            int d;
            int maxd;
            if (!entity_tip_board(e, e->pins[i].number, &tx, &ty)) {
                continue;
            }
            dx = board_mx - tx;
            dy = board_my - ty;
            d = dx * dx + dy * dy;
            maxd = R01A_PIN_HIT_PX;
            if (e->visual == NS_ENTITY_VIS_PASSIVE || e->visual == NS_ENTITY_VIS_OSC) {
                maxd = 1;
            }
            if (d <= maxd * maxd && d < best_d) {
                best_d = d;
                best_c = ci;
                best_p = i;
            }
        }
        if (best_c == ci) {
            break;
        }
    }
    if (best_c < 0) {
        return 0;
    }
    if (chip_out) {
        *chip_out = best_c;
    }
    if (pin_out) {
        *pin_out = best_p;
    }
    return 1;
}

static int hit_resistor_leg(const R01aUi *ui, int board_mx, int board_my, int *chip_out, int *pin_out) {
    int rank;
    int half = R01A_JUMPER_HANDLE_PX / 2;
    int best_d = half * half + 1;
    int best_c = -1;
    int best_p = -1;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        const NsPassive *p;
        int pin;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual != NS_ENTITY_VIS_PASSIVE) {
            continue;
        }
        p = (const NsPassive *)e;
        if (p->kind != NS_PASSIVE_R) {
            continue;
        }
        for (pin = 1; pin <= 2; pin++) {
            int tx;
            int ty;
            int dx;
            int dy;
            int adx;
            int ady;
            int d;
            if (!ns_passive_tip_board(p, pin, &tx, &ty)) {
                continue;
            }
            dx = board_mx - tx;
            dy = board_my - ty;
            adx = dx < 0 ? -dx : dx;
            ady = dy < 0 ? -dy : dy;
            if (adx > half || ady > half) {
                continue;
            }
            d = dx * dx + dy * dy;
            if (d < best_d) {
                best_d = d;
                best_c = ci;
                best_p = pin;
            }
        }
    }
    if (best_c < 0) {
        return 0;
    }
    if (chip_out) {
        *chip_out = best_c;
    }
    if (pin_out) {
        *pin_out = best_p;
    }
    return 1;
}

static void stretch_resistor_leg(R01aUi *ui, NsPassive *p, int pin_num, int board_mx, int board_my) {
    NsBreadboard *bb;
    NsPbHole hole;
    int wx = board_mx;
    int wy = board_my;
    int extra;
    int idx;
    if (!p || p->kind != NS_PASSIVE_R) {
        return;
    }
    bb = hit_breadboard(ui, board_mx, board_my, &hole);
    if (bb) {
        ns_breadboard_hole_world(bb, hole, &wx, &wy);
    }
    ns_passive_set_leg_to(p, pin_num, wx, wy);
    idx = (pin_num == 1) ? 0 : 1;
    extra = p->leg_ext[idx];
    if (!bb) {
        extra = ((extra + NS_PB_PITCH / 2) / NS_PB_PITCH) * NS_PB_PITCH;
        ns_passive_set_leg_ext(p, pin_num, extra);
    }
}

static int pins_share_net(const NsEntity *a, int ap, const NsEntity *b, int bp) {
    int sa;
    int sb;
    if (!a || !b || ap < 0 || bp < 0 || ap >= a->pin_count || bp >= b->pin_count) {
        return 0;
    }
    sa = pin_net_find((NsEntity *)a, ap);
    sb = pin_net_find((NsEntity *)b, bp);
    if (sa < 0 || sb < 0) {
        return 0;
    }
    return pin_net_root(sa) == pin_net_root(sb);
}

static int pin_name_is_gnd(const char *name) {
    return name && (strcmp(name, "GND") == 0 || strcmp(name, "AGND") == 0 || strcmp(name, "DGND") == 0);
}

static int pin_name_is_vdd(const char *name) {
    return name && (strcmp(name, "VDD") == 0 || strcmp(name, "VCC") == 0 || strcmp(name, "APOS") == 0 ||
                    strcmp(name, "DPOS") == 0);
}

static int pin_on_hub_net(const R01aUi *ui, const NsEntity *e, int pin_index, const char *rail) {
    NsEntity *hub;
    const NsPin *rp;
    int rslot;
    int slot;
    if (!e || pin_index < 0 || pin_index >= e->pin_count || !rail) {
        return 0;
    }
    hub = ui_ent(ui, "Y2");
    if (!hub) {
        return 0;
    }
    rp = ns_entity_pin_named_const(hub, rail);
    if (!rp) {
        return 0;
    }
    rslot = pin_net_find(hub, (int)(rp - hub->pins));
    slot = pin_net_find((NsEntity *)e, pin_index);
    if (rslot < 0 || slot < 0) {
        return 0;
    }
    return pin_net_root(rslot) == pin_net_root(slot);
}

static int pin_on_gnd_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (pin_name_is_gnd(e->pins[pin_index].name)) {
        return 1;
    }
    return pin_on_hub_net(ui, e, pin_index, "GND");
}

static int pin_on_vdd_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (pin_name_is_vdd(e->pins[pin_index].name)) {
        return 1;
    }
    return pin_on_hub_net(ui, e, pin_index, "VDD");
}

static int pin_name_is_clk(const char *name) {
    return name && (strcmp(name, "DOT") == 0 || strcmp(name, "FSC") == 0 || strcmp(name, "CLK") == 0 ||
                    strcmp(name, "HSYNC") == 0 || strcmp(name, "CSYNC") == 0 || strcmp(name, "HWRAP") == 0 ||
                    strcmp(name, "FIN") == 0);
}

static int pin_on_clk_net(const R01aUi *ui, const NsEntity *e, int pin_index) {
    int slot;
    int root;
    int i;
    (void)ui;
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (pin_name_is_clk(e->pins[pin_index].name)) {
        return 1;
    }
    slot = pin_net_find((NsEntity *)e, pin_index);
    if (slot < 0) {
        return 0;
    }
    root = pin_net_root(slot);
    for (i = 0; i < g_pin_slot_count; i++) {
        const NsEntity *pe = g_pin_slots[i].entity;
        int pi = g_pin_slots[i].pin_index;
        if (!pe || pi < 0 || pi >= pe->pin_count || pin_net_root(i) != root) {
            continue;
        }
        if (pin_name_is_clk(pe->pins[pi].name)) {
            return 1;
        }
    }
    return 0;
}

enum {
    R01A_AIR_GND = 0,
    R01A_AIR_PWR,
    R01A_AIR_CLK,
    R01A_AIR_DATA
};

static int air_kind_for_pin(const R01aUi *ui, const NsEntity *e, int pin_index) {
    if (pin_on_gnd_net(ui, e, pin_index)) {
        return R01A_AIR_GND;
    }
    if (pin_on_vdd_net(ui, e, pin_index)) {
        return R01A_AIR_PWR;
    }
    if (pin_on_clk_net(ui, e, pin_index)) {
        return R01A_AIR_CLK;
    }
    return R01A_AIR_DATA;
}

static void air_kind_rgb(int kind, Uint8 *r, Uint8 *g, Uint8 *b) {
    Uint8 cr = 40;
    Uint8 cg = 200;
    Uint8 cb = 70;
    if (kind == R01A_AIR_GND) {
        cr = 0;
        cg = 0;
        cb = 0;
    } else if (kind == R01A_AIR_PWR) {
        cr = 220;
        cg = 40;
        cb = 40;
    } else if (kind == R01A_AIR_CLK) {
        cr = 40;
        cg = 190;
        cb = 220;
    }
    if (r) {
        *r = cr;
    }
    if (g) {
        *g = cg;
    }
    if (b) {
        *b = cb;
    }
}

static Uint8 air_pulse_alpha(void) {
    Uint32 t = SDL_GetTicks() % (R01A_AIR_PULSE_MS * 2u);
    if (t >= R01A_AIR_PULSE_MS) {
        t = (R01A_AIR_PULSE_MS * 2u) - t;
    }
    return (Uint8)((t * 255u) / R01A_AIR_PULSE_MS);
}

static int hole_is_neg_rail(NsPbHole h) {
    return h.lane == NS_PB_LANE_TOP_NEG || h.lane == NS_PB_LANE_BOT_NEG;
}

static int hole_is_pos_rail(NsPbHole h) {
    return h.lane == NS_PB_LANE_TOP_POS || h.lane == NS_PB_LANE_BOT_POS;
}

static int ui_hover_rail(const R01aUi *ui, int pos, int *wx, int *wy, const NsBreadboard **bb_out,
                         NsPbHole *h_out) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        const NsBreadboard *bb;
        NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        bb = (const NsBreadboard *)e;
        if (!bb->hover_valid) {
            continue;
        }
        if (pos) {
            if (!hole_is_pos_rail(bb->hover)) {
                continue;
            }
        } else if (!hole_is_neg_rail(bb->hover)) {
            continue;
        }
        if (wx && wy) {
            ns_breadboard_hole_world(bb, bb->hover, wx, wy);
        }
        if (bb_out) {
            *bb_out = bb;
        }
        if (h_out) {
            *h_out = bb->hover;
        }
        return 1;
    }
    return 0;
}

static int ui_hover_rail_xy(const R01aUi *ui, int pos, int *wx, int *wy) {
    return ui_hover_rail(ui, pos, wx, wy, NULL, NULL);
}

static int rail_right_col(int lane) {
    int col;
    for (col = NS_PB_COLS - 1; col >= 0; col--) {
        NsPbHole h = {col, lane};
        if (ns_breadboard_hole_exists(h)) {
            return col;
        }
    }
    return 0;
}

static NsBreadboard *ui_bb1(const R01aUi *ui) {
    int i;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD || !e->refdes) {
            continue;
        }
        if (strcmp(e->refdes, "BB1") == 0) {
            return (NsBreadboard *)e;
        }
    }
    return NULL;
}

static int hole_has_part(const R01aUi *ui, int wx, int wy) {
    int i;
    int pi;
    for (i = 0; i < ui->chip_count; i++) {
        const NsEntity *e = ui->chips[i];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int tx;
            int ty;
            if (!entity_tip_board(e, e->pins[pi].number, &tx, &ty)) {
                continue;
            }
            if (tx == wx && ty == wy) {
                return 1;
            }
        }
    }
    return 0;
}

static int ui_supply_rail_tip(const R01aUi *ui, int ax, int ay, int pos, int *gx, int *gy) {
    const NsBreadboard *bb1 = ui_bb1(ui);
    int best_d = -1;
    int found = 0;
    int col;
    int li;
    int lanes[2];
    int fb_x = 0;
    int fb_y = 0;
    int fb_ok = 0;
    if (!bb1) {
        return 0;
    }
    lanes[0] = pos ? NS_PB_LANE_TOP_POS : NS_PB_LANE_TOP_NEG;
    lanes[1] = pos ? NS_PB_LANE_BOT_POS : NS_PB_LANE_BOT_NEG;
    for (li = 0; li < 2; li++) {
        for (col = 0; col < NS_PB_COLS; col++) {
            NsPbHole h = {col, lanes[li]};
            int hx;
            int hy;
            int dx;
            int dy;
            int d;
            if (!ns_breadboard_hole_exists(h)) {
                continue;
            }
            ns_breadboard_hole_world(bb1, h, &hx, &hy);
            if (!fb_ok) {
                fb_x = hx;
                fb_y = hy;
                fb_ok = 1;
            }
            if (hole_has_part(ui, hx, hy)) {
                continue;
            }
            dx = hx - ax;
            dy = hy - ay;
            d = dx * dx + dy * dy;
            if (!found || d < best_d) {
                best_d = d;
                found = 1;
                if (gx) {
                    *gx = hx;
                }
                if (gy) {
                    *gy = hy;
                }
            }
        }
    }
    if (found) {
        return 1;
    }
    if (!fb_ok) {
        return 0;
    }
    if (gx) {
        *gx = fb_x;
    }
    if (gy) {
        *gy = fb_y;
    }
    return 1;
}

static void elbow_pts(int ax, int ay, int bx, int by, int h_first, int jog, int *x, int *y) {
    int x1;
    int y1;
    int x2;
    int y2;
    x[0] = ax;
    y[0] = ay;
    x[3] = bx;
    y[3] = by;
    if (ax == bx && ay == by) {
        x[1] = ax;
        y[1] = ay;
        x[2] = bx;
        y[2] = by;
        return;
    }
    if (jog == 0) {
        jog = R01A_HOVER_JOG;
    }
    if (h_first && ay != by) {
        x1 = (ax + bx) / 2;
        if (x1 == ax || x1 == bx) {
            x1 = ax + (bx >= ax ? jog : -jog);
        }
        y1 = ay;
        x2 = x1;
        y2 = by;
    } else if (!h_first && ax != bx) {
        x1 = ax;
        y1 = (ay + by) / 2;
        if (y1 == ay || y1 == by) {
            y1 = ay + (by >= ay ? jog : -jog);
        }
        x2 = bx;
        y2 = y1;
    } else if (ay == by) {
        x1 = ax;
        y1 = ay + jog;
        x2 = bx;
        y2 = y1;
    } else {
        x1 = ax + jog;
        y1 = ay;
        x2 = x1;
        y2 = by;
    }
    x[1] = x1;
    y[1] = y1;
    x[2] = x2;
    y[2] = y2;
}

static void jumper_custom_pts(int ax, int ay, int bx, int by, int h_first, int mid, int *x, int *y) {
    x[0] = ax;
    y[0] = ay;
    x[3] = bx;
    y[3] = by;
    if (h_first && ay != by) {
        x[1] = mid;
        y[1] = ay;
        x[2] = mid;
        y[2] = by;
    } else if (!h_first && ax != bx) {
        x[1] = ax;
        y[1] = mid;
        x[2] = bx;
        y[2] = mid;
    } else if (ay == by) {
        x[1] = ax;
        y[1] = mid;
        x[2] = bx;
        y[2] = mid;
    } else {
        x[1] = mid;
        y[1] = ay;
        x[2] = mid;
        y[2] = by;
    }
}

static void jumper_auto_route(int ax, int ay, int bx, int by, int *h_first, int *mid) {
    int x[4];
    int y[4];
    int hf = manhattan_h_first(ax, ay, bx, by);
    elbow_pts(ax, ay, bx, by, hf, R01A_HOVER_JOG, x, y);
    if (h_first) {
        *h_first = hf;
    }
    if (!mid) {
        return;
    }
    if ((hf && ay != by) || (ax == bx && ay != by)) {
        *mid = x[1];
    } else {
        *mid = y[1];
    }
}

static void jumper_route_from_mouse(int ax, int ay, int bx, int by, int which, int mx, int my, int *h_first,
                                   int *mid) {
    int xmin;
    int xmax;
    int ymin;
    int ymax;
    int out_x;
    int out_y;
    const int margin = 16;
    (void)which;
    if (!h_first || !mid) {
        return;
    }
    if (ay == by) {
        *h_first = 0;
        *mid = my;
        return;
    }
    if (ax == bx) {
        *h_first = 1;
        *mid = mx;
        return;
    }
    xmin = ax < bx ? ax : bx;
    xmax = ax > bx ? ax : bx;
    ymin = ay < by ? ay : by;
    ymax = ay > by ? ay : by;
    out_x = mx < xmin - margin || mx > xmax + margin;
    out_y = my < ymin - margin || my > ymax + margin;
    if (out_y && !out_x) {
        *h_first = 0;
        *mid = my;
    } else if (out_x && !out_y) {
        *h_first = 1;
        *mid = mx;
    } else if (*h_first) {
        *mid = mx;
    } else {
        *mid = my;
    }
}

static int manhattan_h_first(int ax, int ay, int bx, int by) {
    int dx = bx - ax;
    int dy = by - ay;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    return dx >= dy;
}

static void draw_2elbow(SDL_Renderer *r, int ax, int ay, int bx, int by, int h_first, int jog) {
    int x[4];
    int y[4];
    int i;
    elbow_pts(ax, ay, bx, by, h_first, jog, x, y);
    for (i = 0; i < 3; i++) {
        if (x[i] != x[i + 1] || y[i] != y[i + 1]) {
            SDL_RenderDrawLine(r, x[i], y[i], x[i + 1], y[i + 1]);
        }
    }
}

static void draw_ants_px(SDL_Renderer *r, int x, int y, int along, int phase) {
    int step = (along + phase) % R01A_ANTS_PERIOD;
    if (step == 0) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderDrawPoint(r, x, y);
    } else if (step == 2) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        SDL_RenderDrawPoint(r, x, y);
    }
}

static int iabs_px(int v) {
    return v < 0 ? -v : v;
}

static void draw_ants_seg(SDL_Renderer *r, int x0, int y0, int x1, int y1, int skip_first, int *along,
                         int phase) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = iabs_px(dx);
    int ady = iabs_px(dy);
    int steps = adx > ady ? adx : ady;
    int sx = (dx == 0) ? 0 : (dx > 0 ? 1 : -1);
    int sy = (dy == 0) ? 0 : (dy > 0 ? 1 : -1);
    int i;
    if (steps <= 0) {
        if (!skip_first) {
            draw_ants_px(r, x0, y0, *along, phase);
            (*along)++;
        }
        return;
    }
    for (i = 0; i <= steps; i++) {
        int x = x0 + sx * i;
        int y = y0 + sy * i;
        if (adx >= ady) {
            y = y0 + (sy * i * ady) / steps;
        } else {
            x = x0 + (sx * i * adx) / steps;
        }
        if (skip_first && i == 0) {
            continue;
        }
        draw_ants_px(r, x, y, *along, phase);
        (*along)++;
    }
}

static void draw_ants_2elbow(SDL_Renderer *r, int ax, int ay, int bx, int by, int h_first, int jog) {
    int x[4];
    int y[4];
    int i;
    int along = 0;
    int phase;
    if (ax == bx && ay == by) {
        return;
    }
    phase = (int)((SDL_GetTicks() / R01A_ANTS_MS) % R01A_ANTS_PERIOD);
    elbow_pts(ax, ay, bx, by, h_first, jog, x, y);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (i = 0; i < 3; i++) {
        if (x[i] == x[i + 1] && y[i] == y[i + 1]) {
            continue;
        }
        draw_ants_seg(r, x[i], y[i], x[i + 1], y[i + 1], along > 0, &along, phase);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static int phys_strip_find(int *parent, int s) {
    if (!parent || s < 0 || s >= NS_PB_STRIPS) {
        return s;
    }
    while (parent[s] != s) {
        parent[s] = parent[parent[s]];
        s = parent[s];
    }
    return s;
}

static void phys_strip_union(int *parent, int a, int b) {
    int ra;
    int rb;
    if (!parent) {
        return;
    }
    ra = phys_strip_find(parent, a);
    rb = phys_strip_find(parent, b);
    if (ra != rb && ra >= 0 && ra < NS_PB_STRIPS) {
        parent[rb] = ra;
    }
}

#define R01A_PHYS_BB_MAX (R01A_BB_EXTRA_MAX + 1)
#define R01A_PHYS_GNET (R01A_PHYS_BB_MAX * NS_PB_STRIPS)

static int jumper_intra_on_bb(const R01aJumper *j, const NsBreadboard *bb) {
    if (!j || !bb || !bb->base.refdes) {
        return 0;
    }
    return strcmp(r01a_jumper_a_ref(j), bb->base.refdes) == 0 &&
           strcmp(r01a_jumper_b_ref(j), bb->base.refdes) == 0;
}

static void phys_nets_on_bb(int *parent, const R01aBoard *board, const NsBreadboard *bb, int union_r) {
    int s;
    int i;
    for (s = 0; s < NS_PB_STRIPS; s++) {
        parent[s] = s;
    }
    if (!board || !bb) {
        return;
    }
    for (i = 0; i < board->jumper_count; i++) {
        if (!jumper_intra_on_bb(&board->jumpers[i], bb)) {
            continue;
        }
        phys_strip_union(parent, ns_breadboard_strip_id(board->jumpers[i].a),
                         ns_breadboard_strip_id(board->jumpers[i].b));
    }
    if (!union_r) {
        return;
    }
    {
        int ri;
        for (ri = 0; ri < board->passives.count; ri++) {
            const NsPassive *p = &board->passives.parts[ri];
            int t1x;
            int t1y;
            int t2x;
            int t2y;
            int s1;
            int s2;
            if (p->kind != NS_PASSIVE_R) {
                continue;
            }
            if (!ns_passive_tip_board(p, 1, &t1x, &t1y) || !ns_passive_tip_board(p, 2, &t2x, &t2y)) {
                continue;
            }
            if (!ns_breadboard_tip_strip(bb, t1x, t1y, &s1) || !ns_breadboard_tip_strip(bb, t2x, t2y, &s2)) {
                continue;
            }
            phys_strip_union(parent, s1, s2);
        }
    }
}

static int phys_g_find(int *parent, int s) {
    if (!parent || s < 0 || s >= R01A_PHYS_GNET) {
        return s;
    }
    while (parent[s] != s) {
        parent[s] = parent[parent[s]];
        s = parent[s];
    }
    return s;
}

static void phys_g_union(int *parent, int a, int b) {
    int ra;
    int rb;
    if (!parent) {
        return;
    }
    ra = phys_g_find(parent, a);
    rb = phys_g_find(parent, b);
    if (ra != rb && ra >= 0 && ra < R01A_PHYS_GNET) {
        parent[rb] = ra;
    }
}

static int phys_collect_bbs(const R01aUi *ui, const NsBreadboard **bbs) {
    int n = 0;
    int i;
    if (!ui || !bbs) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (n >= R01A_PHYS_BB_MAX) {
            break;
        }
        bbs[n++] = (const NsBreadboard *)e;
    }
    return n;
}

static int phys_bb_index(const NsBreadboard **bbs, int n, const NsBreadboard *bb) {
    int i;
    for (i = 0; i < n; i++) {
        if (bbs[i] == bb) {
            return i;
        }
    }
    return -1;
}

static int phys_bb_index_ref(const NsBreadboard **bbs, int n, const char *ref) {
    int i;
    if (!ref || !ref[0]) {
        ref = "BB1";
    }
    for (i = 0; i < n; i++) {
        if (bbs[i] && bbs[i]->base.refdes && strcmp(bbs[i]->base.refdes, ref) == 0) {
            return i;
        }
    }
    return -1;
}

static void phys_gnet_build(int *parent, const R01aUi *ui, const R01aBoard *board, const NsBreadboard **bbs,
                           int nbb, int union_r) {
    int i;
    int s;
    int local[NS_PB_STRIPS];
    int n = nbb * NS_PB_STRIPS;
    for (i = 0; i < n && i < R01A_PHYS_GNET; i++) {
        parent[i] = i;
    }
    if (!ui || !board) {
        return;
    }
    for (i = 0; i < nbb; i++) {
        phys_nets_on_bb(local, board, bbs[i], union_r);
        for (s = 0; s < NS_PB_STRIPS; s++) {
            phys_g_union(parent, i * NS_PB_STRIPS + s, i * NS_PB_STRIPS + phys_strip_find(local, s));
        }
    }
    for (i = 0; i < board->jumper_count; i++) {
        int ia = phys_bb_index_ref(bbs, nbb, r01a_jumper_a_ref(&board->jumpers[i]));
        int ib = phys_bb_index_ref(bbs, nbb, r01a_jumper_b_ref(&board->jumpers[i]));
        int sa;
        int sb;
        if (ia < 0 || ib < 0) {
            continue;
        }
        sa = ns_breadboard_strip_id(board->jumpers[i].a);
        sb = ns_breadboard_strip_id(board->jumpers[i].b);
        if (sa < 0 || sa >= NS_PB_STRIPS || sb < 0 || sb >= NS_PB_STRIPS) {
            continue;
        }
        phys_g_union(parent, ia * NS_PB_STRIPS + sa, ib * NS_PB_STRIPS + sb);
    }
}

static int pin_bb_strip(const R01aUi *ui, const NsEntity *e, int pin_index, const NsBreadboard **bb_out,
                        int *strip_out) {
    int tx;
    int ty;
    int i;
    if (!ui || !e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    if (!entity_tip_board(e, e->pins[pin_index].number, &tx, &ty)) {
        return 0;
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *be = ui->chips[i];
        int strip;
        if (!be || be->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (!ns_breadboard_tip_strip((const NsBreadboard *)be, tx, ty, &strip)) {
            continue;
        }
        if (bb_out) {
            *bb_out = (const NsBreadboard *)be;
        }
        if (strip_out) {
            *strip_out = strip;
        }
        return 1;
    }
    return 0;
}

static int pins_phys_connected(const R01aUi *ui, const R01aBoard *board, const NsEntity *a, int ap,
                              const NsEntity *b, int bp) {
    const NsBreadboard *bba;
    const NsBreadboard *bbb;
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int sa;
    int sb;
    int nbb;
    int ia;
    int ib;
    int parent[R01A_PHYS_GNET];
    if (!pin_bb_strip(ui, a, ap, &bba, &sa) || !pin_bb_strip(ui, b, bp, &bbb, &sb)) {
        return 0;
    }
    nbb = phys_collect_bbs(ui, bbs);
    ia = phys_bb_index(bbs, nbb, bba);
    ib = phys_bb_index(bbs, nbb, bbb);
    if (ia < 0 || ib < 0) {
        return 0;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 1);
    return phys_g_find(parent, ia * NS_PB_STRIPS + sa) == phys_g_find(parent, ib * NS_PB_STRIPS + sb);
}

static int pin_phys_on_hole_net(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                               const NsBreadboard *bb, NsPbHole h) {
    const NsBreadboard *pbb;
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int ps;
    int nbb;
    int ia;
    int ih;
    int parent[R01A_PHYS_GNET];
    int hs;
    if (!bb || !pin_bb_strip(ui, e, pin_index, &pbb, &ps)) {
        return 0;
    }
    nbb = phys_collect_bbs(ui, bbs);
    ia = phys_bb_index(bbs, nbb, pbb);
    ih = phys_bb_index(bbs, nbb, bb);
    hs = ns_breadboard_strip_id(h);
    if (ia < 0 || ih < 0 || hs < 0 || hs >= NS_PB_STRIPS) {
        return 0;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 1);
    return phys_g_find(parent, ia * NS_PB_STRIPS + ps) == phys_g_find(parent, ih * NS_PB_STRIPS + hs);
}

static int rail_strip_ids(int pos, int *ids) {
    int base = NS_PB_COLS * 2;
    if (pos) {
        ids[0] = base + 0;
        ids[1] = base + 2;
    } else {
        ids[0] = base + 1;
        ids[1] = base + 3;
    }
    return 2;
}

static int pin_phys_on_powered_rail(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                                   int pos) {
    const NsBreadboard *pbb;
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int ps;
    int nbb;
    int ia;
    int parent[R01A_PHYS_GNET];
    int pin_g;
    if (!pin_bb_strip(ui, e, pin_index, &pbb, &ps)) {
        return 0;
    }
    nbb = phys_collect_bbs(ui, bbs);
    ia = phys_bb_index(bbs, nbb, pbb);
    if (ia < 0) {
        return 0;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 0);
    pin_g = phys_g_find(parent, ia * NS_PB_STRIPS + ps);
    {
        int bi1 = phys_bb_index_ref(bbs, nbb, "BB1");
        int k;
        int lanes[2];
        if (bi1 >= 0) {
            lanes[0] = pos ? NS_PB_LANE_TOP_POS : NS_PB_LANE_TOP_NEG;
            lanes[1] = pos ? NS_PB_LANE_BOT_POS : NS_PB_LANE_BOT_NEG;
            for (k = 0; k < 2; k++) {
                NsPbHole rh = {0, lanes[k]};
                int rs = ns_breadboard_strip_id(rh);
                if (phys_g_find(parent, bi1 * NS_PB_STRIPS + rs) == pin_g) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int gnet_marked(int *seen, int *nseen, int max, int g) {
    int i;
    if (!seen || !nseen) {
        return 0;
    }
    for (i = 0; i < *nseen; i++) {
        if (seen[i] == g) {
            return 1;
        }
    }
    if (*nseen < max) {
        seen[(*nseen)++] = g;
    }
    return 0;
}

/* 1 = already on BB1 north. 0 = draw from *sx,*sy (pin or local rail). -1 = unseated. */
static int supply_air_src(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index, int pos,
                         int *sx, int *sy, int *g_out) {
    const NsBreadboard *pbb;
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int parent[R01A_PHYS_GNET];
    int ps;
    int nbb;
    int ia;
    int pin_g;
    int bi1;
    int i;
    int tx;
    int ty;
    if (!pin_bb_strip(ui, e, pin_index, &pbb, &ps) || !entity_tip_board(e, e->pins[pin_index].number, &tx, &ty)) {
        return -1;
    }
    nbb = phys_collect_bbs(ui, bbs);
    ia = phys_bb_index(bbs, nbb, pbb);
    if (ia < 0) {
        return -1;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 0);
    pin_g = phys_g_find(parent, ia * NS_PB_STRIPS + ps);
    if (g_out) {
        *g_out = pin_g;
    }
    bi1 = phys_bb_index_ref(bbs, nbb, "BB1");
    if (bi1 >= 0) {
        int k;
        int lanes[2];
        lanes[0] = pos ? NS_PB_LANE_TOP_POS : NS_PB_LANE_TOP_NEG;
        lanes[1] = pos ? NS_PB_LANE_BOT_POS : NS_PB_LANE_BOT_NEG;
        for (k = 0; k < 2; k++) {
            NsPbHole rh = {0, lanes[k]};
            int rs = ns_breadboard_strip_id(rh);
            if (phys_g_find(parent, bi1 * NS_PB_STRIPS + rs) == pin_g) {
                return 1;
            }
        }
    }
    for (i = 0; i < nbb; i++) {
        int li;
        int lanes[2];
        lanes[0] = pos ? NS_PB_LANE_TOP_POS : NS_PB_LANE_TOP_NEG;
        lanes[1] = pos ? NS_PB_LANE_BOT_POS : NS_PB_LANE_BOT_NEG;
        for (li = 0; li < 2; li++) {
            int half;
            for (half = 0; half < 2; half++) {
                NsPbHole rh = {half ? NS_PB_RAIL_GAP_END : 0, lanes[li]};
                int rs;
                int col;
                if (!ns_breadboard_hole_exists(rh)) {
                    rh.col = half ? rail_right_col(lanes[li]) : 0;
                    if (!ns_breadboard_hole_exists(rh)) {
                        continue;
                    }
                }
                rs = ns_breadboard_strip_id(rh);
                if (phys_g_find(parent, i * NS_PB_STRIPS + rs) != pin_g) {
                    continue;
                }
                for (col = 0; col < NS_PB_COLS; col++) {
                    NsPbHole h = {col, lanes[li]};
                    int hx;
                    int hy;
                    if (!ns_breadboard_hole_exists(h) || ns_breadboard_strip_id(h) != rs) {
                        continue;
                    }
                    ns_breadboard_hole_world(bbs[i], h, &hx, &hy);
                    if (hole_has_part(ui, hx, hy)) {
                        continue;
                    }
                    if (sx) {
                        *sx = hx;
                    }
                    if (sy) {
                        *sy = hy;
                    }
                    return 0;
                }
                ns_breadboard_hole_world(bbs[i], rh, sx, sy);
                return 0;
            }
        }
    }
    if (sx) {
        *sx = tx;
    }
    if (sy) {
        *sy = ty;
    }
    return 0;
}

static int hover_skip_pin_pair(const R01aUi *ui, const R01aBoard *board, const NsEntity *a, int ap,
                               const NsEntity *b, int bp) {
    return pins_phys_connected(ui, board, a, ap, b, bp);
}

static int hover_skip_rail_pin(const R01aUi *ui, const R01aBoard *board, const NsEntity *e, int pin_index,
                              int pos, const NsBreadboard *hover_bb, NsPbHole hover_h) {
    if (pin_phys_on_hole_net(ui, board, e, pin_index, hover_bb, hover_h)) {
        return 1;
    }
    return pin_phys_on_powered_rail(ui, board, e, pin_index, pos);
}

static int jumper_pair_kind(int a, int b) {
    if (a == R01A_AIR_PWR || b == R01A_AIR_PWR) {
        return R01A_AIR_PWR;
    }
    if (a == R01A_AIR_GND || b == R01A_AIR_GND) {
        return R01A_AIR_GND;
    }
    if (a == R01A_AIR_CLK || b == R01A_AIR_CLK) {
        return R01A_AIR_CLK;
    }
    return R01A_AIR_DATA;
}

static void jumper_kind_rgb(int kind, uint8_t *r, uint8_t *g, uint8_t *b) {
    int pal = R01A_JUMPER_PAL_DATA;
    const Uint8 *rgb;
    if (kind == R01A_AIR_PWR) {
        pal = R01A_JUMPER_PAL_PWR;
    } else if (kind == R01A_AIR_GND) {
        pal = R01A_JUMPER_PAL_GND;
    } else if (kind == R01A_AIR_CLK) {
        pal = R01A_JUMPER_PAL_CLK;
    }
    rgb = k_jumper_rgb[pal];
    if (r) {
        *r = rgb[0];
    }
    if (g) {
        *g = rgb[1];
    }
    if (b) {
        *b = rgb[2];
    }
}

static int hole_net_kind(const R01aUi *ui, const R01aBoard *board, const NsBreadboard *bb, NsPbHole h) {
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int parent[R01A_PHYS_GNET];
    int ids[4];
    int nids;
    int nbb;
    int bi;
    int hs;
    int g;
    int i;
    int k;
    int ci;
    int saw_clk = 0;
    if (!bb) {
        return R01A_AIR_DATA;
    }
    if (hole_is_neg_rail(h)) {
        return R01A_AIR_GND;
    }
    if (hole_is_pos_rail(h)) {
        return R01A_AIR_PWR;
    }
    nbb = phys_collect_bbs(ui, bbs);
    bi = phys_bb_index(bbs, nbb, bb);
    hs = ns_breadboard_strip_id(h);
    if (bi < 0 || hs < 0 || hs >= NS_PB_STRIPS) {
        return R01A_AIR_DATA;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 0);
    g = phys_g_find(parent, bi * NS_PB_STRIPS + hs);
    nids = rail_strip_ids(0, ids);
    for (i = 0; i < nbb; i++) {
        for (k = 0; k < nids; k++) {
            if (phys_g_find(parent, i * NS_PB_STRIPS + ids[k]) == g) {
                return R01A_AIR_GND;
            }
        }
    }
    nids = rail_strip_ids(1, ids);
    for (i = 0; i < nbb; i++) {
        for (k = 0; k < nids; k++) {
            if (phys_g_find(parent, i * NS_PB_STRIPS + ids[k]) == g) {
                return R01A_AIR_PWR;
            }
        }
    }
    for (ci = 0; ci < ui->chip_count; ci++) {
        const NsEntity *ce = ui->chips[ci];
        int pi;
        if (!ce) {
            continue;
        }
        for (pi = 0; pi < ce->pin_count; pi++) {
            const NsBreadboard *pbb = NULL;
            int ps;
            int pbi;
            if (!pin_bb_strip(ui, ce, pi, &pbb, &ps)) {
                continue;
            }
            pbi = phys_bb_index(bbs, nbb, pbb);
            if (pbi < 0 || phys_g_find(parent, pbi * NS_PB_STRIPS + ps) != g) {
                continue;
            }
            if (pin_name_is_gnd(ce->pins[pi].name)) {
                return R01A_AIR_GND;
            }
            if (pin_name_is_vdd(ce->pins[pi].name)) {
                return R01A_AIR_PWR;
            }
            if (pin_name_is_clk(ce->pins[pi].name)) {
                saw_clk = 1;
            }
        }
    }
    return saw_clk ? R01A_AIR_CLK : R01A_AIR_DATA;
}

static void jumper_color_for_holes(const R01aUi *ui, const R01aBoard *board, const NsBreadboard *ba, NsPbHole a,
                                  const NsBreadboard *bb, NsPbHole b, uint8_t *r, uint8_t *g, uint8_t *bl) {
    jumper_kind_rgb(jumper_pair_kind(hole_net_kind(ui, board, ba, a), hole_net_kind(ui, board, bb, b)), r, g,
                    bl);
}

static void jumper_apply_auto_color(R01aUi *ui, R01aBoard *board, int ji) {
    R01aJumper *j;
    if (!ui || !board || ji < 0 || ji >= board->jumper_count) {
        return;
    }
    j = &board->jumpers[ji];
    jumper_color_for_holes(ui, board, ui_bb_named(ui, r01a_jumper_a_ref(j)), j->a,
                           ui_bb_named(ui, r01a_jumper_b_ref(j)), j->b, &j->r, &j->g, &j->bcol);
}

static int jumper_arm_rgb(const R01aUi *ui, const R01aBoard *board, uint8_t *r, uint8_t *g, uint8_t *b) {
    NsBreadboard *hover_bb;
    NsPbHole hole;
    int bx;
    int by;
    if (!ui || !board || !ui->jumper_arm || !ui->jumper_bb) {
        return 0;
    }
    logic_to_board(ui, ui->mouse_lx, ui->mouse_ly, &bx, &by);
    hover_bb = hit_breadboard(ui, bx, by, &hole);
    jumper_color_for_holes(ui, board, ui->jumper_bb, ui->jumper_from, hover_bb ? hover_bb : ui->jumper_bb,
                           hover_bb ? hole : ui->jumper_from, r, g, b);
    return 1;
}

static void guide_consider(int mx, int my, int x, int y, int skip_x, int skip_y, int *best_d, int *found,
                          int *ox, int *oy) {
    int dx;
    int dy;
    int d;
    if (x == skip_x && y == skip_y) {
        return;
    }
    dx = x - mx;
    dy = y - my;
    d = dx * dx + dy * dy;
    if (!*found || d < *best_d) {
        *best_d = d;
        *found = 1;
        *ox = x;
        *oy = y;
    }
}

static int nearest_supply_hole(const R01aUi *ui, int ax, int ay, int pos, int skip_x, int skip_y, int *gx,
                              int *gy) {
    int best_d = -1;
    int found = 0;
    int i;
    int col;
    int lanes[2];
    lanes[0] = pos ? NS_PB_LANE_TOP_POS : NS_PB_LANE_TOP_NEG;
    lanes[1] = pos ? NS_PB_LANE_BOT_POS : NS_PB_LANE_BOT_NEG;
    for (i = 0; i < ui->chip_count; i++) {
        const NsBreadboard *bb;
        int li;
        if (!ui->chips[i] || ui->chips[i]->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        bb = (const NsBreadboard *)ui->chips[i];
        for (li = 0; li < 2; li++) {
            for (col = 0; col < NS_PB_COLS; col++) {
                NsPbHole h;
                int hx;
                int hy;
                h.col = col;
                h.lane = lanes[li];
                if (!ns_breadboard_hole_exists(h)) {
                    continue;
                }
                ns_breadboard_hole_world(bb, h, &hx, &hy);
                guide_consider(ax, ay, hx, hy, skip_x, skip_y, &best_d, &found, gx, gy);
            }
        }
    }
    return found;
}

static int jumper_guide_dest(const R01aUi *ui, const R01aBoard *board, int *dx, int *dy) {
    const NsBreadboard *bbs[R01A_PHYS_BB_MAX];
    int parent[R01A_PHYS_GNET];
    int ids[4];
    int nids;
    int nbb;
    int bi;
    int hs;
    int g;
    int on_rail;
    int kind;
    int mx;
    int my;
    int sx;
    int sy;
    int best_d = -1;
    int found = 0;
    int ox = 0;
    int oy = 0;
    int ci;
    int i;
    int k;
    if (!ui || !board || !ui->jumper_arm || !ui->jumper_bb || !dx || !dy) {
        return 0;
    }
    logic_to_board(ui, ui->mouse_lx, ui->mouse_ly, &mx, &my);
    ns_breadboard_hole_world(ui->jumper_bb, ui->jumper_from, &sx, &sy);
    kind = hole_net_kind(ui, board, ui->jumper_bb, ui->jumper_from);
    nbb = phys_collect_bbs(ui, bbs);
    bi = phys_bb_index(bbs, nbb, ui->jumper_bb);
    hs = ns_breadboard_strip_id(ui->jumper_from);
    if (bi < 0 || hs < 0 || hs >= NS_PB_STRIPS) {
        return 0;
    }
    phys_gnet_build(parent, ui, board, bbs, nbb, 1);
    g = phys_g_find(parent, bi * NS_PB_STRIPS + hs);
    on_rail = hole_is_neg_rail(ui->jumper_from) || hole_is_pos_rail(ui->jumper_from);
    if (kind == R01A_AIR_GND || kind == R01A_AIR_PWR) {
        int pos = (kind == R01A_AIR_PWR);
        int start_rail_hole = hole_is_neg_rail(ui->jumper_from) || hole_is_pos_rail(ui->jumper_from);
        nids = rail_strip_ids(pos, ids);
        for (i = 0; i < nbb; i++) {
            for (k = 0; k < nids; k++) {
                if (phys_g_find(parent, i * NS_PB_STRIPS + ids[k]) == g) {
                    on_rail = 1;
                }
            }
        }
        if (on_rail && start_rail_hole) {
            for (ci = 0; ci < ui->chip_count; ci++) {
                const NsEntity *e = ui->chips[ci];
                int pi;
                if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
                    continue;
                }
                for (pi = 0; pi < e->pin_count; pi++) {
                    int tx;
                    int ty;
                    if (pos) {
                        if (!pin_on_vdd_net(ui, e, pi)) {
                            continue;
                        }
                    } else if (!pin_on_gnd_net(ui, e, pi)) {
                        continue;
                    }
                    if (pin_phys_on_powered_rail(ui, board, e, pi, pos)) {
                        continue;
                    }
                    if (pin_phys_on_hole_net(ui, board, e, pi, ui->jumper_bb, ui->jumper_from)) {
                        continue;
                    }
                    if (!entity_tip_board(e, e->pins[pi].number, &tx, &ty)) {
                        continue;
                    }
                    guide_consider(mx, my, tx, ty, sx, sy, &best_d, &found, &ox, &oy);
                }
            }
        } else if (!on_rail) {
            found = nearest_supply_hole(ui, mx, my, pos, sx, sy, &ox, &oy);
        }
    } else {
        for (ci = 0; ci < ui->chip_count; ci++) {
            const NsEntity *src = ui->chips[ci];
            int sp;
            if (!src || src->visual == NS_ENTITY_VIS_BREADBOARD || src->visual == NS_ENTITY_VIS_DISPLAY) {
                continue;
            }
            for (sp = 0; sp < src->pin_count; sp++) {
                int oi;
                if (!pin_phys_on_hole_net(ui, board, src, sp, ui->jumper_bb, ui->jumper_from)) {
                    continue;
                }
                if (pin_on_gnd_net(ui, src, sp) || pin_on_vdd_net(ui, src, sp)) {
                    continue;
                }
                for (oi = 0; oi < ui->chip_count; oi++) {
                    const NsEntity *dst = ui->chips[oi];
                    int dp;
                    if (!dst || dst == src) {
                        continue;
                    }
                    if (dst->visual == NS_ENTITY_VIS_BREADBOARD || dst->visual == NS_ENTITY_VIS_DISPLAY) {
                        continue;
                    }
                    for (dp = 0; dp < dst->pin_count; dp++) {
                        int tx;
                        int ty;
                        if (!pins_share_net(src, sp, dst, dp)) {
                            continue;
                        }
                        if (pin_name_is_gnd(dst->pins[dp].name) || pin_on_gnd_net(ui, dst, dp)) {
                            continue;
                        }
                        if (pins_phys_connected(ui, board, src, sp, dst, dp)) {
                            continue;
                        }
                        if (!entity_tip_board(dst, dst->pins[dp].number, &tx, &ty)) {
                            continue;
                        }
                        guide_consider(mx, my, tx, ty, sx, sy, &best_d, &found, &ox, &oy);
                    }
                }
            }
        }
    }
    if (!found) {
        return 0;
    }
    *dx = ox;
    *dy = oy;
    return 1;
}

static void draw_air_line(SDL_Renderer *r, const R01aUi *ui, int ax, int ay, int bx, int by, int kind) {
    Uint8 cr;
    Uint8 cg;
    Uint8 cb;
    Uint8 a;
    int sx0;
    int sy0;
    int sx1;
    int sy1;
    if (ax == bx && ay == by) {
        return;
    }
    a = air_pulse_alpha();
    if (a == 0) {
        return;
    }
    air_kind_rgb(kind, &cr, &cg, &cb);
    sx0 = board_sx(ui, ax);
    sy0 = board_sy(ui, ay);
    sx1 = board_sx(ui, bx);
    sy1 = board_sy(ui, by);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);
    if (R01A_AIR_ELBOWS) {
        draw_2elbow(r, sx0, sy0, sx1, sy1, manhattan_h_first(ax, ay, bx, by), R01A_HOVER_JOG);
    } else {
        SDL_RenderDrawLine(r, sx0, sy0, sx1, sy1);
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}

static void draw_rail_net_from(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, int ax, int ay,
                              int vdd, const NsBreadboard *hover_bb, NsPbHole hover_h) {
    int i;
    int pi;
    int kind = vdd ? R01A_AIR_PWR : R01A_AIR_GND;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int bx;
            int by;
            if (vdd) {
                if (!pin_on_vdd_net(ui, e, pi)) {
                    continue;
                }
            } else if (!pin_on_gnd_net(ui, e, pi)) {
                continue;
            }
            if (hover_skip_rail_pin(ui, board, e, pi, vdd, hover_bb, hover_h)) {
                continue;
            }
            if (!entity_tip_board(e, e->pins[pi].number, &bx, &by)) {
                continue;
            }
            draw_air_line(r, ui, ax, ay, bx, by, kind);
        }
    }
}

static void draw_hover_supply(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, const NsEntity *src_e,
                             int src_pin, int ax, int ay, int pos, int *seen, int *nseen) {
    int gx = 0;
    int gy = 0;
    int sx = ax;
    int sy = ay;
    int g = -1;
    int st = supply_air_src(ui, board, src_e, src_pin, pos, &sx, &sy, &g);
    if (st != 0) {
        return;
    }
    if (g >= 0 && gnet_marked(seen, nseen, 128, g)) {
        return;
    }
    if (!ui_supply_rail_tip(ui, sx, sy, pos, &gx, &gy)) {
        return;
    }
    if (sx == gx && sy == gy) {
        return;
    }
    draw_air_line(r, ui, sx, sy, gx, gy, pos ? R01A_AIR_PWR : R01A_AIR_GND);
}

static void draw_hover_ground(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, const NsEntity *src_e,
                             int src_pin, int ax, int ay, int *seen, int *nseen) {
    draw_hover_supply(r, ui, board, src_e, src_pin, ax, ay, 0, seen, nseen);
}

static void draw_pin_air_from(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, int chip_i,
                             int pin_i, int later_chips_only, int *seen, int *nseen) {
    const NsEntity *src_e;
    int ax;
    int ay;
    int i;
    int pi;
    int saw_gnd = 0;
    int src_kind;

    if (chip_i < 0 || chip_i >= ui->chip_count || pin_i < 0) {
        return;
    }
    src_e = ui->chips[chip_i];
    if (!src_e || pin_i >= src_e->pin_count) {
        return;
    }
    if (src_e->visual == NS_ENTITY_VIS_BREADBOARD || src_e->visual == NS_ENTITY_VIS_DISPLAY) {
        return;
    }
    if (!entity_tip_board(src_e, src_e->pins[pin_i].number, &ax, &ay)) {
        return;
    }
    src_kind = air_kind_for_pin(ui, src_e, pin_i);
    if (pin_on_gnd_net(ui, src_e, pin_i)) {
        draw_hover_supply(r, ui, board, src_e, pin_i, ax, ay, 0, seen, nseen);
        return;
    }
    if (pin_on_vdd_net(ui, src_e, pin_i)) {
        draw_hover_supply(r, ui, board, src_e, pin_i, ax, ay, 1, seen, nseen);
        return;
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e == src_e) {
            continue;
        }
        if (later_chips_only && i <= chip_i) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int bx;
            int by;
            if (!pins_share_net(src_e, pin_i, e, pi)) {
                continue;
            }
            if (pin_name_is_gnd(e->pins[pi].name) || pin_on_gnd_net(ui, e, pi)) {
                saw_gnd = 1;
                continue;
            }
            if (hover_skip_pin_pair(ui, board, src_e, pin_i, e, pi)) {
                continue;
            }
            if (!entity_tip_board(e, e->pins[pi].number, &bx, &by)) {
                continue;
            }
            draw_air_line(r, ui, ax, ay, bx, by, src_kind);
        }
    }
    if (saw_gnd) {
        draw_hover_ground(r, ui, board, src_e, pin_i, ax, ay, seen, nseen);
    }
}

static void draw_all_air_wires(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    int i;
    int pi;
    int seen[128];
    int nseen = 0;
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (!e || e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            draw_pin_air_from(r, ui, board, i, pi, 1, seen, &nseen);
        }
    }
}

static void draw_hover_wires(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board) {
    if (ui->hover_chip < 0 || ui->hover_pin < 0) {
        int rax;
        int ray;
        const NsBreadboard *hbb = NULL;
        NsPbHole hh = {0, 0};
        if (ui_hover_rail(ui, 0, &rax, &ray, &hbb, &hh)) {
            draw_rail_net_from(r, ui, board, rax, ray, 0, hbb, hh);
        } else if (ui_hover_rail(ui, 1, &rax, &ray, &hbb, &hh)) {
            draw_rail_net_from(r, ui, board, rax, ray, 1, hbb, hh);
        }
        return;
    }
    draw_pin_air_from(r, ui, board, ui->hover_chip, ui->hover_pin, 0, NULL, NULL);
}

static void pulse_pixel(SDL_Renderer *r, int sx, int sy, Uint8 cr, Uint8 cg, Uint8 cb, int on) {
    if (on) {
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
    } else {
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    }
    SDL_RenderDrawPoint(r, sx, sy);
}

static void draw_manual_pin_pulses(SDL_Renderer *r, const R01aUi *ui, const R01aBoard *board, const NsEntity *e) {
    uint8_t on_hole[NS_MAX_PINS];
    int seated;
    int open;
    Uint32 now;
    int ok_on;
    int i;
    (void)board;
    if (!ui || !ui->pin_gray || !e || e->pin_count <= 0) {
        return;
    }
    if (e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
        return;
    }
    seated = 0;
    memset(on_hole, 0, sizeof(on_hole));
    for (i = 0; i < e->pin_count; i++) {
        const NsBreadboard *pbb = NULL;
        int strip = -1;
        if (pin_bb_strip(ui, e, i, &pbb, &strip)) {
            on_hole[i] = 1;
            seated++;
        }
    }
    if (seated == 0) {
        return;
    }
    open = seated < e->pin_count;
    now = SDL_GetTicks();
    ok_on = ((now / R01A_PULSE_OK_MS) & 1u) == 0;
    for (i = 0; i < e->pin_count; i++) {
        int tx;
        int ty;
        if (!entity_tip_board(e, e->pins[i].number, &tx, &ty)) {
            continue;
        }
        if (on_hole[i] &&
            (e->pins[i].level == NS_LVL_X || r01a_netlist_pin_shorted(e, i))) {
            /* Drawn in a later pass so jumper lines cannot cover the blink. */
            continue;
        } else if (on_hole[i]) {
            pulse_pixel(r, board_sx(ui, tx), board_sy(ui, ty), 50, 210, 80, ok_on);
        } else if (open) {
            pulse_pixel(r, board_sx(ui, tx), board_sy(ui, ty), 240, 140, 30, ok_on);
        }
    }
}

static void draw_short_pin_pulses(SDL_Renderer *r, const R01aUi *ui) {
    Uint32 now;
    int short_on;
    int ci;
    if (!ui || !ui->pin_gray) {
        return;
    }
    now = SDL_GetTicks();
    short_on = ((now / R01A_PULSE_SHORT_MS) & 1u) == 0;
    for (ci = 0; ci < ui->chip_count; ci++) {
        const NsEntity *e = ui->chips[ci];
        int i;
        if (!e || e->pin_count <= 0) {
            continue;
        }
        if (e->visual == NS_ENTITY_VIS_BREADBOARD || e->visual == NS_ENTITY_VIS_DISPLAY) {
            continue;
        }
        for (i = 0; i < e->pin_count; i++) {
            const NsBreadboard *pbb = NULL;
            int strip = -1;
            int tx;
            int ty;
            if (!pin_bb_strip(ui, e, i, &pbb, &strip)) {
                continue;
            }
            if (e->pins[i].level != NS_LVL_X && !r01a_netlist_pin_shorted(e, i)) {
                continue;
            }
            if (!entity_tip_board(e, e->pins[i].number, &tx, &ty)) {
                continue;
            }
            pulse_pixel(r, board_sx(ui, tx), board_sy(ui, ty), 220, 40, 40, short_on);
        }
    }
}

static void draw_entity(SDL_Renderer *r, R01aUi *ui, R01aBoard *board, NsEntity *e, int selected) {
    if (!e || chip_hidden(e)) {
        return;
    }
    switch (e->visual) {
    case NS_ENTITY_VIS_BREADBOARD:
        ns_breadboard_draw(r, (const NsBreadboard *)e, board_sx(ui, e->board_x), board_sy(ui, e->board_y),
                           selected);
        break;
    case NS_ENTITY_VIS_OSC:
        draw_osc(r, ui, e, selected);
        break;
    case NS_ENTITY_VIS_DISPLAY:
        draw_lcd(r, ui, (NsVideoSink *)e, selected);
        break;
    case NS_ENTITY_VIS_PASSIVE: {
        const NsPassive *p = (const NsPassive *)e;
        ns_passive_draw(r, p, board_sx(ui, p->pivot_x), board_sy(ui, p->pivot_y), selected);
        break;
    }
    case NS_ENTITY_VIS_IC:
    default:
        draw_ic(r, ui, e, selected);
        break;
    }
    draw_manual_pin_pulses(r, ui, board, e);
}

static void draw_tooltip(SDL_Renderer *r, int lx, int ly, const char *text) {
    int tw;
    int pad = 4;
    int box_x;
    int box_y;
    int box_w;
    int box_h = r01a_font_line_h() + pad * 2;

    if (!text || !text[0]) {
        return;
    }
    tw = r01a_font_text_width(text);
    box_w = tw + pad * 2;
    box_x = lx + 14;
    box_y = ly + 16;
    if (box_x + box_w > NS_LOGIC_W - 4) {
        box_x = lx - box_w - 8;
    }
    if (box_y + box_h > NS_LOGIC_H - 4) {
        box_y = ly - box_h - 8;
    }
    if (box_x < 4) {
        box_x = 4;
    }
    if (box_y < 4) {
        box_y = 4;
    }
    fill_rect(r, box_x, box_y, box_w, box_h, 255, 245, 180);
    draw_rect(r, box_x, box_y, box_w, box_h, 200, 180, 100);
    r01a_font_draw(r, box_x + pad, box_y + pad, text, 0, 0, 0);
}

static void fill_tooltip(const R01aUi *ui, char *out, size_t out_len) {
    int chip_i;
    const NsEntity *e;
    if (!out || out_len == 0) {
        return;
    }
    out[0] = '\0';
    if (ui->hover_jumper >= 0) {
        snprintf(out, out_len, "jumper");
        return;
    }
    if (ui_hover_rail_xy(ui, 0, NULL, NULL)) {
        snprintf(out, out_len, "GND rail");
        return;
    }
    if (ui_hover_rail_xy(ui, 1, NULL, NULL)) {
        snprintf(out, out_len, "VDD rail");
        return;
    }
    chip_i = hit_top_chip(ui, ui->mouse_lx, ui->mouse_ly);
    if (chip_i < 0) {
        return;
    }
    e = ui->chips[chip_i];
    if (!e) {
        return;
    }
    if (ui->hover_pin >= 0 && ui->hover_chip == chip_i && ui->hover_pin < e->pin_count &&
        e->pins[ui->hover_pin].name) {
        snprintf(out, out_len, "%s %s", e->refdes ? e->refdes : "?", e->pins[ui->hover_pin].name);
        return;
    }
    if (e->visual == NS_ENTITY_VIS_PASSIVE) {
        const NsPassive *p = (const NsPassive *)e;
        snprintf(out, out_len, "%s %s", e->refdes ? e->refdes : "?", p->value[0] ? p->value : e->part);
        return;
    }
    snprintf(out, out_len, "%s (%s)", e->refdes ? e->refdes : "?", e->part ? e->part : "?");
}

static const char *wire_mode_label(int mode) {
    return (mode == R01A_WIRE_MANUAL) ? "Manual" : "Auto";
}

static void wire_mode_btn_rect(int mode, SDL_Rect *rc) {
    int tw = r01a_font_text_width(wire_mode_label(mode));
    rc->x = 4;
    rc->y = 2;
    rc->w = tw + 12;
    rc->h = r01a_font_line_h() + 4;
}

static int point_in_rect(int x, int y, const SDL_Rect *rc) {
    return x >= rc->x && y >= rc->y && x < rc->x + rc->w && y < rc->y + rc->h;
}

enum {
    R01A_CTX_R = 0,
    R01A_CTX_CCAP,
    R01A_CTX_ECAP,
    R01A_CTX_OSC,
    R01A_CTX_D,
    R01A_CTX_BB,
    R01A_CTX_COUNT
};

static const char *k_ctx_label[R01A_CTX_COUNT] = {
    "Add resistor", "Add ceramic cap", "Add electrolytic", "Add oscillator", "Add diode", "Add breadboard"};

static const char *k_ctx_value[R01A_CTX_COUNT] = {"33", "100nF", "220uF", "8.000MHz", "", ""};

static int ctx_item_h(void) {
    return r01a_font_line_h() + 4;
}

static int ctx_menu_w(void) {
    int i;
    int w = 0;
    for (i = 0; i < R01A_CTX_COUNT; i++) {
        int tw = r01a_font_text_width(k_ctx_label[i]);
        if (tw > w) {
            w = tw;
        }
    }
    return w + 16;
}

static void ctx_menu_geom(const R01aUi *ui, int *x, int *y, int *w, int *h) {
    int mw = ctx_menu_w();
    int mh = ctx_item_h() * R01A_CTX_COUNT;
    int mx = ui->ctx_lx;
    int my = ui->ctx_ly;
    if (mx + mw > NS_LOGIC_W - 4) {
        mx = NS_LOGIC_W - 4 - mw;
    }
    if (my + mh > NS_LOGIC_H - 4) {
        my = NS_LOGIC_H - 4 - mh;
    }
    if (mx < 4) {
        mx = 4;
    }
    if (my < 4) {
        my = 4;
    }
    if (x) {
        *x = mx;
    }
    if (y) {
        *y = my;
    }
    if (w) {
        *w = mw;
    }
    if (h) {
        *h = mh;
    }
}

static int ctx_hit_item(const R01aUi *ui, int lx, int ly) {
    int x;
    int y;
    int w;
    int h;
    int item;
    if (!ui->ctx_open) {
        return -1;
    }
    ctx_menu_geom(ui, &x, &y, &w, &h);
    if (lx < x || ly < y || lx >= x + w || ly >= y + h) {
        return -1;
    }
    item = (ly - y) / ctx_item_h();
    if (item < 0 || item >= R01A_CTX_COUNT) {
        return -1;
    }
    return item;
}

static void draw_ctx_menu(SDL_Renderer *r, const R01aUi *ui) {
    int x;
    int y;
    int w;
    int h;
    int i;
    int ih;
    if (!ui->ctx_open) {
        return;
    }
    ctx_menu_geom(ui, &x, &y, &w, &h);
    ih = ctx_item_h();
    fill_rect(r, x, y, w, h, 28, 32, 28);
    draw_rect(r, x, y, w, h, 180, 180, 120);
    for (i = 0; i < R01A_CTX_COUNT; i++) {
        int iy = y + i * ih;
        if (ui->mouse_lx >= x && ui->mouse_lx < x + w && ui->mouse_ly >= iy && ui->mouse_ly < iy + ih) {
            fill_rect(r, x + 1, iy, w - 2, ih, 50, 70, 50);
        }
        r01a_font_draw(r, x + 8, iy + 2, k_ctx_label[i], 230, 230, 200);
    }
}

static NsBreadboard *ui_bb_named(const R01aUi *ui, const char *ref) {
    int i;
    if (!ref || !ref[0]) {
        ref = "BB1";
    }
    for (i = 0; i < ui->chip_count; i++) {
        NsEntity *e = ui->chips[i];
        if (e && e->visual == NS_ENTITY_VIS_BREADBOARD && e->refdes && strcmp(e->refdes, ref) == 0) {
            return (NsBreadboard *)e;
        }
    }
    return ui_breadboard(ui);
}

static NsBreadboard *hit_breadboard(const R01aUi *ui, int mx, int my, NsPbHole *hole) {
    int rank;
    for (rank = ui->chip_count - 1; rank >= 0; rank--) {
        int ci = ui->chip_z[rank];
        NsEntity *e;
        NsPbHole h;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        e = ui->chips[ci];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (ns_breadboard_hit_hole((const NsBreadboard *)e, mx, my, &h)) {
            if (hole) {
                *hole = h;
            }
            return (NsBreadboard *)e;
        }
    }
    return NULL;
}

static void ctx_apply(R01aUi *ui, R01aBoard *board, int item) {
    sel_clear(ui);
    if (item == R01A_CTX_BB) {
        if (r01a_board_add_breadboard(board, ui->ctx_bx, ui->ctx_by)) {
            bind_chips(ui, board);
        }
        return;
    }
    {
        NsPassiveKind kind = NS_PASSIVE_R;
        if (item == R01A_CTX_CCAP) {
            kind = NS_PASSIVE_CCAP;
        } else if (item == R01A_CTX_ECAP) {
            kind = NS_PASSIVE_ECAP;
        } else if (item == R01A_CTX_OSC) {
            kind = NS_PASSIVE_OSC4LEGS;
        } else if (item == R01A_CTX_D) {
            kind = NS_PASSIVE_D;
        }
        if (r01a_board_add_passive(board, kind, k_ctx_value[item], ui->ctx_bx, ui->ctx_by)) {
            bind_chips(ui, board);
        }
    }
}

static int jumper_seg_hit(int px, int py, int x0, int y0, int x1, int y1, int *d2_out) {
    int vx = x1 - x0;
    int vy = y1 - y0;
    long c2 = (long)vx * vx + (long)vy * vy;
    int cx;
    int cy;
    int adx;
    int ady;
    int half = R01A_JUMPER_SEG_HALF_PX;
    if (c2 <= 0) {
        cx = x0;
        cy = y0;
    } else {
        long c1 = (long)vx * (px - x0) + (long)vy * (py - y0);
        if (c1 <= 0) {
            cx = x0;
            cy = y0;
        } else if (c1 >= c2) {
            cx = x1;
            cy = y1;
        } else {
            cx = x0 + (int)((vx * c1 + c2 / 2) / c2);
            cy = y0 + (int)((vy * c1 + c2 / 2) / c2);
        }
    }
    adx = px - cx;
    ady = py - cy;
    if (adx < 0) {
        adx = -adx;
    }
    if (ady < 0) {
        ady = -ady;
    }
    if (adx > half || ady > half) {
        return 0;
    }
    if (d2_out) {
        *d2_out = adx * adx + ady * ady;
    }
    return 1;
}

static int jumper_world_ends(const R01aUi *ui, const R01aJumper *j, int *x0, int *y0, int *x1, int *y1) {
    const NsBreadboard *bba;
    const NsBreadboard *bbb;
    if (!j) {
        return 0;
    }
    bba = ui_bb_named(ui, r01a_jumper_a_ref(j));
    bbb = ui_bb_named(ui, r01a_jumper_b_ref(j));
    if (!bba || !bbb) {
        return 0;
    }
    ns_breadboard_hole_world(bba, j->a, x0, y0);
    ns_breadboard_hole_world(bbb, j->b, x1, y1);
    return 1;
}

static int jumper_board_path(const R01aUi *ui, const R01aBoard *board, int ji, int *x, int *y) {
    int ax;
    int ay;
    int bx;
    int by;
    const R01aJumper *j;
    if (!ui || !board || !x || !y || ji < 0 || ji >= board->jumper_count) {
        return 0;
    }
    j = &board->jumpers[ji];
    if (!jumper_world_ends(ui, j, &ax, &ay, &bx, &by)) {
        return 0;
    }
    if (ui->drag_jumper == ji && ui->drag_jumper_preview_ok && ui->drag_jumper_preview_bb &&
        ui->drag_jumper_elbow < 0) {
        int hx;
        int hy;
        ns_breadboard_hole_world(ui->drag_jumper_preview_bb, ui->drag_jumper_preview, &hx, &hy);
        if (ui->drag_jumper_end) {
            bx = hx;
            by = hy;
        } else {
            ax = hx;
            ay = hy;
        }
    }
    if (ui->drag_jumper == ji && ui->drag_jumper_elbow >= 0) {
        jumper_custom_pts(ax, ay, bx, by, ui->drag_elbow_hf, ui->drag_elbow_mid, x, y);
    } else if (j->route) {
        jumper_custom_pts(ax, ay, bx, by, j->h_first, j->mid, x, y);
    } else {
        elbow_pts(ax, ay, bx, by, manhattan_h_first(ax, ay, bx, by), R01A_HOVER_JOG, x, y);
    }
    return 1;
}

static void jumper_effective_route(const R01aUi *ui, const R01aJumper *j, int ax, int ay, int bx, int by,
                                  int *h_first, int *mid) {
    (void)ui;
    if (j && j->route) {
        if (h_first) {
            *h_first = j->h_first;
        }
        if (mid) {
            *mid = j->mid;
        }
        return;
    }
    jumper_auto_route(ax, ay, bx, by, h_first, mid);
}

static void jumper_rgb_draw(const R01aJumper *j, int bright, Uint8 *r, Uint8 *g, Uint8 *b) {
    Uint8 cr = j ? j->r : 220;
    Uint8 cg = j ? j->g : 160;
    Uint8 cb = j ? j->bcol : 40;
    if (bright) {
        cr = (Uint8)(cr + (255 - cr) / 2);
        cg = (Uint8)(cg + (255 - cg) / 2);
        cb = (Uint8)(cb + (255 - cb) / 2);
    }
    if (r) {
        *r = cr;
    }
    if (g) {
        *g = cg;
    }
    if (b) {
        *b = cb;
    }
}

static void draw_end_handle(SDL_Renderer *r, int x, int y, Uint8 R, Uint8 G, Uint8 B) {
    int half = R01A_JUMPER_HANDLE_PX / 2;
    fill_rect(r, x - half, y - half, R01A_JUMPER_HANDLE_PX, R01A_JUMPER_HANDLE_PX, R, G, B);
}

static int jumper_handle_hit(int px, int py, int hx, int hy, int *d2_out) {
    int dx = px - hx;
    int dy = py - hy;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int half = R01A_JUMPER_HANDLE_PX / 2;
    if (adx > half || ady > half) {
        return 0;
    }
    if (d2_out) {
        *d2_out = dx * dx + dy * dy;
    }
    return 1;
}

static int hit_jumper_end(R01aUi *ui, const R01aBoard *board, int bx, int by, int *j_out, int *end_out) {
    int k;
    int best = R01A_JUMPER_HANDLE_PX * R01A_JUMPER_HANDLE_PX;
    int found = 0;
    jumper_z_sync(ui, board);
    for (k = 0; k < ui->jumper_z_n; k++) {
        int i = ui->jumper_z[k];
        int x0;
        int y0;
        int x1;
        int y1;
        int d;
        if (i < 0 || i >= board->jumper_count) {
            continue;
        }
        if (!jumper_world_ends(ui, &board->jumpers[i], &x0, &y0, &x1, &y1)) {
            continue;
        }
        if (jumper_handle_hit(bx, by, x0, y0, &d) && d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (end_out) {
                *end_out = 0;
            }
        }
        if (jumper_handle_hit(bx, by, x1, y1, &d) && d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (end_out) {
                *end_out = 1;
            }
        }
    }
    return found;
}

static int hit_jumper_elbow(R01aUi *ui, const R01aBoard *board, int bx, int by, int *j_out, int *elbow_out) {
    int k;
    int best = R01A_JUMPER_HANDLE_PX * R01A_JUMPER_HANDLE_PX;
    int found = 0;
    jumper_z_sync(ui, board);
    for (k = 0; k < ui->jumper_z_n; k++) {
        int i = ui->jumper_z[k];
        int x[4];
        int y[4];
        int d;
        if (i < 0 || i >= board->jumper_count) {
            continue;
        }
        if (!jumper_board_path(ui, board, i, x, y)) {
            continue;
        }
        if (jumper_handle_hit(bx, by, x[1], y[1], &d) && d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (elbow_out) {
                *elbow_out = 0;
            }
        }
        if (jumper_handle_hit(bx, by, x[2], y[2], &d) && d <= best) {
            best = d;
            found = 1;
            if (j_out) {
                *j_out = i;
            }
            if (elbow_out) {
                *elbow_out = 1;
            }
        }
    }
    return found;
}

static int hit_jumper_body(R01aUi *ui, const R01aBoard *board, int bx, int by) {
    int k;
    int best = 0x7fffffff;
    int found = -1;
    jumper_z_sync(ui, board);
    for (k = 0; k < ui->jumper_z_n; k++) {
        int i = ui->jumper_z[k];
        int x[4];
        int y[4];
        int s;
        if (i < 0 || i >= board->jumper_count) {
            continue;
        }
        if (!jumper_board_path(ui, board, i, x, y)) {
            continue;
        }
        for (s = 0; s < 3; s++) {
            int d;
            if (jumper_seg_hit(bx, by, x[s], y[s], x[s + 1], y[s + 1], &d) && d <= best) {
                best = d;
                found = i;
            }
        }
    }
    return found;
}

static void jumper_sel_set_one(R01aUi *ui, R01aBoard *board, int ji) {
    sel_clear(ui);
    if (ji < 0 || ji >= R01A_JUMPER_MAX) {
        return;
    }
    ui->jumper_sel[ji] = 1;
    jumper_z_raise(ui, board, ji);
}

static int jumper_sel_any(const R01aUi *ui) {
    int i;
    for (i = 0; i < R01A_JUMPER_MAX; i++) {
        if (ui->jumper_sel[i]) {
            return 1;
        }
    }
    return 0;
}

static int jumper_palette_i(const R01aJumper *j) {
    int i;
    if (!j) {
        return 0;
    }
    for (i = 0; i < R01A_JUMPER_COLORS; i++) {
        if (j->r == k_jumper_rgb[i][0] && j->g == k_jumper_rgb[i][1] && j->bcol == k_jumper_rgb[i][2]) {
            return i;
        }
    }
    return 0;
}

static void jumper_cycle_selected(R01aUi *ui, R01aBoard *board, int dir) {
    int i;
    int last = ui->jumper_color_i;
    if (!dir) {
        return;
    }
    for (i = 0; i < board->jumper_count; i++) {
        int ci;
        const Uint8 *rgb;
        if (!ui->jumper_sel[i]) {
            continue;
        }
        ci = jumper_palette_i(&board->jumpers[i]);
        if (dir > 0) {
            ci = (ci + 1) % R01A_JUMPER_COLORS;
        } else {
            ci = (ci + R01A_JUMPER_COLORS - 1) % R01A_JUMPER_COLORS;
        }
        rgb = k_jumper_rgb[ci];
        board->jumpers[i].r = rgb[0];
        board->jumpers[i].g = rgb[1];
        board->jumpers[i].bcol = rgb[2];
        last = ci;
    }
    ui->jumper_color_i = last;
}

static void jumper_delete_selected(R01aUi *ui, R01aBoard *board) {
    R01aJumper gone[R01A_JUMPER_MAX];
    int ngone = 0;
    int i;
    for (i = 0; i < board->jumper_count && ngone < R01A_JUMPER_MAX; i++) {
        if (ui->jumper_sel[i]) {
            gone[ngone++] = board->jumpers[i];
        }
    }
    for (i = board->jumper_count - 1; i >= 0; i--) {
        if (!ui->jumper_sel[i]) {
            continue;
        }
        r01a_board_jumper_remove(board, i);
        {
            int k;
            for (k = i; k < board->jumper_count; k++) {
                ui->jumper_sel[k] = ui->jumper_sel[k + 1];
            }
            ui->jumper_sel[board->jumper_count] = 0;
            {
                int z;
                int zdst = 0;
                for (z = 0; z < ui->jumper_z_n; z++) {
                    int zi = ui->jumper_z[z];
                    if (zi == i) {
                        continue;
                    }
                    if (zi > i) {
                        zi--;
                    }
                    ui->jumper_z[zdst++] = (uint8_t)zi;
                }
                ui->jumper_z_n = zdst;
            }
        }
    }
    if (ngone > 0) {
        undo_push_jmp_del(ui, gone, ngone);
    }
    ui->hover_jumper = -1;
    ui->drag_jumper = -1;
    ui->drag_jumper_preview_ok = 0;
    ui->drag_jumper_elbow = -1;
}

static void breadboard_delete_selected(R01aUi *ui, R01aBoard *board) {
    char refs[R01A_BB_EXTRA_MAX + 1][R01A_BB_REF_LEN];
    int nref = 0;
    int i;
    int removed = 0;
    R01aUndoCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.kind = R01A_UNDO_BB_DEL;
    for (i = 0; i < ui->chip_count; i++) {
        const NsEntity *e;
        if (!ui->chip_sel[i]) {
            continue;
        }
        e = ui->chips[i];
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD || !e->refdes) {
            continue;
        }
        if (nref >= R01A_BB_EXTRA_MAX + 1) {
            break;
        }
        snprintf(refs[nref], sizeof(refs[nref]), "%s", e->refdes);
        pose_from_entity(&cmd.before[cmd.npose], e);
        cmd.npose++;
        nref++;
    }
    for (i = 0; i < board->jumper_count && cmd.njmp < R01A_JUMPER_MAX; i++) {
        int r;
        for (r = 0; r < nref; r++) {
            if (strcmp(r01a_jumper_a_ref(&board->jumpers[i]), refs[r]) == 0 ||
                strcmp(r01a_jumper_b_ref(&board->jumpers[i]), refs[r]) == 0) {
                cmd.jmps[cmd.njmp++] = board->jumpers[i];
                break;
            }
        }
    }
    for (i = 0; i < nref; i++) {
        NsEntity *e = r01a_board_entity_by_refdes(board, refs[i]);
        if (!e || e->visual != NS_ENTITY_VIS_BREADBOARD) {
            continue;
        }
        if (ui->jumper_bb == (NsBreadboard *)e) {
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
        }
        if (r01a_board_remove_breadboard(board, (NsBreadboard *)e)) {
            removed = 1;
        }
    }
    if (removed) {
        if (cmd.npose > 0) {
            undo_push(ui, &cmd);
        }
        sel_clear(ui);
        bind_chips(ui, board);
        pin_net_build(ui);
    }
}

static void draw_jumpers(SDL_Renderer *r, R01aUi *ui, const R01aBoard *board) {
    int k;
    jumper_z_sync(ui, board);
    for (k = 0; k < ui->jumper_z_n; k++) {
        int i = ui->jumper_z[k];
        int x[4];
        int y[4];
        int s;
        Uint8 cr;
        Uint8 cg;
        Uint8 cb;
        int sel;
        int hover;
        int bright;
        if (i < 0 || i >= board->jumper_count) {
            continue;
        }
        sel = ui->jumper_sel[i];
        hover = (ui->hover_jumper == i);
        bright = sel || hover;
        if (!jumper_board_path(ui, board, i, x, y)) {
            continue;
        }
        jumper_rgb_draw(&board->jumpers[i], bright, &cr, &cg, &cb);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        for (s = 0; s < 3; s++) {
            int x0 = x[s];
            int y0 = y[s];
            int x1 = x[s + 1];
            int y1 = y[s + 1];
            if (x0 != x1 || y0 != y1) {
                SDL_RenderDrawLine(r, board_sx(ui, x0), board_sy(ui, y0), board_sx(ui, x1), board_sy(ui, y1));
            }
        }
        if (sel || hover) {
            draw_end_handle(r, board_sx(ui, x[0]), board_sy(ui, y[0]), sel ? 255 : cr, sel ? 230 : cg,
                            sel ? 80 : cb);
            draw_end_handle(r, board_sx(ui, x[3]), board_sy(ui, y[3]), sel ? 255 : cr, sel ? 230 : cg,
                            sel ? 80 : cb);
            draw_end_handle(r, board_sx(ui, x[1]), board_sy(ui, y[1]), sel ? 255 : 240, sel ? 255 : cg,
                            sel ? 255 : cb);
            draw_end_handle(r, board_sx(ui, x[2]), board_sy(ui, y[2]), sel ? 255 : 240, sel ? 255 : cg,
                            sel ? 255 : cb);
        }
    }
    if (ui->jumper_arm && ui->jumper_bb) {
        int x0;
        int y0;
        uint8_t cr;
        uint8_t cg;
        uint8_t cb;
        if (!jumper_arm_rgb(ui, board, &cr, &cg, &cb)) {
            cr = k_jumper_rgb[R01A_JUMPER_PAL_DATA][0];
            cg = k_jumper_rgb[R01A_JUMPER_PAL_DATA][1];
            cb = k_jumper_rgb[R01A_JUMPER_PAL_DATA][2];
        }
        ns_breadboard_hole_world(ui->jumper_bb, ui->jumper_from, &x0, &y0);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        draw_2elbow(r, board_sx(ui, x0), board_sy(ui, y0), view_mouse_x(ui), view_mouse_y(ui),
                    manhattan_h_first(board_sx(ui, x0), board_sy(ui, y0), view_mouse_x(ui), view_mouse_y(ui)),
                    R01A_HOVER_JOG);
        draw_end_handle(r, board_sx(ui, x0), board_sy(ui, y0), cr, cg, cb);
        {
            int dx;
            int dy;
            if (jumper_guide_dest(ui, board, &dx, &dy)) {
                int sx1 = board_sx(ui, dx);
                int sy1 = board_sy(ui, dy);
                draw_ants_2elbow(r, view_mouse_x(ui), view_mouse_y(ui), sx1, sy1,
                                 manhattan_h_first(view_mouse_x(ui), view_mouse_y(ui), sx1, sy1),
                                 R01A_HOVER_JOG);
            }
        }
    }
}

static void present_frame(R01aUi *ui);

static void draw_frame(R01aUi *ui, R01aBoard *board) {
    char status[128];
    char tip[96];
    int rank;
    SDL_Rect btn;
    int hud_x;
    R01aNetIssue issues[R01A_NET_ISSUE_MAX];
    int ni = 0;

    SDL_SetRenderTarget(ui->rend, ui->target);
    SDL_SetRenderDrawColor(ui->rend, NS_BOARD_BG_R, NS_BOARD_BG_G, NS_BOARD_BG_B, 255);
    SDL_RenderClear(ui->rend);

    ui->pin_gray = r01a_board_wire_mode(board) == R01A_WIRE_MANUAL;
    if (ui->pin_gray) {
        ni = r01a_netlist_check(board, issues, R01A_NET_ISSUE_MAX);
        /* Plot uses the Compositor latch, so a picture can appear before INDEX
         * reaches the PROM. Manual stays dark until the breadboard matches. */
        if (ni > 0) {
            ns_video_sink_clear(&board->sink);
        }
    }

    SDL_RenderSetScale(ui->rend, (float)canvas_zoom(ui), (float)canvas_zoom(ui));
    for (rank = 0; rank < ui->chip_count; rank++) {
        int ci = ui->chip_z[rank];
        int selected;
        if (ci < 0 || ci >= ui->chip_count) {
            continue;
        }
        selected = ui->chip_sel[ci] || ci == ui->selected;
        draw_entity(ui->rend, ui, board, ui->chips[ci], selected);
    }
    if (ui->air_always) {
        draw_all_air_wires(ui->rend, ui, board);
    } else {
        draw_hover_wires(ui->rend, ui, board);
    }
    draw_jumpers(ui->rend, ui, board);

    if (ui->box_sel) {
        int x0 = board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx0 : ui->box_bx1);
        int y0 = board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by0 : ui->box_by1);
        int x1 = board_sx(ui, ui->box_bx0 < ui->box_bx1 ? ui->box_bx1 : ui->box_bx0);
        int y1 = board_sy(ui, ui->box_by0 < ui->box_by1 ? ui->box_by1 : ui->box_by0);
        int bw = x1 - x0;
        int bh = y1 - y0;
        if (bw < 1) {
            bw = 1;
        }
        if (bh < 1) {
            bh = 1;
        }
        fill_rect_a(ui->rend, x0, y0, bw, bh, 80, 180, 120, 40);
        draw_rect(ui->rend, x0, y0, bw, bh, 80, 180, 120);
    }
    draw_short_pin_pulses(ui->rend, ui);
    SDL_RenderSetScale(ui->rend, 1.0f, 1.0f);

    wire_mode_btn_rect(r01a_board_wire_mode(board), &btn);
    fill_rect(ui->rend, btn.x, btn.y, btn.w, btn.h, 18, 28, 22);
    draw_rect(ui->rend, btn.x, btn.y, btn.w, btn.h,
              (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? 220 : 180,
              (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? 180 : 180, 80);
    r01a_font_draw(ui->rend, btn.x + 6, btn.y + 2, wire_mode_label(r01a_board_wire_mode(board)), 230, 230,
                   200);

    ns_island_group_fill_status(r01a_board_group(board), status, sizeof(status));
    hud_x = btn.x + btn.w + 10;
    if (ui->jumper_mode) {
        r01a_font_draw(ui->rend, hud_x, 2, "JUMPER", 255, 210, 80);
        hud_x += r01a_font_text_width("JUMPER") + 8;
    }
    if (ui->jumper_arm || jumper_sel_any(ui)) {
        uint8_t cr = 0;
        uint8_t cg = 0;
        uint8_t cb = 0;
        const Uint8 *rgb = k_jumper_rgb[ui->jumper_color_i % R01A_JUMPER_COLORS];
        if (ui->jumper_arm && jumper_arm_rgb(ui, board, &cr, &cg, &cb)) {
            rgb = NULL;
        } else if (!ui->jumper_arm) {
            int i;
            for (i = 0; i < board->jumper_count; i++) {
                if (ui->jumper_sel[i]) {
                    rgb = k_jumper_rgb[jumper_palette_i(&board->jumpers[i])];
                    break;
                }
            }
        }
        if (rgb) {
            fill_rect(ui->rend, hud_x, 3, 10, 8, rgb[0], rgb[1], rgb[2]);
        } else {
            fill_rect(ui->rend, hud_x, 3, 10, 8, cr, cg, cb);
        }
        hud_x += 16;
    }
    if (ui->jumper_mode || ui->jumper_arm || jumper_sel_any(ui)) {
        hud_x += 4;
    }
    if (ui->air_always) {
        r01a_font_draw(ui->rend, hud_x, 2, "AIR", 80, 200, 230);
        hud_x += r01a_font_text_width("AIR") + 8;
    }
    if (canvas_zoom(ui) > 1) {
        char zlab[8];
        snprintf(zlab, sizeof(zlab), "%dx", canvas_zoom(ui));
        r01a_font_draw(ui->rend, hud_x, 2, zlab, 200, 200, 180);
        hud_x += r01a_font_text_width(zlab) + 8;
    }
    r01a_font_draw_a(ui->rend, hud_x, 2, board->running ? "RUN" : "PAUSE", 180, 180, 120, 180);
    hud_x += r01a_font_text_width("PAUSE") + 10;
    r01a_font_draw_a(ui->rend, hud_x, 2, status, 200, 210, 200, 160);

    {
        char fps[16];
        int tw;
        Uint32 now = SDL_GetTicks();
        ui->fps_n++;
        if (ui->fps_t0 == 0) {
            ui->fps_t0 = now;
        }
        if ((now - ui->fps_t0) >= 1000u) {
            ui->fps = ui->fps_n;
            ui->fps_n = 0;
            ui->fps_t0 = now;
        }
        snprintf(fps, sizeof(fps), "%d", ui->fps);
        tw = r01a_font_text_width(fps);
        r01a_font_draw_a(ui->rend, NS_LOGIC_W - 4 - tw, 2, fps, 160, 170, 150, 180);
    }

    if (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) {
        int y = btn.y + btn.h + 3;
        int shown;
        int i;
        if (ni <= 0) {
            r01a_font_draw_a(ui->rend, 8, y, "NET ok", 80, 200, 110, 200);
        } else {
            char head[32];
            snprintf(head, sizeof(head), "NET %d", ni);
            r01a_font_draw(ui->rend, 8, y, head, 240, 90, 80);
            y += r01a_font_line_h();
            shown = ni < 6 ? ni : 6;
            for (i = 0; i < shown; i++) {
                Uint8 cr = 240;
                Uint8 cg = 180;
                Uint8 cb = 70;
                if (issues[i].kind != R01A_NET_MISSING && issues[i].kind != R01A_NET_UNSEATED) {
                    cr = 255;
                    cg = 90;
                    cb = 80;
                }
                r01a_font_draw(ui->rend, 8, y, issues[i].text, cr, cg, cb);
                y += r01a_font_line_h();
            }
            if (ni > shown) {
                char more[24];
                snprintf(more, sizeof(more), "+%d more", ni - shown);
                r01a_font_draw_a(ui->rend, 8, y, more, 200, 160, 80, 200);
            }
        }
    }

    if (ui->ctx_open) {
        draw_ctx_menu(ui->rend, ui);
    } else if (SDL_GetTicks() >= ui->tip_show_at && !ui->drag_pan && ui->drag_chip < 0 && ui->drag_jumper < 0 &&
               !ui->box_sel && !ui->jumper_arm) {
        fill_tooltip(ui, tip, sizeof(tip));
        if (tip[0]) {
            draw_tooltip(ui->rend, ui->mouse_lx, ui->mouse_ly, tip);
        }
    }
    present_frame(ui);
}

static void update_scale(R01aUi *ui) {
    int ww;
    int wh;
    int sx;
    int sy;
    SDL_GetWindowSize(ui->win, &ww, &wh);
    sx = ww / NS_LOGIC_W;
    sy = wh / NS_LOGIC_H;
    ui->scale = sx < sy ? sx : sy;
    if (ui->scale < 1) {
        ui->scale = 1;
    }
}

static void logic_from_window(const R01aUi *ui, int wx, int wy, int *lx, int *ly) {
    int ww;
    int wh;
    int scale = ui->scale > 0 ? ui->scale : 1;
    int draw_w;
    int draw_h;
    int ox;
    int oy;
    SDL_GetWindowSize(ui->win, &ww, &wh);
    draw_w = NS_LOGIC_W * scale;
    draw_h = NS_LOGIC_H * scale;
    ox = (ww - draw_w) / 2;
    oy = (wh - draw_h) / 2;
    *lx = (wx - ox) / scale;
    *ly = (wy - oy) / scale;
}

static void present_frame(R01aUi *ui) {
    int ww;
    int wh;
    int draw_w;
    int draw_h;
    SDL_Rect dst;

    update_scale(ui);
    SDL_GetWindowSize(ui->win, &ww, &wh);
    draw_w = NS_LOGIC_W * ui->scale;
    draw_h = NS_LOGIC_H * ui->scale;
    dst.x = (ww - draw_w) / 2;
    dst.y = (wh - draw_h) / 2;
    dst.w = draw_w;
    dst.h = draw_h;
    SDL_SetRenderTarget(ui->rend, NULL);
    SDL_SetRenderDrawColor(ui->rend, 0, 0, 0, 255);
    SDL_RenderClear(ui->rend);
    if (ui->target) {
        SDL_RenderCopy(ui->rend, ui->target, NULL, &dst);
    }
    SDL_RenderPresent(ui->rend);
}

static void tip_reset(R01aUi *ui, int mx, int my) {
    ui->tip_stable_mx = mx;
    ui->tip_stable_my = my;
    ui->tip_show_at = SDL_GetTicks() + R01A_UI_TOOLTIP_DELAY_MS;
}

static void sim_frame(R01aBoard *board) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 budget = (freq * (Uint64)R01A_SIM_BUDGET_MS) / 1000u;
    int n = 0;

    while (n < R01A_SIM_MAX_STEPS_PER_FRAME) {
        r01a_board_step_dots(board, (uint32_t)R01A_DOTS_PER_STEP);
        n++;
        if ((SDL_GetPerformanceCounter() - t0) >= budget) {
            break;
        }
    }
}

static int handle_event(R01aUi *ui, R01aBoard *board, const SDL_Event *e, int lx, int ly) {
    int board_mx = 0;
    int board_my = 0;

    ui->mouse_lx = lx;
    ui->mouse_ly = ly;
    logic_to_board(ui, lx, ly, &board_mx, &board_my);
    if (!hit_pin_at(ui, board_mx, board_my, &ui->hover_chip, &ui->hover_pin)) {
        ui->hover_chip = -1;
        ui->hover_pin = -1;
    }

    if (e->type == SDL_MOUSEMOTION && (lx != ui->tip_stable_mx || ly != ui->tip_stable_my)) {
        tip_reset(ui, lx, ly);
    }
    {
        int i;
        NsBreadboard *bb;
        NsPbHole hole;
        for (i = 0; i < ui->chip_count; i++) {
            if (ui->chips[i] && ui->chips[i]->visual == NS_ENTITY_VIS_BREADBOARD) {
                ns_breadboard_clear_hover((NsBreadboard *)ui->chips[i]);
            }
        }
        bb = hit_breadboard(ui, board_mx, board_my, &hole);
        if (bb) {
            ns_breadboard_set_hover(bb, 1, hole);
        }
    }
    if (ui->drag_jumper >= 0) {
        ui->hover_jumper = ui->drag_jumper;
    } else if (ui->jumper_arm) {
        ui->hover_jumper = -1;
    } else {
        int je;
        int end_dummy;
        if (hit_jumper_end(ui, board, board_mx, board_my, &je, &end_dummy)) {
            ui->hover_jumper = je;
        } else if (hit_jumper_elbow(ui, board, board_mx, board_my, &je, &end_dummy)) {
            ui->hover_jumper = je;
        } else {
            ui->hover_jumper = hit_jumper_body(ui, board, board_mx, board_my);
        }
    }
    if (e->type == SDL_MOUSEWHEEL) {
        int dy = e->wheel.y;
        int dx = e->wheel.x;
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            dy = -dy;
            dx = -dx;
        }
        if (SDL_GetModState() & KMOD_CTRL) {
            if (dy > 0) {
                canvas_zoom_by(ui, 1, lx, ly);
            } else if (dy < 0) {
                canvas_zoom_by(ui, -1, lx, ly);
            }
            return 1;
        }
        if (ui->jumper_arm) {
            if (dy > 0) {
                ui->jumper_color_i = (ui->jumper_color_i + 1) % R01A_JUMPER_COLORS;
            } else if (dy < 0) {
                ui->jumper_color_i = (ui->jumper_color_i + R01A_JUMPER_COLORS - 1) % R01A_JUMPER_COLORS;
            }
            return 1;
        }
        if (jumper_sel_any(ui)) {
            if (dy > 0) {
                jumper_cycle_selected(ui, board, 1);
            } else if (dy < 0) {
                jumper_cycle_selected(ui, board, -1);
            }
            return 1;
        }
        {
            int step = 32 / canvas_zoom(ui);
            if (step < 1) {
                step = 1;
            }
            ui->pan_x -= dx * step;
            ui->pan_y -= dy * step;
            clamp_pan(ui);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 1;
        pan_grab_begin(ui, lx, ly);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        ui->right_armed = 1;
        ui->right_pan = 0;
        ui->right_lx = lx;
        ui->right_ly = ly;
        ui->drag_last_x = lx;
        ui->drag_last_y = ly;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_RIGHT) {
        int chip_i;
        ui->right_armed = 0;
        if (ui->right_pan || ui->drag_pan) {
            ui->right_pan = 0;
            ui->drag_pan = 0;
            return 1;
        }
        chip_i = hit_top_chip(ui, lx, ly);
        if (chip_i < 0) {
            ui->ctx_open = 1;
            ui->ctx_lx = lx;
            ui->ctx_ly = ly;
            ui->ctx_bx = board_mx;
            ui->ctx_by = board_my;
        } else {
            ui->ctx_open = 0;
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->right_armed && !ui->right_pan) {
        int dx = lx - ui->right_lx;
        int dy = ly - ui->right_ly;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dx > 5 || dy > 5) {
            ui->right_pan = 1;
            ui->drag_pan = 1;
            ui->ctx_open = 0;
            pan_grab_begin(ui, lx, ly);
        }
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        pan_grab_to(ui, lx, ly);
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->box_sel) {
        ui->box_bx1 = board_mx;
        ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_jumper >= 0) {
        if (ui->drag_jumper_elbow >= 0) {
            int ax;
            int ay;
            int bx;
            int by;
            int hf = ui->drag_elbow_hf;
            int mid = ui->drag_elbow_mid;
            if (jumper_world_ends(ui, &board->jumpers[ui->drag_jumper], &ax, &ay, &bx, &by)) {
                jumper_route_from_mouse(ax, ay, bx, by, ui->drag_jumper_elbow, board_mx, board_my, &hf, &mid);
                ui->drag_elbow_hf = hf;
                ui->drag_elbow_mid = mid;
            }
            return 1;
        }
        {
            NsBreadboard *bb;
            NsPbHole hole;
            ui->drag_jumper_preview_ok = 0;
            ui->drag_jumper_preview_bb = NULL;
            bb = hit_breadboard(ui, board_mx, board_my, &hole);
            if (bb) {
                ui->drag_jumper_preview = hole;
                ui->drag_jumper_preview_bb = bb;
                ui->drag_jumper_preview_ok = 1;
            }
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_leg_chip >= 0) {
        NsEntity *e_leg = ui->chips[ui->drag_leg_chip];
        if (e_leg && e_leg->visual == NS_ENTITY_VIS_PASSIVE) {
            stretch_resistor_leg(ui, (NsPassive *)e_leg, ui->drag_leg_pin, board_mx, board_my);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        if (sel_count(ui) > 1) {
            move_selection_drag(ui, board, board_mx, board_my);
            snap_selection_to_breadboard(ui);
        } else {
            move_chip_drag(ui, board, ui->drag_chip, board_mx, board_my);
            snap_chip_to_breadboard(ui, ui->drag_chip);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (ui->box_sel) {
            int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
            int w = ui->box_bx1 - ui->box_bx0;
            int h = ui->box_by1 - ui->box_by0;
            if (w < 0) {
                w = -w;
            }
            if (h < 0) {
                h = -h;
            }
            ui->box_sel = 0;
            if (w >= 4 || h >= 4) {
                sel_from_box(ui, board, shift);
            } else if (!shift) {
                sel_clear(ui);
            }
            return 1;
        }
        if (ui->drag_jumper >= 0) {
            if (ui->drag_jumper_elbow >= 0) {
                (void)r01a_board_jumper_set_route(board, ui->drag_jumper, ui->drag_elbow_hf,
                                                  ui->drag_elbow_mid);
            } else if (ui->drag_jumper_preview_ok && ui->drag_jumper_preview_bb) {
                int ji = ui->drag_jumper;
                (void)r01a_board_jumper_set_end_on(board, ui->drag_jumper, ui->drag_jumper_end,
                                                   ui->drag_jumper_preview_bb, ui->drag_jumper_preview);
                jumper_apply_auto_color(ui, board, ji);
            }
            ui->drag_jumper = -1;
            ui->drag_jumper_preview_ok = 0;
            ui->drag_jumper_preview_bb = NULL;
            ui->drag_jumper_elbow = -1;
            return 1;
        }
        if (ui->drag_leg_chip >= 0) {
            NsEntity *e_leg = ui->chips[ui->drag_leg_chip];
            if (e_leg && e_leg->visual == NS_ENTITY_VIS_PASSIVE) {
                stretch_resistor_leg(ui, (NsPassive *)e_leg, ui->drag_leg_pin, board_mx, board_my);
            }
            undo_push_move(ui, board);
            ui->drag_leg_chip = -1;
            return 1;
        }
        if (ui->drag_chip >= 0) {
            snap_selection_to_breadboard(ui);
            apply_bb_followers(ui, board);
            undo_push_move(ui, board);
        }
        ui->drag_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int chip_i;
        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
        SDL_Rect btn;
        NsBreadboard *bb;
        NsPbHole hole;

        if (ui->ctx_open) {
            int item = ctx_hit_item(ui, lx, ly);
            ui->ctx_open = 0;
            if (item >= 0) {
                ctx_apply(ui, board, item);
            }
            return 1;
        }

        wire_mode_btn_rect(r01a_board_wire_mode(board), &btn);
        if (point_in_rect(lx, ly, &btn)) {
            int next = (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? R01A_WIRE_AUTO : R01A_WIRE_MANUAL;
            r01a_board_set_wire_mode(board, next);
            ui->jumper_arm = 0;
            return 1;
        }

        if (e->button.clicks == 2) {
            chip_i = hit_top_chip(ui, lx, ly);
            if (chip_i >= 0 && ui->chips[chip_i] && ui->chips[chip_i]->visual == NS_ENTITY_VIS_DISPLAY) {
                NsVideoSink *sink = (NsVideoSink *)ui->chips[chip_i];
                ns_video_sink_set_scale_2x(sink, !ns_video_sink_scale_2x(sink));
                return 1;
            }
        }

        ui->drag_chip = -1;
        ui->drag_leg_chip = -1;
        ui->drag_jumper = -1;
        ui->drag_jumper_elbow = -1;
        chip_i = hit_top_chip(ui, lx, ly);
        bb = hit_breadboard(ui, board_mx, board_my, &hole);
        if (SDL_GetModState() & KMOD_CTRL) {
            int leg_c = -1;
            int leg_p = 0;
            if (hit_resistor_leg(ui, board_mx, board_my, &leg_c, &leg_p)) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                sel_set_one(ui, leg_c);
                ui->drag_leg_chip = leg_c;
                ui->drag_leg_pin = leg_p;
                undo_capture_all(ui, ui->drag_pose, &ui->drag_pose_n);
                stretch_resistor_leg(ui, (NsPassive *)ui->chips[leg_c], leg_p, board_mx, board_my);
                return 1;
            }
        }
        if (ui->jumper_mode && ui->jumper_arm && bb) {
            if (ui->jumper_bb == bb && ui->jumper_from.col == hole.col &&
                ui->jumper_from.lane == hole.lane) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            } else if (ui->jumper_bb) {
                uint8_t cr;
                uint8_t cg;
                uint8_t cb;
                jumper_color_for_holes(ui, board, ui->jumper_bb, ui->jumper_from, bb, hole, &cr, &cg, &cb);
                {
                    int n0 = board->jumper_count;
                    if (r01a_board_jumper_add_across(board, ui->jumper_bb, ui->jumper_from, bb, hole, cr, cg,
                                                    cb) &&
                        board->jumper_count > n0) {
                        undo_push_jmp_add(ui, &board->jumpers[board->jumper_count - 1]);
                    }
                }
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            }
            return 1;
        }
        {
            int ji = -1;
            int end_i = 0;
            if (hit_jumper_end(ui, board, board_mx, board_my, &ji, &end_i)) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                if (shift) {
                    int on = ui->jumper_sel[ji] ? 0 : 1;
                    ui->jumper_sel[ji] = (uint8_t)on;
                    if (on) {
                        jumper_z_raise(ui, board, ji);
                    }
                } else {
                    jumper_sel_set_one(ui, board, ji);
                }
                ui->drag_jumper = ji;
                ui->drag_jumper_end = end_i;
                ui->drag_jumper_elbow = -1;
                ui->drag_jumper_preview_ok = 0;
                return 1;
            }
            if (hit_jumper_elbow(ui, board, board_mx, board_my, &ji, &end_i)) {
                int ax;
                int ay;
                int bx;
                int by;
                int hf = 0;
                int mid = 0;
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                if (shift) {
                    int on = ui->jumper_sel[ji] ? 0 : 1;
                    ui->jumper_sel[ji] = (uint8_t)on;
                    if (on) {
                        jumper_z_raise(ui, board, ji);
                    }
                } else {
                    jumper_sel_set_one(ui, board, ji);
                }
                ui->drag_jumper = ji;
                ui->drag_jumper_elbow = end_i;
                ui->drag_jumper_preview_ok = 0;
                if (jumper_world_ends(ui, &board->jumpers[ji], &ax, &ay, &bx, &by)) {
                    jumper_effective_route(ui, &board->jumpers[ji], ax, ay, bx, by, &hf, &mid);
                    ui->drag_elbow_hf = hf;
                    ui->drag_elbow_mid = mid;
                }
                return 1;
            }
            ji = hit_jumper_body(ui, board, board_mx, board_my);
            if (ji >= 0) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                if (shift) {
                    int on = ui->jumper_sel[ji] ? 0 : 1;
                    ui->jumper_sel[ji] = (uint8_t)on;
                    if (on) {
                        jumper_z_raise(ui, board, ji);
                    }
                } else {
                    jumper_sel_set_one(ui, board, ji);
                }
                return 1;
            }
        }
        if (ui->jumper_mode && bb) {
            ui->jumper_arm = 1;
            ui->jumper_from = hole;
            ui->jumper_bb = bb;
            return 1;
        }
        if (ui->jumper_arm) {
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
        }
        if (chip_i >= 0) {
            chip_z_raise(ui, chip_i);
            if (shift) {
                sel_toggle(ui, chip_i);
                return 1;
            }
            if (ui->chip_sel[chip_i] && sel_count(ui) > 1) {
                ui->selected = chip_i;
                ui->drag_chip = chip_i;
                ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
                ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
                begin_sel_drag(ui, board, board_mx, board_my);
                return 1;
            }
            sel_set_one(ui, chip_i);
            ui->drag_chip = chip_i;
            ui->drag_grab_bx = board_mx - ui->chips[chip_i]->board_x;
            ui->drag_grab_by = board_my - ui->chips[chip_i]->board_y;
            begin_sel_drag(ui, board, board_mx, board_my);
            return 1;
        }
        if (!shift) {
            sel_clear(ui);
        }
        ui->box_sel = 1;
        ui->box_bx0 = ui->box_bx1 = board_mx;
        ui->box_by0 = ui->box_by1 = board_my;
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        int step = 48 / canvas_zoom(ui);
        if (step < 1) {
            step = 1;
        }
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            if (ui->ctx_open) {
                ui->ctx_open = 0;
                return 1;
            }
            if (ui->jumper_arm) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
                return 1;
            }
            if (ui->jumper_mode) {
                ui->jumper_mode = 0;
                return 1;
            }
            return 2;
        }
        if (e->key.keysym.sym == SDLK_j) {
            ui->jumper_mode = !ui->jumper_mode;
            if (!ui->jumper_mode) {
                ui->jumper_arm = 0;
                ui->jumper_bb = NULL;
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_DELETE || e->key.keysym.sym == SDLK_BACKSPACE) {
            jumper_delete_selected(ui, board);
            breadboard_delete_selected(ui, board);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_SPACE && !e->key.repeat) {
            ui->air_always = !ui->air_always;
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_RETURN || e->key.keysym.sym == SDLK_KP_ENTER) && !e->key.repeat) {
            board->running = !board->running;
            if (r01a_board_group(board)) {
                r01a_board_group(board)->running = board->running;
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_PERIOD) {
            r01a_board_step(board);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_a) {
            int next = (r01a_board_wire_mode(board) == R01A_WIRE_MANUAL) ? R01A_WIRE_AUTO : R01A_WIRE_MANUAL;
            r01a_board_set_wire_mode(board, next);
            ui->jumper_arm = 0;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_x) {
            if (board->jumper_count > 0) {
                undo_push_jmp_del(ui, board->jumpers, board->jumper_count);
            }
            r01a_board_jumper_clear(board);
            ui->jumper_arm = 0;
            ui->jumper_bb = NULL;
            memset(ui->jumper_sel, 0, sizeof(ui->jumper_sel));
            ui->jumper_z_n = 0;
            ui->hover_jumper = -1;
            ui->drag_jumper = -1;
            ui->drag_jumper_elbow = -1;
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_z || e->key.keysym.sym == SDLK_y) && (e->key.keysym.mod & KMOD_CTRL) &&
            !e->key.repeat) {
            if (e->key.keysym.sym == SDLK_y || (e->key.keysym.mod & KMOD_SHIFT)) {
                redo_do(ui, board);
            } else {
                undo_do(ui, board);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r && (e->key.keysym.mod & KMOD_CTRL)) {
            r01a_board_reset(board);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_f && (e->key.keysym.mod & KMOD_CTRL) && !e->key.repeat) {
            Uint32 flags = SDL_GetWindowFlags(ui->win);
            if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) {
                SDL_SetWindowFullscreen(ui->win, 0);
            } else {
                SDL_SetWindowFullscreen(ui->win, SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r) {
            rotate_selected(ui);
            return 1;
        }
        if (e->key.keysym.mod & KMOD_SHIFT) {
            if (e->key.keysym.sym == SDLK_LEFT) {
                ui->pan_x -= step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_RIGHT) {
                ui->pan_x += step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_UP) {
                ui->pan_y -= step;
                clamp_pan(ui);
                return 1;
            }
            if (e->key.keysym.sym == SDLK_DOWN) {
                ui->pan_y += step;
                clamp_pan(ui);
                return 1;
            }
        }
    }
    return 0;
}

int r01a_ui_run(R01aBoard *board) {
    R01aUi ui;
    int quit = 0;

    memset(&ui, 0, sizeof(ui));
    ui.undo = (R01aUndoCmd *)calloc((size_t)R01A_UNDO_MAX, sizeof(R01aUndoCmd));
    if (!ui.undo) {
        fprintf(stderr, "undo alloc failed\n");
        return 1;
    }
    ui.selected = -1;
    ui.drag_chip = -1;
    ui.drag_leg_chip = -1;
    ui.jumper_arm = 0;
    ui.jumper_mode = 0;
    ui.jumper_color_i = 0;
    ui.hover_jumper = -1;
    ui.drag_jumper = -1;
    ui.drag_jumper_elbow = -1;
    ui.hover_chip = -1;
    ui.hover_pin = -1;
    ui.air_always = 0;
    ui.zoom = 1;
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        free(ui.undo);
        return 1;
    }
    (void)r01a_font_init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    ui.win = SDL_CreateWindow("Retr01 Tier B", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              NS_LOGIC_W * R01A_UI_SCALE, NS_LOGIC_H * R01A_UI_SCALE, SDL_WINDOW_RESIZABLE);
    if (!ui.win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        r01a_font_shutdown();
        SDL_Quit();
        free(ui.undo);
        return 1;
    }
    ui.rend = SDL_CreateRenderer(ui.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ui.rend) {
        ui.rend = SDL_CreateRenderer(ui.win, -1, 0);
    }
    if (!ui.rend) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        free(ui.undo);
        return 1;
    }
    ui.target = SDL_CreateTexture(ui.rend, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, NS_LOGIC_W,
                                  NS_LOGIC_H);
    if (!ui.target) {
        fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ui.rend);
        SDL_DestroyWindow(ui.win);
        r01a_font_shutdown();
        SDL_Quit();
        free(ui.undo);
        return 1;
    }
    SDL_SetTextureScaleMode(ui.target, SDL_ScaleModeNearest);
    ui.scale = R01A_UI_SCALE;
    tip_reset(&ui, 0, 0);
    if (r01a_layout_load(R01A_LAYOUT_FILE, board, &ui.pan_x, &ui.pan_y, &ui.zoom, &ui.air_always) != 0) {
        /* First launch: keep the Tier A breadboards, part positions, and jumpers. */
        (void)r01a_layout_load(R01A_TIER_A_LAYOUT, board, &ui.pan_x, &ui.pan_y, &ui.zoom, &ui.air_always);
    }
    if (ui.zoom < 1) {
        ui.zoom = 1;
    }
    if (ui.zoom > R01A_ZOOM_MAX) {
        ui.zoom = R01A_ZOOM_MAX;
    }
    ui.air_always = ui.air_always ? 1 : 0;
    bind_chips(&ui, board);
    pin_net_build(&ui);
    clamp_pan(&ui);

    while (!quit) {
        SDL_Event ev;
        update_scale(&ui);
        while (SDL_PollEvent(&ev)) {
            int lx = 0;
            int ly = 0;
            int rc;
            if (ev.type == SDL_QUIT) {
                quit = 1;
                break;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN || ev.type == SDL_MOUSEBUTTONUP) {
                logic_from_window(&ui, ev.button.x, ev.button.y, &lx, &ly);
            } else if (ev.type == SDL_MOUSEMOTION) {
                logic_from_window(&ui, ev.motion.x, ev.motion.y, &lx, &ly);
            } else {
                int mx = 0;
                int my = 0;
                SDL_GetMouseState(&mx, &my);
                logic_from_window(&ui, mx, my, &lx, &ly);
            }
            rc = handle_event(&ui, board, &ev, lx, ly);
            if (rc == 2) {
                quit = 1;
            }
        }
        if (board->running) {
            sim_frame(board);
        }
        draw_frame(&ui, board);
    }

    (void)r01a_layout_save(R01A_LAYOUT_FILE, board, ui.pan_x, ui.pan_y, canvas_zoom(&ui), ui.air_always ? 1 : 0);

    if (ui.lcd_tex) {
        SDL_DestroyTexture(ui.lcd_tex);
    }
    if (ui.target) {
        SDL_DestroyTexture(ui.target);
    }
    SDL_DestroyRenderer(ui.rend);
    SDL_DestroyWindow(ui.win);
    r01a_font_shutdown();
    SDL_Quit();
    free(ui.undo);
    return 0;
}

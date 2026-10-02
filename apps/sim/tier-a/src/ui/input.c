#include "r01a_ui.h"

#include <SDL.h>

static void begin_chip_drag(R01aUi *ui, int ci, int bx, int by) {
    ui->selected = ci;
    ui->drag_chip = ci;
    ui->drag_grab_bx = bx - ui->chips[ci]->board_x;
    ui->drag_grab_by = by - ui->chips[ci]->board_y;
    ui->drag_from_x = ui->chips[ci]->board_x;
    ui->drag_from_y = ui->chips[ci]->board_y;
}

void rotate_selected(R01aUi *ui) {
    NsEntity *e;
    if (ui->selected < 0 || ui->selected >= ui->chip_count) {
        return;
    }
    e = ui->chips[ui->selected];
    if (!e) {
        return;
    }
    if (is_passive_glyph(e) || e->visual == NS_ENTITY_VIS_DISPLAY) {
        return;
    }
    ns_entity_set_orient(e, ns_orient_next_cw(e->orient));
    hist_after(ui);
}

int handle_event(R01aUi *ui, R01aBoard *board, const SDL_Event *e, int lx, int ly) {
    int bx = 0;
    int by = 0;
    (void)board;
    ui->mouse_lx = lx;
    ui->mouse_ly = ly;
    logic_to_board(ui, lx, ly, &bx, &by);
    if (!hit_pin_at(ui, bx, by, &ui->hover_chip, &ui->hover_pin)) {
        ui->hover_pin = -1;
        ui->hover_chip = hit_top_chip(ui, bx, by);
    }

    if (e->type == SDL_MOUSEWHEEL) {
        int dy = e->wheel.y;
        if (e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
            dy = -dy;
        }
        if (SDL_GetModState() & KMOD_CTRL) {
            if (dy > 0) {
                canvas_zoom_by(ui, 1, lx, ly);
            } else if (dy < 0) {
                canvas_zoom_by(ui, -1, lx, ly);
            }
            return 1;
        }
        ui->pan_x -= e->wheel.x * 24;
        ui->pan_y -= dy * 24;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 1;
        ui->drag_grab_bx = bx;
        ui->drag_grab_by = by;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_MIDDLE) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_pan) {
        pan_lock_point(ui, ui->drag_grab_bx, ui->drag_grab_by, lx, ly);
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_RIGHT) {
        if (ui->arm) {
            arm_cancel(ui);
        }
        ui->drag_pan = 1;
        ui->drag_grab_bx = bx;
        ui->drag_grab_by = by;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_RIGHT) {
        ui->drag_pan = 0;
        return 1;
    }
    if (e->type == SDL_MOUSEMOTION && ui->drag_chip >= 0) {
        NsEntity *e_drag = ui->chips[ui->drag_chip];
        if (e_drag) {
            move_entity(e_drag, bx - ui->drag_grab_bx, by - ui->drag_grab_by);
        }
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        if (ui->drag_chip >= 0 && ui->drag_chip < ui->chip_count && ui->chips[ui->drag_chip] &&
            (ui->chips[ui->drag_chip]->board_x != ui->drag_from_x ||
             ui->chips[ui->drag_chip]->board_y != ui->drag_from_y)) {
            hist_after(ui);
        }
        ui->drag_chip = -1;
        return 1;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        int ci = -1;
        int pi = -1;
        SDL_Rect btn;
        mode_btn_rect(ui, &btn);
        if (lx >= btn.x && ly >= btn.y && lx < btn.x + btn.w && ly < btn.y + btn.h) {
            ui->show_nets = !ui->show_nets;
            if (!ui->show_nets) {
                arm_cancel(ui);
            }
            return 1;
        }
        if (e->button.clicks == 2) {
            ci = hit_top_chip(ui, bx, by);
            if (ci >= 0 && ui->chips[ci] && ui->chips[ci]->visual == NS_ENTITY_VIS_DISPLAY) {
                NsVideoSink *sink = (NsVideoSink *)ui->chips[ci];
                ns_video_sink_set_scale_2x(sink, !ns_video_sink_scale_2x(sink));
                ui->selected = ci;
                hist_after(ui);
                return 1;
            }
        }
        if (!ui->show_nets) {
            ci = hit_top_chip(ui, bx, by);
            if (ci >= 0) {
                begin_chip_drag(ui, ci, bx, by);
            } else {
                ui->selected = -1;
            }
            return 1;
        }
        if (hit_pin_at(ui, bx, by, &ci, &pi)) {
            int cx;
            int cy;
            pin_center(ui->chips[ci], pi, &cx, &cy);
            if (ui->arm) {
                arm_add_to_pad(ui, cx, cy);
                arm_commit(ui);
            } else {
                arm_begin(ui, ci, pi);
            }
            ui->selected = ci;
            return 1;
        }
        if (ui->arm) {
            arm_add_point(ui, bx, by);
            return 1;
        }
        ci = hit_top_chip(ui, bx, by);
        if (ci >= 0) {
            begin_chip_drag(ui, ci, bx, by);
            return 1;
        }
        ui->selected = -1;
        return 1;
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            if (ui->arm) {
                arm_cancel(ui);
                return 1;
            }
            return 2;
        }
        if (e->key.keysym.sym == SDLK_BACKSPACE && ui->arm && ui->arm_n > 1) {
            ui->arm_n--;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_RETURN && ui->arm) {
            arm_commit(ui);
            return 1;
        }
        if ((e->key.keysym.sym == SDLK_z) && (SDL_GetModState() & KMOD_CTRL) && !e->key.repeat) {
            if (SDL_GetModState() & KMOD_SHIFT) {
                hist_redo(ui);
            } else {
                hist_undo(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_y && (SDL_GetModState() & KMOD_CTRL) && !e->key.repeat) {
            hist_redo(ui);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_a && !e->key.repeat) {
            ui->show_nets = !ui->show_nets;
            if (!ui->show_nets) {
                arm_cancel(ui);
            }
            return 1;
        }
        if (e->key.keysym.sym == SDLK_l && !e->key.repeat) {
            ui->air_hard = !ui->air_hard;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_r) {
            rotate_selected(ui);
            return 1;
        }
        if (e->key.keysym.sym == SDLK_SPACE && !e->key.repeat) {
            board->running = !board->running;
            return 1;
        }
        if (e->key.keysym.sym == SDLK_LEFT) {
            ui->pan_x -= 24;
        }
        if (e->key.keysym.sym == SDLK_RIGHT) {
            ui->pan_x += 24;
        }
        if (e->key.keysym.sym == SDLK_UP) {
            ui->pan_y -= 24;
        }
        if (e->key.keysym.sym == SDLK_DOWN) {
            ui->pan_y += 24;
        }
        return 1;
    }
    return 0;
}

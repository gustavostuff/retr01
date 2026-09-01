#include "ui/widgets/widgets.h"
#include "ui/internal.h"
#include "font/font.h"

#include <string.h>

#define UI_TEXT_PAD 2
#define UI_TEXT_CARET_MS 530

static int text_len(const UiTextEdit *t) {
    return t && t->buf ? (int)strlen(t->buf) : 0;
}

static void text_clamp(UiTextEdit *t) {
    int len = text_len(t);
    if (t->caret < 0) {
        t->caret = 0;
    }
    if (t->caret > len) {
        t->caret = len;
    }
    if (t->anchor < 0) {
        t->anchor = 0;
    }
    if (t->anchor > len) {
        t->anchor = len;
    }
}

static int sel_lo(const UiTextEdit *t) {
    return t->caret < t->anchor ? t->caret : t->anchor;
}

static int sel_hi(const UiTextEdit *t) {
    return t->caret > t->anchor ? t->caret : t->anchor;
}

static int has_sel(const UiTextEdit *t) {
    return t->caret != t->anchor;
}

static void ensure_caret_visible(UiTextEdit *t, int view_w) {
    int cx = font_text_width_n(t->buf, t->caret);
    int inner = view_w - UI_TEXT_PAD * 2;
    if (inner < 4) {
        inner = 4;
    }
    if (cx < t->scroll) {
        t->scroll = cx;
    }
    if (cx - t->scroll > inner - 1) {
        t->scroll = cx - (inner - 1);
    }
    if (t->scroll < 0) {
        t->scroll = 0;
    }
}

static int caret_from_x(const UiTextEdit *t, int local_x) {
    int target = local_x - UI_TEXT_PAD + t->scroll;
    int i, w = 0;
    int len = text_len(t);
    if (target <= 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        char ch[2] = {t->buf[i], '\0'};
        int cw = font_text_width(ch);
        if (w + cw / 2 >= target) {
            return i;
        }
        w += cw;
    }
    return len;
}

void ui_text_blur(UiState *ui) {
    if (!ui) {
        return;
    }
    ui->text.buf = NULL;
    ui->text.cap = 0;
    ui->text.field_id = 0;
    ui->text.caret = 0;
    ui->text.anchor = 0;
    ui->text.scroll = 0;
    ui->text.drag = 0;
    SDL_StopTextInput();
}

void ui_text_focus(UiState *ui, char *buf, int cap, int field_id) {
    int len;
    if (!ui || !buf || cap < 2 || field_id < 1) {
        return;
    }
    ui->text.buf = buf;
    ui->text.cap = cap;
    ui->text.field_id = field_id;
    len = (int)strlen(buf);
    ui->text.caret = len;
    ui->text.anchor = len;
    ui->text.scroll = 0;
    ui->text.drag = 0;
    SDL_StartTextInput();
}

int ui_text_active(const UiState *ui, int field_id) {
    return ui && ui->text.field_id == field_id && ui->text.buf != NULL;
}

static void delete_sel(UiTextEdit *t) {
    int lo, hi, len, n;
    if (!has_sel(t)) {
        return;
    }
    lo = sel_lo(t);
    hi = sel_hi(t);
    len = text_len(t);
    n = len - hi;
    memmove(t->buf + lo, t->buf + hi, (size_t)n + 1);
    t->caret = lo;
    t->anchor = lo;
}

static void insert_chars(UiTextEdit *t, const char *src) {
    int len, room, i;
    if (!t || !t->buf || !src) {
        return;
    }
    delete_sel(t);
    len = text_len(t);
    room = t->cap - 1 - len;
    for (i = 0; src[i] && room > 0; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 32 || c > 126) {
            continue;
        }
        memmove(t->buf + t->caret + 1, t->buf + t->caret, (size_t)(len - t->caret) + 1);
        t->buf[t->caret] = (char)c;
        t->caret++;
        t->anchor = t->caret;
        len++;
        room--;
    }
}

void ui_text_draw(UiState *ui, SDL_Renderer *r, int x, int y, int w, const char *text, int field_id) {
    const char *show = text ? text : "";
    int focused = ui_text_active(ui, field_id);
    SDL_Rect clip;
    int ty = y + (UI_BTN_H - font_line_h()) / 2;
    if (ty < y) {
        ty = y;
    }

    fill_rect(r, x, y, w, UI_BTN_H, 240, 240, 240);
    if (focused) {
        draw_rect(r, x, y, w, UI_BTN_H, 45, 125, 70);
        ensure_caret_visible(&ui->text, w);
    } else {
        draw_rect(r, x, y, w, UI_BTN_H, UI_COL_WELL_R, UI_COL_WELL_G, UI_COL_WELL_B);
    }

    clip.x = x + UI_TEXT_PAD;
    clip.y = y;
    clip.w = w - UI_TEXT_PAD * 2;
    clip.h = UI_BTN_H;
    if (clip.w < 1) {
        return;
    }
    SDL_RenderSetClipRect(r, &clip);

    if (focused) {
        UiTextEdit *t = &ui->text;
        int pen = x + UI_TEXT_PAD - t->scroll;
        if (has_sel(t)) {
            int x0 = pen + font_text_width_n(t->buf, sel_lo(t));
            int x1 = pen + font_text_width_n(t->buf, sel_hi(t));
            fill_rect(r, x0, y + 2, x1 - x0, UI_BTN_H - 4, 160, 190, 230);
        }
        font_draw(r, pen, ty, t->buf, 20, 20, 20);
        if (((SDL_GetTicks() / UI_TEXT_CARET_MS) & 1) == 0) {
            int cx = pen + font_text_width_n(t->buf, t->caret);
            fill_rect(r, cx, y + 2, 1, UI_BTN_H - 4, 20, 20, 20);
        }
    } else {
        font_draw(r, x + UI_TEXT_PAD, ty, show, 20, 20, 20);
    }

    SDL_RenderSetClipRect(r, NULL);
}

int ui_text_mouse_down(UiState *ui, int lx, int ly, int x, int y, int w, char *buf, int cap, int field_id) {
    int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
    int idx;
    if (!ui || !buf || !point_in_rect(lx, ly, x, y, w, UI_BTN_H)) {
        return 0;
    }
    if (!ui_text_active(ui, field_id) || ui->text.buf != buf) {
        ui_text_focus(ui, buf, cap, field_id);
    }
    idx = caret_from_x(&ui->text, lx - x);
    ui->text.caret = idx;
    if (!shift) {
        ui->text.anchor = idx;
    }
    ui->text.drag = 1;
    ensure_caret_visible(&ui->text, w);
    return 1;
}

void ui_text_mouse_up(UiState *ui) {
    if (ui) {
        ui->text.drag = 0;
    }
}

void ui_text_mouse_drag(UiState *ui, int lx, int x, int w) {
    if (!ui || !ui->text.drag || !ui->text.buf) {
        return;
    }
    ui->text.caret = caret_from_x(&ui->text, lx - x);
    text_clamp(&ui->text);
    ensure_caret_visible(&ui->text, w);
}

int ui_text_key(UiState *ui, SDL_Keycode sym, Uint16 mod) {
    UiTextEdit *t;
    int ctrl = (mod & KMOD_CTRL) != 0;
    int shift = (mod & KMOD_SHIFT) != 0;
    int len;
    if (!ui || !ui->text.buf || ui->text.field_id < 1) {
        return 0;
    }
    t = &ui->text;
    text_clamp(t);
    len = text_len(t);

    if (sym == SDLK_ESCAPE) {
        ui_text_blur(ui);
        return 1;
    }
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
        ui_text_blur(ui);
        return 1;
    }
    if (ctrl && sym == SDLK_a) {
        t->anchor = 0;
        t->caret = len;
        return 1;
    }
    if (sym == SDLK_LEFT) {
        if (ctrl) {
            /* word-ish: skip non-space then spaces */
            int i = t->caret;
            while (i > 0 && t->buf[i - 1] == ' ') {
                i--;
            }
            while (i > 0 && t->buf[i - 1] != ' ') {
                i--;
            }
            t->caret = i;
        } else if (!shift && has_sel(t)) {
            t->caret = sel_lo(t);
        } else if (t->caret > 0) {
            t->caret--;
        }
        if (!shift) {
            t->anchor = t->caret;
        }
        return 1;
    }
    if (sym == SDLK_RIGHT) {
        if (ctrl) {
            int i = t->caret;
            while (i < len && t->buf[i] != ' ') {
                i++;
            }
            while (i < len && t->buf[i] == ' ') {
                i++;
            }
            t->caret = i;
        } else if (!shift && has_sel(t)) {
            t->caret = sel_hi(t);
        } else if (t->caret < len) {
            t->caret++;
        }
        if (!shift) {
            t->anchor = t->caret;
        }
        return 1;
    }
    if (sym == SDLK_HOME) {
        t->caret = 0;
        if (!shift) {
            t->anchor = 0;
        }
        return 1;
    }
    if (sym == SDLK_END) {
        t->caret = len;
        if (!shift) {
            t->anchor = len;
        }
        return 1;
    }
    if (sym == SDLK_BACKSPACE) {
        if (has_sel(t)) {
            delete_sel(t);
        } else if (t->caret > 0) {
            memmove(t->buf + t->caret - 1, t->buf + t->caret, (size_t)(len - t->caret) + 1);
            t->caret--;
            t->anchor = t->caret;
        }
        return 1;
    }
    if (sym == SDLK_DELETE) {
        if (has_sel(t)) {
            delete_sel(t);
        } else if (t->caret < len) {
            memmove(t->buf + t->caret, t->buf + t->caret + 1, (size_t)(len - t->caret));
            t->anchor = t->caret;
        }
        return 1;
    }
    /* Printable keys also arrive as SDL_TEXTINPUT; ignore here when not ctrl. */
    return 1;
}

int ui_text_input(UiState *ui, const char *utf8) {
    if (!ui || !ui->text.buf || ui->text.field_id < 1 || !utf8) {
        return 0;
    }
    insert_chars(&ui->text, utf8);
    return 1;
}

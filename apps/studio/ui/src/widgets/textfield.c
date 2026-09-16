#include "retr01_ui/widgets.h"
#include "retr01_ui/font.h"

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

void ui_text_blur(UiTextEdit *t) {
    if (!t) {
        return;
    }
    t->buf = NULL;
    t->cap = 0;
    t->field_id = 0;
    t->caret = 0;
    t->anchor = 0;
    t->scroll = 0;
    t->drag = 0;
    SDL_StopTextInput();
}

void ui_text_focus(UiTextEdit *t, char *buf, int cap, int field_id) {
    int len;
    if (!t || !buf || cap < 2 || field_id < 1) {
        return;
    }
    t->buf = buf;
    t->cap = cap;
    t->field_id = field_id;
    len = (int)strlen(buf);
    t->caret = len;
    t->anchor = len;
    t->scroll = 0;
    t->drag = 0;
    SDL_StartTextInput();
}

int ui_text_active(const UiTextEdit *t, int field_id) {
    return t && t->field_id == field_id && t->buf != NULL;
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

void ui_text_draw(UiTextEdit *t, SDL_Renderer *r, int x, int y, int w, const char *text, int field_id) {
    const char *show = text ? text : "";
    int focused = ui_text_active(t, field_id);
    SDL_Rect clip;
    int ty = y + (UI_BTN_H - font_line_h()) / 2;
    if (ty < y) {
        ty = y;
    }

    fill_rect(r, x, y, w, UI_BTN_H, 240, 240, 240);
    if (focused) {
        ensure_caret_visible(t, w);
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

int ui_text_mouse_down(UiTextEdit *t, int lx, int ly, int x, int y, int w, char *buf, int cap, int field_id) {
    int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
    int idx;
    if (!t || !buf || !point_in_rect(lx, ly, x, y, w, UI_BTN_H)) {
        return 0;
    }
    if (!ui_text_active(t, field_id) || t->buf != buf) {
        ui_text_focus(t, buf, cap, field_id);
    }
    idx = caret_from_x(t, lx - x);
    t->caret = idx;
    if (!shift) {
        t->anchor = idx;
    }
    t->drag = 1;
    ensure_caret_visible(t, w);
    return 1;
}

void ui_text_mouse_up(UiTextEdit *t) {
    if (t) {
        t->drag = 0;
    }
}

void ui_text_mouse_drag(UiTextEdit *t, int lx, int x, int w) {
    if (!t || !t->drag || !t->buf) {
        return;
    }
    t->caret = caret_from_x(t, lx - x);
    text_clamp(t);
    ensure_caret_visible(t, w);
}

int ui_text_key(UiTextEdit *t, SDL_Keycode sym, Uint16 mod) {
    int ctrl = (mod & KMOD_CTRL) != 0;
    int shift = (mod & KMOD_SHIFT) != 0;
    int len;
    if (!t || !t->buf || t->field_id < 1) {
        return 0;
    }
    text_clamp(t);
    len = text_len(t);

    if (sym == SDLK_ESCAPE) {
        ui_text_blur(t);
        return 1;
    }
    if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
        ui_text_blur(t);
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

int ui_text_input(UiTextEdit *t, const char *utf8) {
    if (!t || !t->buf || t->field_id < 1 || !utf8) {
        return 0;
    }
    insert_chars(t, utf8);
    return 1;
}

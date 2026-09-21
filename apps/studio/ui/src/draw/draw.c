#include "retr01_ui/draw.h"
#include "retr01_ui/font.h"

#include <string.h>

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rc);
}

void fill_rect_alpha(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    SDL_RenderFillRect(r, &rc);
}

static int round_row_inset(int row_from_edge, int rad) {
    int y2, r2, x;
    if (row_from_edge < 0 || row_from_edge >= rad) {
        return 0;
    }
    y2 = 2 * rad - 1 - 2 * row_from_edge;
    r2 = 4 * rad * rad;
    for (x = 0; x < rad; x++) {
        int x2 = 2 * x + 1 - 2 * rad;
        if (x2 * x2 + y2 * y2 <= r2) {
            return x;
        }
    }
    return rad;
}

static void fill_round_rect_raw(SDL_Renderer *r, int x, int y, int w, int h, int rad) {
    int row;
    int max_r;
    if (!r || w < 1 || h < 1) {
        return;
    }
    max_r = w < h ? w / 2 : h / 2;
    if (rad > max_r) {
        rad = max_r;
    }
    if (rad < 1) {
        SDL_Rect rc = {x, y, w, h};
        SDL_RenderFillRect(r, &rc);
        return;
    }
    for (row = 0; row < h; row++) {
        int inset = 0;
        SDL_Rect rc;
        if (row < rad) {
            inset = round_row_inset(row, rad);
        } else if (row >= h - rad) {
            inset = round_row_inset(h - 1 - row, rad);
        }
        if (w - 2 * inset < 1) {
            continue;
        }
        rc.x = x + inset;
        rc.y = y + row;
        rc.w = w - 2 * inset;
        rc.h = 1;
        SDL_RenderFillRect(r, &rc);
    }
}

void fill_round_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, Uint8 R, Uint8 G, Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    fill_round_rect_raw(r, x, y, w, h, rad);
}

void fill_round_rect_alpha(SDL_Renderer *r, int x, int y, int w, int h, int rad, Uint8 R, Uint8 G, Uint8 B,
                           Uint8 A) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    fill_round_rect_raw(r, x, y, w, h, rad);
}

void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h) {
    hover_overlay_round(r, x, y, w, h, 0);
}

void hover_overlay_round(SDL_Renderer *r, int x, int y, int w, int h, int rad) {
    fill_round_rect_alpha(r, x, y, w, h, rad, 255, 255, 255, 77);
}

static SDL_Rect ui_clip_intersect(const SDL_Rect *a, const SDL_Rect *b) {
    SDL_Rect out;
    int x1 = a->x > b->x ? a->x : b->x;
    int y1 = a->y > b->y ? a->y : b->y;
    int x2 = a->x + a->w < b->x + b->w ? a->x + a->w : b->x + b->w;
    int y2 = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    out.x = x1;
    out.y = y1;
    out.w = x2 - x1;
    out.h = y2 - y1;
    if (out.w < 0) {
        out.w = 0;
    }
    if (out.h < 0) {
        out.h = 0;
    }
    return out;
}

void ui_clip_push(SDL_Renderer *r, int x, int y, int w, int h, UiClipStack *stack) {
    SDL_Rect next = {x, y, w, h};
    SDL_Rect prev;

    if (!r || !stack || w < 1 || h < 1) {
        return;
    }
    stack->had_clip = SDL_FALSE;
    SDL_RenderGetClipRect(r, &prev);
    if (prev.w > 0 && prev.h > 0) {
        stack->had_clip = SDL_TRUE;
        stack->prev = prev;
        next = ui_clip_intersect(&prev, &next);
    }
    SDL_RenderSetClipRect(r, &next);
}

void ui_clip_pop(SDL_Renderer *r, const UiClipStack *stack) {
    if (!r || !stack) {
        return;
    }
    if (stack->had_clip) {
        SDL_RenderSetClipRect(r, &stack->prev);
    } else {
        SDL_RenderSetClipRect(r, NULL);
    }
}

int point_in_rect(int lx, int ly, int x, int y, int w, int h) {
    return lx >= x && ly >= y && lx < x + w && ly < y + h;
}

int snap8(int v) {
    if (v < UI_UNIT) {
        return UI_UNIT;
    }
    return (v + UI_UNIT - 1) & ~(UI_UNIT - 1);
}

int label_width(const char *text) {
    return snap8(font_text_width(text) + UI_UNIT);
}

void draw_label(SDL_Renderer *r, int x, int y, const char *text) {
    int w = label_width(text);
    font_draw_centered(r, x, y, w, UI_BTN_H, text, 230, 230, 230);
}

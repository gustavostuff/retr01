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

void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderDrawRect(r, &rc);
}

void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h) {
    SDL_Rect rc = {x, y, w, h};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 77);
    SDL_RenderFillRect(r, &rc);
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

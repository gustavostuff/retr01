#ifndef RETR01_UI_DRAW_H
#define RETR01_UI_DRAW_H

#include "retr01_ui/metrics.h"

#include <SDL.h>

typedef struct UiClipStack {
    SDL_Rect prev;
    SDL_bool had_clip;
} UiClipStack;

void fill_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void fill_rect_alpha(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
void fill_round_rect(SDL_Renderer *r, int x, int y, int w, int h, int rad, Uint8 R, Uint8 G, Uint8 B);
void fill_round_rect_alpha(SDL_Renderer *r, int x, int y, int w, int h, int rad, Uint8 R, Uint8 G, Uint8 B,
                           Uint8 A);
void draw_rect(SDL_Renderer *r, int x, int y, int w, int h, Uint8 R, Uint8 G, Uint8 B);
void hover_overlay(SDL_Renderer *r, int x, int y, int w, int h);
void hover_overlay_round(SDL_Renderer *r, int x, int y, int w, int h, int rad);

void ui_clip_push(SDL_Renderer *r, int x, int y, int w, int h, UiClipStack *stack);
void ui_clip_pop(SDL_Renderer *r, const UiClipStack *stack);

int point_in_rect(int lx, int ly, int x, int y, int w, int h);
int snap8(int v);
int label_width(const char *text);
void draw_label(SDL_Renderer *r, int x, int y, const char *text);

#endif

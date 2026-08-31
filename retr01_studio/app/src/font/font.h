#ifndef retr01_STUDIO_FONT_H
#define retr01_STUDIO_FONT_H

#include <SDL.h>

/* Proggy Tiny via FreeType. Labels/buttons are 16px tall. */
#define R01_UI_FONT_PX 16

int font_init(void);
void font_shutdown(void);

int font_text_width(const char *text);
/* Width of the first n bytes (stops at NUL). n < 0 means full string. */
int font_text_width_n(const char *text, int n);
int font_line_h(void);

void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B);

/* Temporary pixel size (restores R01_UI_FONT_PX). alpha 0-255 multiplies glyph coverage. */
void font_draw_sized_alpha(SDL_Renderer *r, int x, int y, int px, const char *text, Uint8 R, Uint8 G, Uint8 B,
                           Uint8 alpha);

/* Draw text centered in a 16px-tall rect (width should already be snap-8). */
void font_draw_centered(SDL_Renderer *r, int x, int y, int w, int h, const char *text, Uint8 R, Uint8 G,
                        Uint8 B);

/* Word-wrap at max_w. Returns height used (line_h * lines). Soft-wrap on spaces; hard-break if needed. */
int font_measure_wrapped(const char *text, int max_w);
int font_draw_wrapped(SDL_Renderer *r, int x, int y, int max_w, const char *text, Uint8 R, Uint8 G, Uint8 B);

#endif

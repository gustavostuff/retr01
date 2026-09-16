#ifndef RETR01_UI_FONT_H
#define RETR01_UI_FONT_H

#include <SDL.h>

/* Proggy Tiny via FreeType. Labels/buttons are 16px tall. */
#define R01_UI_FONT_PX 16

/* Load bundled Proggy Tiny (R01_UI_FONT_DIR) or an explicit path. */
int font_init(void);
int font_init_path(const char *ttf_path);
void font_shutdown(void);

int font_text_width(const char *text);
int font_text_width_n(const char *text, int n);
int font_line_h(void);

void font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B);
void font_draw_sized_alpha(SDL_Renderer *r, int x, int y, int px, const char *text, Uint8 R, Uint8 G, Uint8 B,
                           Uint8 alpha);
void font_draw_centered(SDL_Renderer *r, int x, int y, int w, int h, const char *text, Uint8 R, Uint8 G,
                        Uint8 B);
int font_measure_wrapped(const char *text, int max_w);
int font_draw_wrapped(SDL_Renderer *r, int x, int y, int max_w, const char *text, Uint8 R, Uint8 G, Uint8 B);

#endif

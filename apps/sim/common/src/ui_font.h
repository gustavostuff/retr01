#ifndef R01A_UI_FONT_H
#define R01A_UI_FONT_H

#include <SDL.h>

#ifndef R01A_UI_FONT_PX
#define R01A_UI_FONT_PX 16
#endif

#define R01A_UI_LABEL_PAUSE_MS 700
#define R01A_UI_LABEL_SCROLL_PX_PER_SEC 28

int r01a_font_init(void);
void r01a_font_shutdown(void);
int r01a_font_line_h(void);
int r01a_font_text_width(const char *text);
void r01a_font_draw(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B);
void r01a_font_draw_a(SDL_Renderer *r, int x, int y, const char *text, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
void r01a_draw_label_bounce(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                            unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A);
void r01a_draw_label_bounce_rot90ccw(SDL_Renderer *r, int x, int y, int view_w, int view_h, const char *text,
                                     unsigned phase_seed, Uint8 R, Uint8 G, Uint8 B, Uint8 A);

#endif

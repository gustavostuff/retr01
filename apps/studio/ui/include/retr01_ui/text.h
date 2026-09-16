#ifndef RETR01_UI_TEXT_H
#define RETR01_UI_TEXT_H

#include <SDL.h>

/* Single active text field (caret / selection / scroll). */
typedef struct UiTextEdit {
    char *buf;
    int cap;
    int field_id; /* 0 = none; caller-defined otherwise */
    int caret;
    int anchor; /* selection other end; equals caret when collapsed */
    int scroll; /* horizontal px */
    int drag;   /* mouse-drag selecting */
} UiTextEdit;

void ui_text_blur(UiTextEdit *t);
void ui_text_focus(UiTextEdit *t, char *buf, int cap, int field_id);
int ui_text_active(const UiTextEdit *t, int field_id);
void ui_text_draw(UiTextEdit *t, SDL_Renderer *r, int x, int y, int w, const char *text, int field_id);
int ui_text_mouse_down(UiTextEdit *t, int lx, int ly, int x, int y, int w, char *buf, int cap, int field_id);
void ui_text_mouse_up(UiTextEdit *t);
void ui_text_mouse_drag(UiTextEdit *t, int lx, int x, int w);
int ui_text_key(UiTextEdit *t, SDL_Keycode sym, Uint16 mod);
int ui_text_input(UiTextEdit *t, const char *utf8);

#endif

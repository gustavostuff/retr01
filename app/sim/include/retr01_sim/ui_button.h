#ifndef retr01_SIM_UI_BUTTON_H
#define retr01_SIM_UI_BUTTON_H

#include "retr01_sim/entity.h"

typedef void (*R01sUiButtonFn)(void *ctx);

typedef struct R01sUiButton {
    R01sEntity base;
    char label[24];
    R01sUiButtonFn on_press;
    void *ctx;
    int pressed;
} R01sUiButton;

void r01s_ui_button_init(R01sUiButton *btn, const char *refdes, const char *label);
R01sEntity *r01s_ui_button_entity(R01sUiButton *btn);
void r01s_ui_button_set_callback(R01sUiButton *btn, R01sUiButtonFn fn, void *ctx);
void r01s_ui_button_set_label(R01sUiButton *btn, const char *label);
int r01s_ui_button_hit(const R01sUiButton *btn, int x, int y);
void r01s_ui_button_press(R01sUiButton *btn);

#endif

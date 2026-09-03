#include "retr01_sim/ui_button.h"

#include <stdio.h>
#include <string.h>

static void btn_reset(R01sEntity *e) {
    R01sUiButton *b = (R01sUiButton *)e;
    b->pressed = 0;
}

static void btn_eval(R01sEntity *e) {
    (void)e;
}

static void btn_tick(R01sEntity *e) {
    R01sUiButton *b = (R01sUiButton *)e;
    if (b->pressed > 0) {
        b->pressed--;
    }
}

static void btn_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable UI_BUTTON_VT = {btn_reset, btn_eval, btn_tick, btn_destroy};

void r01s_ui_button_init(R01sUiButton *btn, const char *refdes, const char *label) {
    if (!btn) {
        return;
    }
    memset(btn, 0, sizeof(*btn));
    r01s_entity_init(&btn->base, &UI_BUTTON_VT, "BTN", refdes ? refdes : "BTN");
    btn->base.impl = btn;
    btn->base.visual = R01S_ENTITY_VIS_BUTTON;
    r01s_entity_set_glyph(&btn->base, R01S_ENTITY_VIS_BUTTON, 72, 20);
    r01s_ui_button_set_label(btn, label ? label : "Button");
    r01s_entity_reset(&btn->base);
}

R01sEntity *r01s_ui_button_entity(R01sUiButton *btn) {
    return btn ? &btn->base : NULL;
}

void r01s_ui_button_set_callback(R01sUiButton *btn, R01sUiButtonFn fn, void *ctx) {
    if (!btn) {
        return;
    }
    btn->on_press = fn;
    btn->ctx = ctx;
}

void r01s_ui_button_set_label(R01sUiButton *btn, const char *label) {
    if (!btn) {
        return;
    }
    if (!label) {
        label = "";
    }
    snprintf(btn->label, sizeof(btn->label), "%s", label);
}

int r01s_ui_button_hit(const R01sUiButton *btn, int x, int y) {
    const R01sEntity *e;
    if (!btn) {
        return 0;
    }
    e = &btn->base;
    return x >= e->board_x && x < e->board_x + e->body_w && y >= e->board_y && y < e->board_y + e->body_h;
}

void r01s_ui_button_press(R01sUiButton *btn) {
    if (!btn) {
        return;
    }
    btn->pressed = 6;
    if (btn->on_press) {
        btn->on_press(btn->ctx);
    }
}

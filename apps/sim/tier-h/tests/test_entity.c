#include "retr01_sim/entity.h"
#include "sn74hc157.h"

#include <stdio.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    R01sSn74hc157 chip;
    R01sEntity *e;
    R01sPin *p;
    int body_w;
    int body_h;

    r01s_sn74hc157_init(&chip, "U7");
    e = r01s_sn74hc157_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(e->pin_count == 16, "16 pins");
    expect_true(e->dip_pins == 16, "16-pin package");
    expect_true(e->orient == R01S_ORIENT_H, "default horizontal");
    expect_true(e->body_w > 0 && e->body_h > 0, "body size");
    expect_true(e->part && e->part[0], "part name");
    expect_true(e->refdes && e->refdes[0] == 'U', "refdes");

    body_w = e->body_w;
    body_h = e->body_h;
    r01s_entity_set_orient(e, R01S_ORIENT_V);
    expect_true(e->body_w == body_h && e->body_h == body_w, "vertical swap");

    p = r01s_entity_pin(e, 1);
    expect_true(p != NULL && p->dir == R01S_PIN_IN, "pin 1 in");
    p = r01s_entity_pin(e, 16);
    expect_true(p != NULL && p->dir == R01S_PIN_PWR, "pin 16 pwr");
    expect_true(r01s_entity_pin(e, 99) == NULL, "missing pin");

    r01s_entity_reset(e);
    r01s_entity_eval(e);

    r01s_entity_place(e, 10, 20);
    expect_true(e->board_x == 10 && e->board_y == 20, "place");

    r01s_entity_destroy(e);

    if (g_fail) {
        fprintf(stderr, "test_entity: FAILED\n");
        return 1;
    }
    printf("test_entity: ok\n");
    return 0;
}

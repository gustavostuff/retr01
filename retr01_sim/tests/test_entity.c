#include "retr01_sim/entity.h"
#include "stub14.h"

#include <stdio.h>
#include <stdlib.h>

static int g_fail;

static void expect_true(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    R01sStub14 chip;
    R01sEntity *e;
    R01sPin *p;

    r01s_stub14_init(&chip, "U7");
    e = r01s_stub14_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(e->pin_count == 14, "14 pins");
    expect_true(e->dip_pins == 14, "14-pin package");
    expect_true(e->orient == R01S_ORIENT_H, "default horizontal");
    expect_true(e->pkg_len_mm == 19 && e->pkg_wid_mm == 6, "DIP-14 mm outline");
    expect_true(e->body_w == 19 * R01S_PX_PER_MM, "horizontal body_w");
    expect_true(e->body_h == 6 * R01S_PX_PER_MM, "horizontal body_h");
    expect_true(r01s_dip_body_along_px(40) == 52 * R01S_PX_PER_MM, "DIP-40 length px");
    expect_true(r01s_dip_body_across_px(40) == 14 * R01S_PX_PER_MM, "DIP-40 width px");
    expect_true(e->part && e->part[0], "part name");
    expect_true(e->refdes && e->refdes[0] == 'U', "refdes");

    r01s_entity_set_orient(e, R01S_ORIENT_V);
    expect_true(e->body_w == 6 * R01S_PX_PER_MM && e->body_h == 19 * R01S_PX_PER_MM, "vertical swap");

    p = r01s_entity_pin(e, 1);
    expect_true(p != NULL && p->dir == R01S_PIN_IN, "pin 1 in");
    p = r01s_entity_pin(e, 14);
    expect_true(p != NULL && p->dir == R01S_PIN_PWR, "pin 14 pwr");
    expect_true(r01s_entity_pin(e, 99) == NULL, "missing pin");

    r01s_entity_reset(e);
    r01s_entity_eval(e);
    r01s_entity_eval(e);
    expect_true(chip.eval_count == 2, "vtable eval");

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

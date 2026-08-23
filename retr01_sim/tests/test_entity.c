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
    expect_true(e->body_h == r01s_dip_body_h(14), "body_h from pitch");
    expect_true(e->body_h == R01S_DIP_PIN_MARGIN_Y * 2 + 6 * R01S_DIP_PIN_PITCH, "14-pin pitch=12");
    expect_true(r01s_dip_body_h(40) - r01s_dip_body_h(28) == 6 * R01S_DIP_PIN_PITCH, "same pitch 28↔40");
    expect_true(e->part && e->part[0], "part name");
    expect_true(e->refdes && e->refdes[0] == 'U', "refdes");

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

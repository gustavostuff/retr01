#include "osc_dot.h"

#include "discrete_ic/bus.h"
#include "test_common.h"

int main(void) {
    R01aOscDot osc;
    NsEntity *e;
    int i;
    int toggles = 0;
    NsLevel prev;

    r01a_osc_dot_init(&osc, "Y2");
    e = r01a_osc_dot_entity(&osc);
    ns_entity_drive(e, "VDD", NS_LVL_H);
    ns_entity_drive(e, "OE#", NS_LVL_H);
    prev = ns_entity_sense(e, "DOT");
    for (i = 0; i < 8; i++) {
        ns_entity_tick(e);
        if (ns_entity_sense(e, "DOT") != prev) {
            toggles++;
            prev = ns_entity_sense(e, "DOT");
        }
    }
    expect_true(toggles >= 7, "DOT toggles each tick");

    ns_entity_drive(e, "OE#", NS_LVL_L);
    ns_entity_tick(e);
    expect_true(ns_entity_sense(e, "DOT") == NS_LVL_Z, "OE# low -> hi-Z");

    return test_done("test_osc_dot");
}

#include "sn74hcu04.h"
#include "sn74hc74.h"

#include "discrete_ic/bus.h"
#include "test_common.h"

int main(void) {
    R01aSn74hcu04 u04;
    R01aSn74hc74 u74;
    NsEntity *e04;
    NsEntity *e74;
    int i;
    int u04_toggles = 0;
    int q1_toggles = 0;
    int q2_toggles = 0;
    NsLevel prev_2y;
    NsLevel prev_1q;
    NsLevel prev_2q;

    /* 1. Test 74HCU04 Pierce oscillator and buffer stage */
    r01a_sn74hcu04_init(&u04, "U04");
    e04 = r01a_sn74hcu04_entity(&u04);
    ns_entity_drive(e04, "VCC", NS_LVL_H);
    ns_entity_drive(e04, "GND", NS_LVL_L);
    ns_entity_drive(e04, "3A", NS_LVL_L);
    ns_entity_drive(e04, "4A", NS_LVL_L);
    ns_entity_drive(e04, "5A", NS_LVL_L);
    ns_entity_drive(e04, "6A", NS_LVL_L);

    prev_2y = ns_entity_sense(e04, "2Y");
    for (i = 0; i < 16; i++) {
        ns_entity_tick(e04);
        /* In Pierce mode, Gate 1 (1Y) drives Gate 2 (2A) */
        ns_entity_drive(e04, "2A", ns_entity_sense(e04, "1Y"));
        ns_entity_eval(e04);
        if (ns_entity_sense(e04, "2Y") != prev_2y) {
            u04_toggles++;
            prev_2y = ns_entity_sense(e04, "2Y");
        }
    }
    expect_true(u04_toggles >= 14, "74HCU04 master clock toggles");

    /* 2. Test 74HC74 dual D flip-flop frequency divider (divide by 2, then divide by 4) */
    r01a_sn74hc74_init(&u74, "U74");
    e74 = r01a_sn74hc74_entity(&u74);
    ns_entity_drive(e74, "VCC", NS_LVL_H);
    ns_entity_drive(e74, "GND", NS_LVL_L);
    ns_entity_drive(e74, "1PRE#", NS_LVL_H);
    ns_entity_drive(e74, "1CLR#", NS_LVL_H);
    ns_entity_drive(e74, "2PRE#", NS_LVL_H);
    ns_entity_drive(e74, "2CLR#", NS_LVL_H);

    /* Feedback 1/Q to 1D, 2/Q to 2D */
    ns_entity_drive(e74, "1D", ns_entity_sense(e74, "1/Q"));
    ns_entity_drive(e74, "2D", ns_entity_sense(e74, "2/Q"));
    ns_entity_eval(e74);

    prev_1q = ns_entity_sense(e74, "1Q");
    prev_2q = ns_entity_sense(e74, "2Q");

    /* Clock with 16 pulses (32 edges) */
    for (i = 0; i < 32; i++) {
        NsLevel clk = (i & 1) ? NS_LVL_H : NS_LVL_L;
        ns_entity_drive(e74, "1CLK", clk);
        ns_entity_eval(e74);
        ns_entity_tick(e74);
        ns_entity_drive(e74, "1D", ns_entity_sense(e74, "1/Q"));

        /* Stage 2 is clocked by 1Q */
        ns_entity_drive(e74, "2CLK", ns_entity_sense(e74, "1Q"));
        ns_entity_eval(e74);
        ns_entity_tick(e74);
        ns_entity_drive(e74, "2D", ns_entity_sense(e74, "2/Q"));
        ns_entity_eval(e74);

        if (ns_entity_sense(e74, "1Q") != prev_1q) {
            q1_toggles++;
            prev_1q = ns_entity_sense(e74, "1Q");
        }
        if (ns_entity_sense(e74, "2Q") != prev_2q) {
            q2_toggles++;
            prev_2q = ns_entity_sense(e74, "2Q");
        }
    }

    /* 16 rising edges on 1CLK -> 16 toggles of 1Q (divide by 2) */
    expect_true(q1_toggles == 16, "74HC74 stage 1 divides clock by 2");
    /* 8 rising edges on 1Q -> 8 toggles of 2Q (divide by 4) */
    expect_true(q2_toggles == 8, "74HC74 stage 2 divides clock by 4 to generate DOT clock");

    /* 3. Test asynchronous clear */
    ns_entity_drive(e74, "1CLR#", NS_LVL_L);
    ns_entity_eval(e74);
    expect_true(ns_entity_sense(e74, "1Q") == NS_LVL_L, "74HC74 async 1CLR# resets 1Q to LOW");
    expect_true(ns_entity_sense(e74, "1/Q") == NS_LVL_H, "74HC74 async 1CLR# sets 1/Q to HIGH");

    return test_done("test_clock_stage");
}

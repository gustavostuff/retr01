#include "retr01_sim/board.h"
#include "retr01_sim/board_netlist.h"
#include "retr01_sim/island_builder.h"
#include "test_common.h"

#include <string.h>

static int passive_slots(const R01sBoard *board) {
    int i;
    int n = 0;
    const R01sPinNetlist *nl = &board->pin_netlist;
    for (i = 0; i < nl->slot_count; i++) {
        const R01sEntity *e = nl->slots[i].entity;
        if (e && e->visual == R01S_ENTITY_VIS_PASSIVE) {
            n++;
        }
    }
    return n;
}

static R01sEntity *passive_by_refdes(R01sBoard *board, const char *refdes) {
    int i;
    for (i = 0; i < board->passives.count; i++) {
        if (board->passives.parts[i].base.refdes && strcmp(board->passives.parts[i].base.refdes, refdes) == 0) {
            return &board->passives.parts[i].base;
        }
    }
    return NULL;
}

int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;
    int passive_pins;
    R01sPinNetlist *nl;
    R01sEntity *r1;
    R01sEntity *c1;

    memset(&board, 0, sizeof(board));
    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    expect_true(board.passives.count == 62, "passive BOM count");
    nl = &board.pin_netlist;
    passive_pins = passive_slots(&board);
    expect_true(passive_pins >= 122, "passive pins registered in netlist");
    expect_true(r01s_pin_netlist_net_count(nl) < nl->slot_count,
                "net count below registered pin slots");
    (void)passive_pins;

    r1 = passive_by_refdes(&board, "R1");
    c1 = passive_by_refdes(&board, "C1");
    expect_true(r1 != NULL && c1 != NULL, "R1/C1 in BOM");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_at27c256r_entity(&board.color_prom), "O7", r1, "1"),
                "DAC R1 on U24 O7");
    expect_true(r01s_pin_netlist_same_net(nl, r1, "2", r01s_video_sink_entity(&board.video_sink), "RIN"),
                "DAC R1 to SCR1 RIN");
    expect_true(r01s_pin_netlist_same_net(nl, c1, "1", r01s_w65c02s_entity(&board.cpu), "VDD"),
                "C1 bypass to U1 VDD");
    expect_true(r01s_pin_netlist_same_net(nl, c1, "2", r01s_pwr5v_entity(&board.pwr), "GND"),
                "C1 bypass to GND");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "R12"), "2",
                                          r01s_w65c02s_entity(&board.cpu), "PHI2"),
                "R12 series to CPU PHI2");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "R12"), "1",
                                          r01a_sn74hcu04_entity(&board.u04), "4Y"),
                "R12 series to U04 PHI2 buffer");
    expect_true(board.passives.parts[0].kind == R01S_PASSIVE_XTAL, "Y1 is XTAL");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "Y1"), "1",
                                          r01a_sn74hcu04_entity(&board.u04), "3A"),
                "Y1 Pierce on U04 gate 3");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "Y2"), "1",
                                          r01a_sn74hcu04_entity(&board.u04), "1A"),
                "Y2 Pierce on U04 gate 1");
    expect_true(r01s_pin_netlist_same_net(nl, r01a_sn74hc74_entity(&board.u74), "2Q",
                                          passive_by_refdes(&board, "R13"), "1"),
                "U74 DOT into R13");

    expect_true(r01s_pin_netlist_same_net(nl, r01s_avr128db28_s2_entity(&board.mcu_s2), "AUDIO_PWM",
                                          r01s_rca_jack_entity(&board.j8), "2"),
                "J8 tip on MCU-S2 PWM");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_w65c02s_entity(&board.cpu), "A0",
                                          r01s_sn74hc157_entity(&board.mux157[0]), "1A"),
                "CPU A0 on U7A 1A");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_sn74hc157_entity(&board.mux157[0]), "1Y",
                                          r01s_as6c62256_entity(&board.vram), "A0"),
                "U7A 1Y on VRAM A0");
    expect_true(!r01s_pin_netlist_same_net(nl, r01s_w65c02s_entity(&board.cpu), "A0",
                                           r01s_as6c62256_entity(&board.vram), "A0"),
                "CPU A0 is not shorted to VRAM A0");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_compositor_entity(&board.compositor), "A0",
                                          r01s_at27c256r_entity(&board.color_prom), "A0"),
                "color index A0");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "R14"), "2", &board.io.j36, "D0"),
                "R14 cart side on J36 D0");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.u130, "RESET#", r01s_w65c02s_entity(&board.cpu), "RESB"),
                "MCP130 reset on CPU RESB");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "C17"), "1", &board.io.u130, "VDD"),
                "C17 on MCP130 VDD");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "P0", r01s_avr128db28_s2_entity(&board.mcu_s2), "PAD0"),
                "J5 P0 on PAD0");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "RIGHT", r01s_avr128db28_s2_entity(&board.mcu_s2),
                                          "P2_RIGHT"),
                "J5 RIGHT on P2");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "GND17", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J5 pin 17 GND");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_ad724_entity(&board.ad724), "COMP",
                                          r01s_rca_jack_entity(&board.j9), "2"),
                "J9 tip on AD724 COMP");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_rca_jack_entity(&board.j8), "1A",
                                          r01s_pwr5v_entity(&board.pwr), "GND"),
                "J8 shell GND");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_board_netlist");
}

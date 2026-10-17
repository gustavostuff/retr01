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
    expect_true(board.passives.count == 57, "passive BOM count");
    nl = &board.pin_netlist;
    passive_pins = passive_slots(&board);
    expect_true(passive_pins >= 106, "passive pins registered in netlist");
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
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "5V"), "5V",
                                          r01s_pwr5v_entity(&board.pwr), "VDD"),
                "5V symbol on +5V");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "GND"), "GND",
                                          r01s_pwr5v_entity(&board.pwr), "GND"),
                "GND symbol on GND");
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
                                          &board.io.j2, "AUDIO"),
                "J2 pin 6 on MCU-S2 PWM");
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
    expect_true(r01s_pin_netlist_same_net(nl, r01s_w65c02s_entity(&board.cpu), "D0", &board.io.j36, "D0"),
                "U1 D0 on J36 D0");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.u130, "RESET#", r01s_w65c02s_entity(&board.cpu), "RESB"),
                "MCP130 reset on CPU RESB");
    expect_true(r01s_pin_netlist_same_net(nl, passive_by_refdes(&board, "C17"), "1", &board.io.u130, "VDD"),
                "C17 on MCP130 VDD");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "P0", r01s_avr128db28_s2_entity(&board.mcu_s2), "PAD0"),
                "J5 P0 on PAD0");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "P7", r01s_avr128db28_s2_entity(&board.mcu_s2), "PAD7"),
                "J5 pin 1 on US2 1");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "P6", r01s_avr128db28_s2_entity(&board.mcu_s2), "PAD6"),
                "J5 pin 3 on US2 28");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "P5", r01s_avr128db28_s2_entity(&board.mcu_s2), "PAD5"),
                "J5 pin 5 on US2 27");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "RIGHT", r01s_avr128db28_s2_entity(&board.mcu_s2),
                                          "P2_RIGHT"),
                "J6 RIGHT on P2");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "START", r01s_avr128db28_s2_entity(&board.mcu_s2),
                                          "P2_START"),
                "J6 pin 7 on US2 18");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "COIN", r01s_avr128db28_s2_entity(&board.mcu_s2),
                                          "P2_COIN"),
                "J6 pin 9 on US2 9");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "Y", r01s_avr128db28_s2_entity(&board.mcu_s2), "P2_Y"),
                "J6 pin 5 on US2 8");
    expect_true(board.io.j2.pin_count == 9, "J2 is 9-pin mini-DIN");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "CSYNC", r01s_ad724_entity(&board.ad724), "SYNCIN"),
                "J2 pin 5 CSYNC on CXA2075M SYNCIN");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "VCC", r01s_pwr5v_entity(&board.pwr), "VDD"),
                "J2 pin 2 on +5V");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "RED", passive_by_refdes(&board, "R9"), "1"),
                "J2 pin 7 on R9");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "GREEN", passive_by_refdes(&board, "R10"), "1"),
                "J2 pin 3 on R10");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "BLUE", passive_by_refdes(&board, "R11"), "1"),
                "J2 pin 1 on R11");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "AUDIO_L", &board.io.j2, "AUDIO"),
                "J2 pins 6 and 8 share audio");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j2, "AUDIO_R", &board.io.j2, "AUDIO"),
                "J2 pins 6 and 9 share audio");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j5, "GND7", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J5 pin 7 GND");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "GND1", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J6 pin 1 GND");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j6, "GND3", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J6 pin 3 GND");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_ad724_entity(&board.ad724), "CVOUT", &board.io.j2, "COMP"),
                "J2 pin 4 on CXA2075M CVOUT");
    expect_true(board.io.j11.pin_count == 4, "J11 is 2x2");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j11, "NTSC", r01s_pwr5v_entity(&board.pwr), "VDD"),
                "J11 pin 1 on +5V");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j11, "PAL", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J11 pin 4 on GND");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j11, "NPIN", r01s_ad724_entity(&board.ad724), "NPIN"),
                "J11 pin 2 on CXA2075M NPIN");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j11, "NPIN2", r01s_ad724_entity(&board.ad724), "NPIN"),
                "J11 pin 3 on CXA2075M NPIN");
    expect_true(!r01s_pin_netlist_same_net(nl, &board.io.j11, "NPIN", r01s_pwr5v_entity(&board.pwr), "VDD"),
                "NPIN stays open until the shunt is fitted");
    expect_true(r01s_ad724_entity(&board.ad724)->pin_count == 11, "U2075 wired pins");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_ad724_entity(&board.ad724), "GND1",
                                          r01s_pwr5v_entity(&board.pwr), "GND"),
                "U2075 pin 1 on GND");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_ad724_entity(&board.ad724), "GND2",
                                          r01s_pwr5v_entity(&board.pwr), "GND"),
                "U2075 pin 24 on GND");
    expect_true(r01s_pin_netlist_same_net(nl, r01s_ad724_entity(&board.ad724), "SCIN",
                                          r01a_sn74hcu04_entity(&board.u04), "6Y"),
                "U2075 SCIN on U04 gate 6");
    expect_true(board.io.j10.pin_count == 4, "J10 is 2x2");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j10, "GND1", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J10 pin 2 GND");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j10, "GND2", r01s_pwr5v_entity(&board.pwr), "GND"),
                "J10 pin 4 GND");
    expect_true(r01s_pin_netlist_same_net(nl, &board.io.j10, "DATA",
                                          r01s_avr128db28_m_entity(&board.mcu_m), "SPI_MISO"),
                "J10 DATA on UM PC1");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_board_netlist");
}

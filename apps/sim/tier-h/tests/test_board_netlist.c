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
    expect_true(board.passives.count == 61, "passive BOM count");
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
                                          r01s_osc8m_entity(&board.osc), "PHI2"),
                "R12 series to OSC PHI2");

    r01s_island_builder_shutdown(&builder);
    return test_done("test_board_netlist");
}

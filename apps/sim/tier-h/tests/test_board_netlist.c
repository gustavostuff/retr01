#include "retr01_sim/board.h"
#include "retr01_sim/board_netlist.h"
#include "retr01_sim/island_builder.h"
#include "test_common.h"

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

int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;
    int passive_pins;
    int slot_y1;

    memset(&board, 0, sizeof(board));
    r01s_island_builder_init(&builder);
    expect_true(r01s_board_build(&board, &builder) == 0, "board build");
    expect_true(board.passives.count == 61, "passive BOM count");
    passive_pins = passive_slots(&board);
    expect_true(passive_pins >= 122, "passive pins registered in netlist");
    slot_y1 = r01s_pin_netlist_slot_for_name((R01sPinNetlist *)&board.pin_netlist,
                                             &board.passives.parts[0].base, "OE#");
    expect_true(slot_y1 >= 0, "crystal Y1 OE# in netlist");
    expect_true(r01s_pin_netlist_net_count(&board.pin_netlist) > passive_pins, "IC nets plus passive singletons");
    r01s_island_builder_shutdown(&builder);
    return test_done("test_board_netlist");
}

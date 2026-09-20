#include "r01a_board.h"
#include "r01a_layout.h"

#include "netlist_sim/entity.h"
#include "netlist_sim/passive.h"
#include "test_common.h"

int main(void) {
    R01aBoard board;
    R01aBoard loaded;
    char path[] = "test_layout.json";
    int pan_x = 0;
    int pan_y = 0;
    NsEntity *osc;
    NsPbHole a = {4, NS_PB_LANE_A};
    NsPbHole b = {12, NS_PB_LANE_A};

    r01a_board_init(&board);
    expect_true(board.passives.count == 21, "Tier A passive count");
    osc = r01a_osc_dot_entity(&board.osc_dot);
    ns_entity_place(osc, 120, 80);
    ns_entity_set_orient(osc, NS_ORIENT_0);
    ns_passive_set_pivot(&board.passives.parts[0], 200, 90);
    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    expect_true(r01a_board_jumper_add(&board, a, b), "save jumper");

    expect_true(r01a_layout_save(path, &board, 17, 23) == 0, "layout save");

    r01a_board_init(&loaded);
    expect_true(r01a_layout_load(path, &loaded, &pan_x, &pan_y) == 0, "layout load");
    expect_true(pan_x == 17 && pan_y == 23, "pan restored");
    expect_true(r01a_board_wire_mode(&loaded) == R01A_WIRE_MANUAL, "wire mode restored");
    expect_true(r01a_osc_dot_entity(&loaded.osc_dot)->board_x == 120, "OSC x restored");
    expect_true(r01a_osc_dot_entity(&loaded.osc_dot)->board_y == 80, "OSC y restored");
    expect_true(loaded.passives.parts[0].pivot_x == 200, "passive pivot x");
    expect_true(loaded.passives.parts[0].pivot_y == 90, "passive pivot y");
    expect_true(loaded.jumper_count == 1, "jumper count");
    expect_true(loaded.jumpers[0].a.col == 4 && loaded.jumpers[0].b.col == 12, "jumper holes");

    r01a_board_shutdown(&board);
    r01a_board_shutdown(&loaded);
    return test_done("test_layout");
}

#include "r01a_board.h"
#include "r01a_layout.h"

#include "discrete_ic/entity.h"
#include "discrete_ic/passive.h"
#include "test_common.h"

#include <string.h>

int main(void) {
    R01aBoard board;
    R01aBoard loaded;
    char path[] = "test_layout.json";
    int pan_x = 0;
    int pan_y = 0;
    int zoom = 1;
    int air_always = 0;
    NsEntity *osc;
    NsPbHole a = {4, NS_PB_LANE_A};
    NsPbHole b = {12, NS_PB_LANE_A};

    r01a_board_init(&board);
    expect_true(board.passives.count == 19, "Tier B passive count");
    osc = r01a_osc_dot_entity(&board.osc_dot);
    ns_entity_place(osc, 120, 80);
    ns_entity_set_orient(osc, NS_ORIENT_0);
    ns_passive_set_pivot(&board.passives.parts[0], 200, 90);
    {
        int ri;
        for (ri = 0; ri < board.passives.count; ri++) {
            if (board.passives.parts[ri].kind == NS_PASSIVE_R) {
                ns_passive_set_leg_ext(&board.passives.parts[ri], 1, 15);
                ns_passive_set_leg_ext(&board.passives.parts[ri], 2, 10);
                break;
            }
        }
    }
    r01a_board_set_wire_mode(&board, R01A_WIRE_MANUAL);
    expect_true(r01a_board_jumper_add(&board, a, b), "save jumper");
    expect_true(r01a_board_jumper_set_route(&board, 0, 1, 96), "custom elbows");
    expect_true(r01a_board_add_breadboard(&board, 40, 50) != NULL, "add extra bb");
    {
        NsBreadboard *bb2 = (NsBreadboard *)r01a_board_entity_by_refdes(&board, "BB2");
        NsPbHole xa = {0, NS_PB_LANE_TOP_POS};
        NsPbHole xb = {2, NS_PB_LANE_A};
        expect_true(bb2 != NULL, "BB2 before save");
        expect_true(r01a_board_jumper_add_across(&board, &board.breadboard, xa, bb2, xb, 10, 20, 30),
                    "save cross jumper");
    }
    expect_true(r01a_board_add_passive(&board, NS_PASSIVE_R, "33", 300, 110) != NULL, "add extra R");

    expect_true(r01a_layout_save(path, &board, 17, 23, 3, 1) == 0, "layout save");

    r01a_board_init(&loaded);
    expect_true(r01a_layout_load(path, &loaded, &pan_x, &pan_y, &zoom, &air_always) == 0, "layout load");
    expect_true(pan_x == 17 && pan_y == 23, "pan restored");
    expect_true(zoom == 3, "zoom restored");
    expect_true(air_always == 1, "air always restored");
    expect_true(r01a_board_wire_mode(&loaded) == R01A_WIRE_MANUAL, "wire mode restored");
    expect_true(r01a_osc_dot_entity(&loaded.osc_dot)->board_x == 120, "OSC x restored");
    expect_true(r01a_osc_dot_entity(&loaded.osc_dot)->board_y == 80, "OSC y restored");
    expect_true(loaded.passives.parts[0].pivot_x == 200, "passive pivot x");
    expect_true(loaded.passives.parts[0].pivot_y == 90, "passive pivot y");
    {
        int ri;
        int found = 0;
        for (ri = 0; ri < loaded.passives.count; ri++) {
            if (loaded.passives.parts[ri].kind == NS_PASSIVE_R && loaded.passives.parts[ri].leg_ext[0] == 15 &&
                loaded.passives.parts[ri].leg_ext[1] == 10) {
                found = 1;
                break;
            }
        }
        expect_true(found, "resistor leg stretch restored");
    }
    expect_true(loaded.jumper_count == 2, "jumper count");
    expect_true(loaded.jumpers[0].a.col == 4 && loaded.jumpers[0].b.col == 12, "jumper holes");
    expect_true(loaded.jumpers[0].route == 1 && loaded.jumpers[0].h_first == 1 && loaded.jumpers[0].mid == 96,
                "jumper elbows restored");
    expect_true(strcmp(r01a_jumper_a_ref(&loaded.jumpers[1]), "BB1") == 0, "cross A board");
    expect_true(strcmp(r01a_jumper_b_ref(&loaded.jumpers[1]), "BB2") == 0, "cross B board");
    expect_true(loaded.jumpers[1].a.col == 0 && loaded.jumpers[1].b.col == 2, "cross holes");
    r01a_board_jumper_remove(&loaded, 1);
    expect_true(loaded.jumper_count == 1, "cross jumper dropped after check");
    expect_true(loaded.extra_bb_count == 1, "extra bb count");
    expect_true(r01a_board_entity_by_refdes(&loaded, "BB2") != NULL, "BB2 restored");
    expect_true(r01a_board_entity_by_refdes(&loaded, "BB2")->board_x == 40, "BB2 x");
    expect_true(loaded.passives.count == 20, "extra passive count");
    expect_true(loaded.jumpers[0].r == 220 && loaded.jumpers[0].g == 160 && loaded.jumpers[0].bcol == 40,
                "jumper color restored");
    {
        NsPbHole c = {20, NS_PB_LANE_A};
        expect_true(r01a_board_jumper_set_end(&loaded, 0, 1, c), "move jumper B");
        expect_true(loaded.jumpers[0].b.col == 20, "jumper B col");
        r01a_board_jumper_remove(&loaded, 0);
        expect_true(loaded.jumper_count == 0, "jumper removed");
        expect_true(r01a_board_jumper_add_on(&loaded, &loaded.breadboard, a, b, 50, 120, 220), "colored jumper");
        expect_true(loaded.jumpers[0].r == 50 && loaded.jumpers[0].bcol == 220, "jumper rgb");
    }
    {
        NsBreadboard *bb2 = (NsBreadboard *)r01a_board_entity_by_refdes(&loaded, "BB2");
        NsPbHole ja = {2, NS_PB_LANE_A};
        NsPbHole jb = {8, NS_PB_LANE_A};
        int kept = loaded.jumper_count;
        expect_true(bb2 != NULL, "BB2 ptr");
        expect_true(r01a_board_jumper_add_on(&loaded, bb2, ja, jb, 50, 50, 50), "jumper on BB2");
        expect_true(r01a_board_add_breadboard(&loaded, 80, 50) != NULL, "add another bb");
        expect_true(r01a_board_remove_breadboard(&loaded, bb2), "remove BB2");
        expect_true(loaded.extra_bb_count == 1, "extra compact");
        expect_true(r01a_board_entity_by_refdes(&loaded, "BB2") == NULL, "BB2 gone");
        expect_true(r01a_board_entity_by_refdes(&loaded, "BB3") != NULL, "BB3 kept");
        expect_true(loaded.jumper_count == kept, "BB2 jumpers stripped");
        expect_true(r01a_board_remove_breadboard(&loaded, &loaded.breadboard), "remove BB1");
        expect_true(r01a_board_entity_by_refdes(&loaded, "BB1") == NULL, "BB1 gone");
        expect_true(r01a_board_add_breadboard(&loaded, 10, 10) == &loaded.breadboard, "restore BB1");
        expect_true(r01a_board_entity_by_refdes(&loaded, "BB1") != NULL, "BB1 back");
    }

    r01a_board_shutdown(&board);
    r01a_board_shutdown(&loaded);
    return test_done("test_layout");
}

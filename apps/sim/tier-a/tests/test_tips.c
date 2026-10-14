#include "r01a_board.h"

#include "test_common.h"

#include <string.h>

static void expect_role(R01aBoard *board, const char *ref, const char *role) {
    NsEntity *e = r01a_board_entity_by_refdes(board, ref);
    char buf[64];
    expect_true(e != NULL, ref);
    if (!e) {
        return;
    }
    buf[0] = '\0';
    expect_true(r01s_ui_tip_role(e, buf, sizeof(buf)) == 1, ref);
    expect_true(strcmp(buf, role) == 0, role);
}

int main(void) {
    R01aBoard board;
    NsEntity *beam;
    NsEntity *prom;
    NsEntity *ff;
    char buf[64];

    r01a_board_init(&board);
    expect_role(&board, "R1", "red O7");
    expect_role(&board, "R8", "blue O0");
    expect_role(&board, "R9", "red load");
    expect_role(&board, "R12", "DOT series");
    expect_role(&board, "R13", "Pierce feedback");
    expect_role(&board, "C1", "U04 bypass");
    expect_role(&board, "C6", "Pierce load");
    expect_role(&board, "Y2", "crystal");
    expect_role(&board, "U04", "Pierce clock");
    expect_role(&board, "U74", "divide by 4");
    expect_role(&board, "UPLDX", "Beam X");
    expect_role(&board, "UPLDY", "Beam Y");
    expect_role(&board, "U24", "color PROM");
    expect_role(&board, "J2", "RGBS header");

    beam = r01a_board_entity_by_refdes(&board, "UPLDX");
    prom = r01a_board_entity_by_refdes(&board, "U24");
    ff = r01a_board_entity_by_refdes(&board, "U74");
    buf[0] = '\0';
    expect_true(beam && r01s_ui_tip_live(beam, buf, sizeof(buf)) == 1, "beam live");
    expect_true(strstr(buf, "dot ") == buf, "beam dot count");
    buf[0] = '\0';
    expect_true(prom && r01s_ui_tip_live(prom, buf, sizeof(buf)) == 1, "prom live");
    expect_true(strstr(buf, "index ") == buf, "prom index");
    buf[0] = '\0';
    expect_true(ff && r01s_ui_tip_live(ff, buf, sizeof(buf)) == 1, "divider live");
    expect_true(strstr(buf, "DOT ") == buf, "DOT level");

    r01a_board_shutdown(&board);
    return test_done("test_tips");
}

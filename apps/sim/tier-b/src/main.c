#include "r01a_board.h"
#include "ui.h"

int main(void) {
    R01aBoard board;
    int rc;
    r01a_board_init(&board);
    rc = r01a_ui_run(&board);
    r01a_board_shutdown(&board);
    return rc;
}

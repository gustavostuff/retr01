#include "r01a_lab_sim.h"

#include "r01a_board.h"
#include "r01a_netlist.h"

void r01a_lab_boot(R01aBoard *board) {
    if (!board) {
        return;
    }
    r01a_board_set_wire_mode(board, R01A_WIRE_AUTO);
    r01a_netlist_fill_pins(board);
}

NsIslandGroup *r01a_lab_group(R01aBoard *board) {
    return board ? r01a_board_group(board) : 0;
}

NsPinNetlist *r01a_lab_pins(R01aBoard *board) {
    return board ? &board->pin_net : 0;
}

NsIslandBuilder *r01a_lab_builder(R01aBoard *board) {
    return board ? &board->builder : 0;
}

int r01a_lab_running(const R01aBoard *board) {
    return board && board->running;
}

void r01a_lab_set_running(R01aBoard *board, int on) {
    if (board) {
        board->running = on ? 1 : 0;
    }
}

void r01a_lab_step(R01aBoard *board) {
    if (board) {
        r01a_board_step(board);
    }
}

void r01a_lab_step_dots(R01aBoard *board, uint32_t dots) {
    if (board) {
        r01a_board_step_dots(board, dots);
    }
}

void r01a_lab_reset(R01aBoard *board) {
    if (board) {
        r01a_board_reset(board);
    }
}

#ifndef RETR01_SIM_BOARD_NETLIST_H
#define RETR01_SIM_BOARD_NETLIST_H

struct R01sBoard;

/* Register every board/passive pin and apply motherboard connectivity links. */
void r01s_board_netlist_rebuild(struct R01sBoard *board);

#endif

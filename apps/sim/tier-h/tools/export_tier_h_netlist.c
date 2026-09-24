/*
 * Dump Tier H board pin connectivity as JSON (stdout).
 * See docs/bringup/tier-h-skidl-export.md — illustrative / not fab-ready.
 */
#include "retr01_sim/board.h"
#include "retr01_sim/board_netlist.h"
#include "retr01_sim/island_builder.h"

#include <stdio.h>
#include <string.h>

int main(void) {
    R01sBoard board;
    R01sIslandBuilder builder;

    memset(&board, 0, sizeof(board));
    r01s_island_builder_init(&builder);
    if (r01s_board_build(&board, &builder) != 0) {
        fprintf(stderr, "export_tier_h_netlist: board build failed\n");
        r01s_island_builder_shutdown(&builder);
        return 1;
    }
    if (r01s_pin_netlist_write_json(&board.pin_netlist, stdout) != 0) {
        fprintf(stderr, "export_tier_h_netlist: json write failed\n");
        r01s_island_builder_shutdown(&builder);
        return 1;
    }
    r01s_island_builder_shutdown(&builder);
    return 0;
}

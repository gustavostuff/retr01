#ifndef retr01_SIM_BOARD_DEBUG_H
#define retr01_SIM_BOARD_DEBUG_H

#include "retr01_sim/board.h"

#include <stdint.h>

/* Wall-clock board dump for post-run analysis.
 * Enable with --debug or R01S_DEBUG=1. Default file: retr01_sim/debug/sim_trace.log
 * (override with R01S_DEBUG_LOG). Snapshots ~1 Hz + health/NMI edges. */

void r01s_board_debug_begin(R01sBoard *board, int enabled);
void r01s_board_debug_tick(R01sBoard *board, uint32_t wall_ms);
void r01s_board_debug_end(void);

#endif

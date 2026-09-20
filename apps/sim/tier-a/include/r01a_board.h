#ifndef R01A_BOARD_H
#define R01A_BOARD_H

#include "ad724.h"
#include "at27c256r.h"
#include "atf22v10.h"
#include "osc_dot.h"
#include "osc_fsc.h"
#include "pwr5v.h"

#include "netlist_sim/breadboard.h"
#include "netlist_sim/island_builder.h"
#include "netlist_sim/passive.h"
#include "netlist_sim/video_sink.h"

#include <stdint.h>

enum {
    R01A_ISLAND_VIDEO = 0,
    R01A_ISLAND_COUNT = 1
};

#define R01A_WIRE_AUTO 0
#define R01A_WIRE_MANUAL 1
#define R01A_JUMPER_MAX 64

typedef struct R01aJumper {
    NsPbHole a;
    NsPbHole b;
} R01aJumper;

typedef struct R01aBoard {
    R01aPwr5v pwr;
    R01aOscDot osc_dot;
    R01aOscFsc osc_fsc;
    R01aAtf22v10 beam_x;
    R01aAtf22v10 beam_y;
    R01aAt27c256r prom;
    R01aAd724 ad724;
    NsVideoSink sink;
    NsBreadboard breadboard;
    NsPassiveBank passives;
    NsIslandBuilder builder;
    R01aJumper jumpers[R01A_JUMPER_MAX];
    int jumper_count;
    int wire_mode; /* R01A_WIRE_AUTO or R01A_WIRE_MANUAL */
    int prev_y;
    int running;
} R01aBoard;

void r01a_board_init(R01aBoard *board);
void r01a_board_shutdown(R01aBoard *board);
void r01a_board_reset(R01aBoard *board);
/* One DOT oscillator half-cycle plus combinatorial settle. */
void r01a_board_step(R01aBoard *board);
/* Advance this many whole dots (two half-cycles each). */
void r01a_board_step_dots(R01aBoard *board, uint32_t dots);
NsIslandGroup *r01a_board_group(R01aBoard *board);

void r01a_board_set_wire_mode(R01aBoard *board, int mode);
int r01a_board_wire_mode(const R01aBoard *board);
int r01a_board_jumper_add(R01aBoard *board, NsPbHole a, NsPbHole b);
void r01a_board_jumper_clear(R01aBoard *board);
NsEntity *r01a_board_entity_by_refdes(R01aBoard *board, const char *refdes);

#endif

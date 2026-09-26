#ifndef R01A_BOARD_H
#define R01A_BOARD_H

#include "as6c62256.h"
#include "at27c256r.h"
#include "atf22v10.h"
#include "avr128db28_s1.h"
#include "osc_dot.h"
#include "rgbs_hdr.h"
#include "sn74hc573.h"

#include "discrete_ic/breadboard.h"
#include "discrete_ic/island_builder.h"
#include "discrete_ic/passive.h"
#include "discrete_ic/video_sink.h"

#include <stdint.h>

enum {
    R01A_ISLAND_VIDEO = 0,
    R01A_ISLAND_COUNT = 1
};

#define R01A_WIRE_AUTO 0
#define R01A_WIRE_MANUAL 1
#define R01A_JUMPER_MAX 256
#define R01A_BB_EXTRA_MAX 7
#define R01A_BB_REF_LEN 12

typedef struct R01aJumper {
    NsPbHole a;
    NsPbHole b;
    char bb_ref[R01A_BB_REF_LEN]; /* board for hole a */
    char b_ref[R01A_BB_REF_LEN];  /* board for hole b; empty means same as bb_ref */
    uint8_t r;
    uint8_t g;
    uint8_t bcol;
    uint8_t route;   /* 0 auto 2-elbow, 1 custom mid */
    uint8_t h_first; /* 1 = H-V-H, 0 = V-H-V (when route) */
    int16_t mid;
} R01aJumper;

typedef struct R01aBoard {
    R01aOscDot osc_dot;
    R01aAtf22v10 beam_x;
    R01aAtf22v10 beam_y;
    R01aAtf22v10 compositor;
    R01aAvr128db28S1 mcu_s1;
    R01aSn74hc573 field_latch;
    R01aAs6c62256 field_sram;
    R01aAt27c256r prom;
    R01aRgbsHdr rgbs;
    NsVideoSink sink;
    NsBreadboard breadboard;
    NsBreadboard extra_bb[R01A_BB_EXTRA_MAX];
    int extra_bb_count;
    NsPassiveBank passives;
    NsIslandBuilder builder;
    R01aJumper jumpers[R01A_JUMPER_MAX];
    int jumper_count;
    int wire_mode; /* R01A_WIRE_AUTO or R01A_WIRE_MANUAL */
    int prev_y;
    int prev_vblank;
    int bg0_ping;
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
int r01a_board_jumper_add_on(R01aBoard *board, NsBreadboard *bb, NsPbHole a, NsPbHole b, uint8_t cr,
                            uint8_t cg, uint8_t cb);
int r01a_board_jumper_add_across(R01aBoard *board, NsBreadboard *bb_a, NsPbHole a, NsBreadboard *bb_b,
                                NsPbHole b, uint8_t cr, uint8_t cg, uint8_t cb);
void r01a_board_jumper_remove(R01aBoard *board, int index);
int r01a_board_jumper_set_end(R01aBoard *board, int index, int end_b, NsPbHole hole);
int r01a_board_jumper_set_end_on(R01aBoard *board, int index, int end_b, NsBreadboard *bb, NsPbHole hole);
int r01a_board_jumper_set_route(R01aBoard *board, int index, int h_first, int mid);
void r01a_board_jumper_clear(R01aBoard *board);

static inline const char *r01a_jumper_a_ref(const R01aJumper *j) {
    return (j && j->bb_ref[0]) ? j->bb_ref : "BB1";
}

static inline const char *r01a_jumper_b_ref(const R01aJumper *j) {
    if (j && j->b_ref[0]) {
        return j->b_ref;
    }
    return r01a_jumper_a_ref(j);
}

const uint8_t *r01a_field_mem(const R01aBoard *board);
NsEntity *r01a_board_entity_by_refdes(R01aBoard *board, const char *refdes);
NsPassive *r01a_board_add_passive(R01aBoard *board, NsPassiveKind kind, const char *value, int x, int y);
NsBreadboard *r01a_board_add_breadboard(R01aBoard *board, int x, int y);
int r01a_board_remove_breadboard(R01aBoard *board, NsBreadboard *bb);

#endif

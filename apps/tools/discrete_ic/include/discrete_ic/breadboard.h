#ifndef DISCRETE_IC_BREADBOARD_H
#define DISCRETE_IC_BREADBOARD_H

#include "discrete_ic/entity.h"

#include <SDL.h>

/*
 * Solderless breadboard visual (nano protoboard layout).
 * Drag / rotate / hover rail highlight. Strip IDs feed Manual-mode routing.
 *
 * 63 terminal columns x 10 + 4 power rails (830-point class).
 *
 * Lattice (default): 1 px hole + 4 px gap = 5 px pitch.
 * R01A_BB_3PX (tier A): 3×3 px holes, 3 px gap (6 px pitch); body W/H are multiples of 3 px
 * so a grid-snapped board origin puts every hole center on the world 3 px lattice.
 */
#define NS_PB_COLS 63
#ifdef R01A_BB_3PX
#define NS_PB_HOLE 3
#define NS_PB_GAP 3
#define NS_PB_PITCH (NS_PB_HOLE + NS_PB_GAP)
#define NS_PB_MARGIN NS_PB_PITCH
#define NS_PB_GAP_RAIL (NS_PB_PITCH * 2)
#define NS_PB_GAP_TRENCH (NS_PB_PITCH * 2)
#define NS_PB_HOLE_INSET ((NS_PB_PITCH - NS_PB_HOLE) / 2)
#define NS_PB_RAIL_GROUP 5
#define NS_PB_RAIL_GROUPS 10
#define NS_PB_RAIL_GAP_COL 30
#define NS_PB_RAIL_GAP_LEN 3
#define NS_PB_RAIL_HALF (NS_PB_RAIL_GROUPS * (NS_PB_RAIL_GROUP + 1) / 2)
#else
#define NS_PB_HOLE 1
#define NS_PB_GAP 4
#define NS_PB_PITCH (NS_PB_HOLE + NS_PB_GAP)
#define NS_PB_MARGIN NS_PB_PITCH
#define NS_PB_GAP_RAIL NS_PB_PITCH
#define NS_PB_GAP_TRENCH (NS_PB_PITCH * 2)
#define NS_PB_HOLE_INSET 0
#define NS_PB_RAIL_GROUP 4
#endif
#define NS_PB_RAIL_SEG 25
#define NS_PB_RAIL_GAP_END (NS_PB_COLS - NS_PB_RAIL_SEG)
#define NS_PB_STRIPS (NS_PB_COLS * 2 + 8)

enum {
    NS_PB_LANE_TOP_POS = 0,
    NS_PB_LANE_TOP_NEG = 1,
    NS_PB_LANE_A = 2,
    NS_PB_LANE_E = 6,
    NS_PB_LANE_F = 7,
    NS_PB_LANE_J = 11,
    NS_PB_LANE_BOT_POS = 12,
    NS_PB_LANE_BOT_NEG = 13,
    NS_PB_LANE_COUNT = 14
};

typedef struct NsPbHole {
    int col;
    int lane;
} NsPbHole;

typedef struct NsBreadboard {
    NsEntity base;
    int hover_valid;
    NsPbHole hover;
    char refdes_buf[12];
} NsBreadboard;

void ns_breadboard_init(NsBreadboard *bb, const char *refdes);
NsEntity *ns_breadboard_entity(NsBreadboard *bb);

/* Sync body_w/body_h from current orient (call after rotate). */
void ns_breadboard_sync_body(NsBreadboard *bb);

void ns_breadboard_body_size(NsPkgOrient orient, int *w, int *h);
int ns_breadboard_hole_exists(NsPbHole h);
int ns_breadboard_strip_id(NsPbHole h);

/* Hole center in board canvas coords (same space as entity board_x/y). */
void ns_breadboard_hole_world(const NsBreadboard *bb, NsPbHole h, int *wx, int *wy);

/* Hit-test hole under board-space point. */
int ns_breadboard_hit_hole(const NsBreadboard *bb, int wx, int wy, NsPbHole *out);

void ns_breadboard_set_hover(NsBreadboard *bb, int valid, NsPbHole h);
void ns_breadboard_clear_hover(NsBreadboard *bb);

/* 1 if board-space point is exactly on an existing hole (returns strip_id). */
int ns_breadboard_tip_strip(const NsBreadboard *bb, int wx, int wy, int *strip_out);

/* screen_x/y = screen position of entity top-left (after pan). */
void ns_breadboard_draw(SDL_Renderer *r, const NsBreadboard *bb, int screen_x, int screen_y, int selected);
void ns_breadboard_draw_power(SDL_Renderer *r, const NsBreadboard *bb, int screen_x, int screen_y, int selected,
                              int pos_rail_power, int neg_rail_power);

#endif

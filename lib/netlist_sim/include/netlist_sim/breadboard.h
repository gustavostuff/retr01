#ifndef NETLIST_SIM_BREADBOARD_H
#define NETLIST_SIM_BREADBOARD_H

#include "netlist_sim/entity.h"

#include <SDL.h>

/*
 * Solderless breadboard visual (nano protoboard layout).
 * Drag / rotate / hover rail highlight only. No electrical link to ICs yet.
 *
 * 63 terminal columns x 10 + 4 power rails (830-point class).
 *
 * Lattice: 1 px hole + 4 px gap = 5 px pitch (same as NS_DIP_PIN_PITCH_PX).
 * Trench gap is 2 pitches so E-to-F hole centers are 15 px (300 mil DIP tip span).
 */
#define NS_PB_COLS 63
#define NS_PB_HOLE 1
#define NS_PB_GAP 4
#define NS_PB_PITCH (NS_PB_HOLE + NS_PB_GAP)
#define NS_PB_MARGIN NS_PB_PITCH
#define NS_PB_GAP_RAIL NS_PB_PITCH
#define NS_PB_GAP_TRENCH (NS_PB_PITCH * 2)
#define NS_PB_RAIL_SEG 25
#define NS_PB_RAIL_GAP_END (NS_PB_COLS - NS_PB_RAIL_SEG)
#define NS_PB_RAIL_GROUP 4

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

#endif

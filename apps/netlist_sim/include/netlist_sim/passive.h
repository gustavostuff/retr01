#ifndef NETLIST_SIM_PASSIVE_H
#define NETLIST_SIM_PASSIVE_H

#include "netlist_sim/entity.h"

#include <SDL.h>

/*
 * Passives (R / CCAP / ECAP / OSC / OSC4LEGS / D). UI sprites only.
 * Filename KIND_x_y.png is the canvas pivot. 2-pin parts: pin 1 = pivot, pin 2 at +span.
 * OSC4LEGS (DIP-14 metal can): pivot = pin 14 VDD. Pin 8 OUT, pin 1 OE#, pin 7 GND.
 * Polarized: ECAP pin 1 is - (pivot), pin 2 is +. Diode pin 1 is A (pivot), pin 2 is K.
 */

typedef enum NsPassiveKind {
    NS_PASSIVE_R = 0,
    NS_PASSIVE_CCAP,
    NS_PASSIVE_ECAP,
    NS_PASSIVE_OSC,
    NS_PASSIVE_OSC4LEGS,
    NS_PASSIVE_D,
    NS_PASSIVE_KIND_COUNT
} NsPassiveKind;

#define NS_PASSIVE_MAX 96
#define NS_PASSIVE_VALUE_LEN 24
#define NS_PASSIVE_REF_LEN 12

typedef struct NsPassive {
    NsEntity base;
    NsPassiveKind kind;
    int polarized; /* 1 = ECAP or diode */
    int pivot_x;   /* filename pivot tip in board canvas coords */
    int pivot_y;
    int leg_ext[2]; /* extra px beyond default tips (pin 1, pin 2). Resistors only. */
    char value[NS_PASSIVE_VALUE_LEN];
    char refdes_buf[NS_PASSIVE_REF_LEN];
} NsPassive;

typedef struct NsPassiveBank {
    NsPassive parts[NS_PASSIVE_MAX];
    int count;
} NsPassiveBank;

void ns_passive_bank_clear(NsPassiveBank *bank);

/* Append one part. refdes/value copied into the part. Returns pointer or NULL. */
NsPassive *ns_passive_bank_add(NsPassiveBank *bank, NsPassiveKind kind, const char *refdes,
                                   const char *value);

void ns_passive_sync_aabb(NsPassive *p);
void ns_passive_set_pivot(NsPassive *p, int pivot_x, int pivot_y);
void ns_passive_set_orient(NsPassive *p, NsPkgOrient orient);
void ns_passive_set_leg_ext(NsPassive *p, int pin_num, int extra);
void ns_passive_set_leg_to(NsPassive *p, int pin_num, int wx, int wy);

/* Opaque PNG pixel at board (bx,by), after rotation about the filename pivot. */
int ns_passive_hit(const NsPassive *p, int bx, int by);

/* Tip of pin_num in board canvas coords. OSC4LEGS uses DIP-14 can numbers. */
int ns_passive_tip_board(const NsPassive *p, int pin_num, int *wx, int *wy);

/* screen_pivot_* = pan-adjusted board coords of the filename pivot. */
void ns_passive_draw(SDL_Renderer *r, const NsPassive *p, int screen_pivot_x, int screen_pivot_y,
                       int selected);
void ns_passive_draw_kind(SDL_Renderer *r, NsPassiveKind kind, NsPkgOrient orient, int screen_pivot_x,
                         int screen_pivot_y, int selected);

/* DOT/FSC cans use the OSC4LEGS sprite. board_x/y is AABB top-left. */
int ns_osc4legs_chip_tip(const NsEntity *e, int pin_num, int *wx, int *wy);
void ns_osc4legs_sync_aabb(NsEntity *e);
void ns_osc4legs_set_orient(NsEntity *e, NsPkgOrient orient);
int ns_osc4legs_hit(const NsEntity *e, int bx, int by);

/* Spawn full console passive BOM . Ordered by kind then value. */
int ns_passive_bank_spawn_bom(NsPassiveBank *bank);

/* Pack loose parts in a grid at (origin_x, origin_y), no overlap, pitch-spaced. */
void ns_passive_bank_layout_grid(NsPassiveBank *bank, int origin_x, int origin_y, int row_gap,
                                   int col_gap);

const char *ns_passive_kind_name(NsPassiveKind kind);

#endif

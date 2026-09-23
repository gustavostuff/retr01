#ifndef NETLIST_SIM_PASSIVE_H
#define NETLIST_SIM_PASSIVE_H

#include "netlist_sim/entity.h"

#include <SDL.h>

/*
 * Passives (R / CCAP / ECAP / OSC / D). UI sprites only.
 * No netlist wiring yet. Pivot = pin 1 tip from KIND_x_y.png filename.
 * Polarized parts (ECAP, later D) carry a flag for a future polarity check.
 */

typedef enum NsPassiveKind {
    NS_PASSIVE_R = 0,
    NS_PASSIVE_CCAP,
    NS_PASSIVE_ECAP,
    NS_PASSIVE_OSC,
    NS_PASSIVE_D,
    NS_PASSIVE_KIND_COUNT
} NsPassiveKind;

#define NS_PASSIVE_MAX 96
#define NS_PASSIVE_VALUE_LEN 24
#define NS_PASSIVE_REF_LEN 12

typedef struct NsPassive {
    NsEntity base;
    NsPassiveKind kind;
    int polarized; /* 1 = ECAP (and future diode). Polarity rules later. */
    int pivot_x;   /* pin-1 tip in board canvas coords */
    int pivot_y;
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

/* Pin 1 = pivot. Pin 2 = pivot + rotated span. */
int ns_passive_tip_board(const NsPassive *p, int pin_num, int *wx, int *wy);

/* screen_pivot_* = pan-adjusted board coords of pin-1 tip. */
void ns_passive_draw(SDL_Renderer *r, const NsPassive *p, int screen_pivot_x, int screen_pivot_y,
                       int selected);

/* Spawn full console passive BOM . Ordered by kind then value. */
int ns_passive_bank_spawn_bom(NsPassiveBank *bank);

/* Pack loose parts in a grid at (origin_x, origin_y), no overlap, pitch-spaced. */
void ns_passive_bank_layout_grid(NsPassiveBank *bank, int origin_x, int origin_y, int row_gap,
                                   int col_gap);

const char *ns_passive_kind_name(NsPassiveKind kind);

#endif

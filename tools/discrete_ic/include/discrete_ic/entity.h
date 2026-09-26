#ifndef DISCRETE_IC_ENTITY_H
#define DISCRETE_IC_ENTITY_H

#include "discrete_ic/health.h"
#include "discrete_ic/pin.h"

/*
 * Base chip entity. Concrete ICs "inherit" by:
 *   1. Embedding or pointing NsEntity as the first/header object
 *   2. Providing an NsEntityVTable
 *   3. Storing part-specific state in impl (or a wider struct)
 *
 * UI uses part/refdes/pins/body + board_x/y to draw packages.
 * Sim core will call reset/eval/tick through the vtable.
 */

typedef struct NsEntity NsEntity;

typedef struct NsEntityVTable {
    /* Power-on / RESB-style reset. */
    void (*reset)(NsEntity *e);
    /* Combinatorial / level-sensitive update. */
    void (*eval)(NsEntity *e);
    /* Clock-edge or discrete time step (PHI2, etc.). */
    void (*tick)(NsEntity *e);
    /* Free impl + any owned resources (not the NsEntity shell itself). */
    void (*destroy)(NsEntity *e);
} NsEntityVTable;

/* Board canvas rendering: only NS_ENTITY_VIS_IC uses the DIP package. */
typedef enum NsEntityVisual {
    NS_ENTITY_VIS_IC = 0,
    NS_ENTITY_VIS_PWR,
    NS_ENTITY_VIS_OSC,
    NS_ENTITY_VIS_DISPLAY,
    NS_ENTITY_VIS_BUTTON,
    NS_ENTITY_VIS_PANEL, /* labeled connector / host block (not LCD) */
    NS_ENTITY_VIS_BREADBOARD,
    NS_ENTITY_VIS_PASSIVE, /* R / CCAP / ECAP / OSC / D sprites */
    NS_ENTITY_VIS_NONE,
} NsEntityVisual;

/* Package drawing orientation (DIP body). Four 90-degree steps, CW from default.
 * Pin 1 sits by the notch. Tip pivot for rotate/snap is pin.png col 1, row 0. */
typedef enum NsPkgOrient {
    NS_ORIENT_0 = 0,   /* length along X; notch west; pin1 south */
    NS_ORIENT_90 = 1,  /* length along Y; notch north; pin1 west */
    NS_ORIENT_180 = 2, /* length along X; notch east; pin1 north */
    NS_ORIENT_270 = 3, /* length along Y; notch south; pin1 east */
    NS_ORIENT_H = NS_ORIENT_0, /* legacy alias */
    NS_ORIENT_V = NS_ORIENT_90 /* legacy alias */
} NsPkgOrient;

static inline int ns_orient_is_horiz(NsPkgOrient o) {
    return o == NS_ORIENT_0 || o == NS_ORIENT_180;
}

static inline NsPkgOrient ns_orient_next_cw(NsPkgOrient o) {
    return (NsPkgOrient)(((int)o + 1) & 3);
}

#define NS_PIN_HASH_SIZE 96 /* > max pins/entity; built once, hot-path lookups */

struct NsEntity {
    const NsEntityVTable *vt;
    const char *part;   /* e.g. "W65C02S" */
    const char *refdes; /* e.g. "U1" */
    NsEntityVisual visual;
    int pin_count;
    NsPin pins[NS_MAX_PINS];
    /* Lazy pin-name index: pin_hash_idx[slot] -> pins[] index, -1 empty. */
    uint8_t pin_hash_built;
    int8_t pin_hash_idx[NS_PIN_HASH_SIZE];
    /* DIP body size in logical pixels (UI): from pkg mm x scale + orient. */
    int body_w;
    int body_h;
    /* Physical DIP pin count (e.g. 40 for W65C02S); used for package layout. */
    int dip_pins;
    /* Molded body outline in mm (see hw/md/packages_dip.md). */
    int pkg_len_mm;
    int pkg_wid_mm;
    NsPkgOrient orient;
    /* Top-left of package body on the board canvas. */
    int board_x;
    int board_y;
    /* Outline color for viz: OK=green, WARN=yellow, FAIL=red, BOOT=neutral. */
    NsHealth health;
    void *impl;
};

/* Canvas scale: 1 mm real package = 2 logical pixels (body_px / 2 -> mm). */
#define NS_PX_PER_MM 2
/* JEDEC 0.100" pin pitch = 2.54 mm -> round(2.54 * NS_PX_PER_MM) = 5 px.
 * Same lattice as breadboard: 1 px hole + 4 px gap. */
#define NS_DIP_PIN_PITCH_PX ((254 * NS_PX_PER_MM + 50) / 100)
/* Soft hint only. Draw centers the pin row in leftover body length. */
#define NS_DIP_PIN_MARGIN_PX 2

/* Look up (or estimate) molded body length x width in mm for a DIP pin count. */
void ns_dip_pkg_mm(int dip_pins, int *len_mm, int *wid_mm);

/* Body size along the pin-row axis in px (horizontal body_w / vertical body_h). */
int ns_dip_body_along_px(int dip_pins);
/* Body size across the pin rows in px (snapped so opposing pin tips land on pitch). */
int ns_dip_body_across_px(int dip_pins);
/* Snap across-body px so opposing pin tips land on the hole lattice.
 * 300 mil class -> tip span 20 (across 15). 600 mil -> tip span 35 (across 30).
 * One extra breadboard pitch vs JEDEC so labels fit. */
int ns_dip_snap_across_px(int across_px);

/* Zero entity; set vtable/part/refdes; pins start empty. */
void ns_entity_init(NsEntity *e, const NsEntityVTable *vt, const char *part, const char *refdes);

/* Append a pin (fails if pin_count == NS_MAX_PINS). Returns 0 ok, -1 full. */
int ns_entity_add_pin(NsEntity *e, int number, const char *name, NsPinDir dir);

/*
 * DIP body from JEDEC-class / family mm outline at NS_PX_PER_MM.
 * Default orientation is horizontal (labels read left-to-right).
 */
void ns_entity_set_dip(NsEntity *e, int dip_pins);

/* Same as set_dip, but with an explicit molded body length x width (mm). */
void ns_entity_set_dip_mm(NsEntity *e, int dip_pins, int len_mm, int wid_mm);

/* Toggle / set package orientation; refreshes body_w/body_h. */
void ns_entity_set_orient(NsEntity *e, NsPkgOrient orient);

/* Recompute body_w/body_h from pkg_*_mm and orient. */
void ns_entity_refresh_body(NsEntity *e);

/* Non-DIP board symbols (power, crystal, LCD). dip_pins cleared. */
void ns_entity_set_glyph(NsEntity *e, NsEntityVisual visual, int body_w, int body_h);

void ns_entity_place(NsEntity *e, int board_x, int board_y);

void ns_entity_reset(NsEntity *e);
void ns_entity_eval(NsEntity *e);
void ns_entity_tick(NsEntity *e);
void ns_entity_destroy(NsEntity *e);

/* Find pin by package number (1-based). NULL if missing. */
NsPin *ns_entity_pin(NsEntity *e, int number);
const NsPin *ns_entity_pin_const(const NsEntity *e, int number);

/* Build pin_hash_idx once (safe to call repeatedly). */
void ns_entity_pin_hash_build(NsEntity *e);

NsPin *ns_entity_pin_named(NsEntity *e, const char *name);
const NsPin *ns_entity_pin_named_const(const NsEntity *e, const char *name);

/* Pin.png tip (col 1, row 0) in board canvas coords. DIP packages, PWR glyphs, OSC4LEGS cans. */
int ns_entity_pin_tip_board(const NsEntity *e, int pin_num, int *tbx, int *tby);

#endif

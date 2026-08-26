#ifndef retr01_SIM_ENTITY_H
#define retr01_SIM_ENTITY_H

#include "retr01_sim/pin.h"

/*
 * Base chip entity. Concrete ICs "inherit" by:
 *   1. Embedding or pointing R01sEntity as the first/header object
 *   2. Providing an R01sEntityVTable
 *   3. Storing part-specific state in impl (or a wider struct)
 *
 * UI uses part/refdes/pins/body + board_x/y to draw packages.
 * Sim core will call reset/eval/tick through the vtable.
 */

typedef struct R01sEntity R01sEntity;

typedef struct R01sEntityVTable {
    /* Power-on / RESB-style reset. */
    void (*reset)(R01sEntity *e);
    /* Combinatorial / level-sensitive update. */
    void (*eval)(R01sEntity *e);
    /* Clock-edge or discrete time step (PHI2, etc.). */
    void (*tick)(R01sEntity *e);
    /* Free impl + any owned resources (not the R01sEntity shell itself). */
    void (*destroy)(R01sEntity *e);
} R01sEntityVTable;

/* Board canvas rendering: only R01S_ENTITY_VIS_IC uses the DIP package. */
typedef enum R01sEntityVisual {
    R01S_ENTITY_VIS_IC = 0,
    R01S_ENTITY_VIS_PWR,
    R01S_ENTITY_VIS_OSC,
    R01S_ENTITY_VIS_DISPLAY,
    R01S_ENTITY_VIS_NONE,
} R01sEntityVisual;

/* Package drawing orientation (DIP body). */
typedef enum R01sPkgOrient {
    R01S_ORIENT_H = 0, /* length along X; pins top/bottom; notch left */
    R01S_ORIENT_V = 1, /* length along Y; pins left/right; notch top */
} R01sPkgOrient;

struct R01sEntity {
    const R01sEntityVTable *vt;
    const char *part;   /* e.g. "W65C02S" */
    const char *refdes; /* e.g. "U1" */
    R01sEntityVisual visual;
    int pin_count;
    R01sPin pins[R01S_MAX_PINS];
    /* DIP body size in logical pixels (UI): from pkg mm × scale + orient. */
    int body_w;
    int body_h;
    /* Physical DIP pin count (e.g. 40 for W65C02S); used for package layout. */
    int dip_pins;
    /* Molded body outline in mm (see hw/md/packages_dip.md). */
    int pkg_len_mm;
    int pkg_wid_mm;
    R01sPkgOrient orient;
    /* Top-left of package body on the board canvas. */
    int board_x;
    int board_y;
    void *impl;
};

/* Canvas scale: 1 mm real package = 4 logical pixels (compact bbox ÷ 4 → mm). */
#define R01S_PX_PER_MM 4
/* JEDEC 0.100″ pin pitch ≈ 2.54 mm → 10 px at R01S_PX_PER_MM (never stretch-to-fill). */
#define R01S_DIP_PIN_PITCH_PX 10
#define R01S_DIP_PIN_MARGIN_PX 8 /* soft hint only; draw centers row with leftover */

/* Look up (or estimate) molded body length × width in mm for a DIP pin count. */
void r01s_dip_pkg_mm(int dip_pins, int *len_mm, int *wid_mm);

/* Body size along the pin-row axis in px (horizontal body_w / vertical body_h). */
int r01s_dip_body_along_px(int dip_pins);
/* Body size across the pin rows in px. */
int r01s_dip_body_across_px(int dip_pins);

/* Zero entity; set vtable/part/refdes; pins start empty. */
void r01s_entity_init(R01sEntity *e, const R01sEntityVTable *vt, const char *part, const char *refdes);

/* Append a pin (fails if pin_count == R01S_MAX_PINS). Returns 0 ok, -1 full. */
int r01s_entity_add_pin(R01sEntity *e, int number, const char *name, R01sPinDir dir);

/*
 * DIP body from JEDEC-class / family mm outline at R01S_PX_PER_MM.
 * Default orientation is horizontal (labels read left-to-right).
 */
void r01s_entity_set_dip(R01sEntity *e, int dip_pins);

/* Same as set_dip, but with an explicit molded body length × width (mm). */
void r01s_entity_set_dip_mm(R01sEntity *e, int dip_pins, int len_mm, int wid_mm);

/* Toggle / set package orientation; refreshes body_w/body_h. */
void r01s_entity_set_orient(R01sEntity *e, R01sPkgOrient orient);

/* Recompute body_w/body_h from pkg_*_mm and orient. */
void r01s_entity_refresh_body(R01sEntity *e);

/* Non-DIP board symbols (power, crystal, LCD). dip_pins cleared. */
void r01s_entity_set_glyph(R01sEntity *e, R01sEntityVisual visual, int body_w, int body_h);

void r01s_entity_place(R01sEntity *e, int board_x, int board_y);

void r01s_entity_reset(R01sEntity *e);
void r01s_entity_eval(R01sEntity *e);
void r01s_entity_tick(R01sEntity *e);
void r01s_entity_destroy(R01sEntity *e);

/* Find pin by package number (1-based). NULL if missing. */
R01sPin *r01s_entity_pin(R01sEntity *e, int number);
const R01sPin *r01s_entity_pin_const(const R01sEntity *e, int number);

#endif

#ifndef RETR01_SIM_ENTITY_H
#define RETR01_SIM_ENTITY_H

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

struct R01sEntity {
    const R01sEntityVTable *vt;
    const char *part;   /* e.g. "W65C02S" */
    const char *refdes; /* e.g. "U1" */
    int pin_count;
    R01sPin pins[R01S_MAX_PINS];
    /* DIP body size in logical pixels (UI). */
    int body_w;
    int body_h;
    /* Physical DIP pin count (e.g. 8 for OSC8M); used for package layout. */
    int dip_pins;
    /* Top-left of package body on the board canvas. */
    int board_x;
    int board_y;
    void *impl;
};

/* Zero entity; set vtable/part/refdes; pins start empty. */
void r01s_entity_init(R01sEntity *e, const R01sEntityVTable *vt, const char *part, const char *refdes);

/* Append a pin (fails if pin_count == R01S_MAX_PINS). Returns 0 ok, -1 full. */
int r01s_entity_add_pin(R01sEntity *e, int number, const char *name, R01sPinDir dir);

/* DIP body helpers: dual-row package, pin 1 top-left. dip_pins is the package pin count. */
void r01s_entity_set_dip(R01sEntity *e, int dip_pins, int body_w, int body_h);

void r01s_entity_place(R01sEntity *e, int board_x, int board_y);

void r01s_entity_reset(R01sEntity *e);
void r01s_entity_eval(R01sEntity *e);
void r01s_entity_tick(R01sEntity *e);
void r01s_entity_destroy(R01sEntity *e);

/* Find pin by package number (1-based). NULL if missing. */
R01sPin *r01s_entity_pin(R01sEntity *e, int number);
const R01sPin *r01s_entity_pin_const(const R01sEntity *e, int number);

#endif

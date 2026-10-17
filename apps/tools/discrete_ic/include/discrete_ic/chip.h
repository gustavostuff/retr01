#ifndef DISCRETE_IC_CHIP_H
#define DISCRETE_IC_CHIP_H

#include "discrete_ic/entity.h"

#include <stddef.h>

/*
 * String-keyed chip registry and helpers.
 * Part identity is always a string (e.g. "SN74HC00"), never a per-part C symbol.
 */

typedef void (*NsChipFn)(NsEntity *e);

typedef struct NsChipPinDef {
    int number;
    const char *name;
    NsPinDir dir;
} NsChipPinDef;

typedef struct NsChipClass {
    const char *part;
    int dip_pins;
    int len_mm; /* 0 = use JEDEC default for dip_pins */
    int wid_mm;
    const NsChipPinDef *pins;
    int pin_count;
    NsChipFn reset;
    NsChipFn eval;
    NsChipFn tick;
    NsChipFn destroy;
    size_t impl_size; /* extra bytes after NsEntity when using ns_chip_create */
} NsChipClass;

/* Register or replace a part definition. Returns 0 ok, -1 on error. */
int ns_chip_register(const NsChipClass *cls);

const NsChipClass *ns_chip_lookup(const char *part);

/*
 * Allocate entity (+ optional impl_size) for a registered part.
 * Copies pin list, sets DIP, installs vtable from the class.
 * Caller owns the pointer (free with ns_chip_destroy).
 */
NsEntity *ns_chip_create(const char *part, const char *refdes);

void ns_chip_destroy(NsEntity *e);

/* Manual build without registry (still string part name). */
NsEntity *ns_chip_create_blank(const char *part, const char *refdes, size_t impl_size);
void ns_chip_set_callbacks(NsEntity *e, NsChipFn reset, NsChipFn eval, NsChipFn tick, NsChipFn destroy);

#endif

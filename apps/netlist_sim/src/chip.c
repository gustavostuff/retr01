#include "netlist_sim/chip.h"

#include <stdlib.h>
#include <string.h>

#define NS_CHIP_REG_MAX 64

typedef struct NsChipSlot {
    NsChipClass cls;
    NsEntityVTable vt;
    int used;
} NsChipSlot;

static NsChipSlot g_reg[NS_CHIP_REG_MAX];

static NsChipSlot *slot_for_part(const char *part, int create) {
    int i;
    int free_i = -1;
    if (!part || !part[0]) {
        return NULL;
    }
    for (i = 0; i < NS_CHIP_REG_MAX; i++) {
        if (!g_reg[i].used) {
            if (free_i < 0) {
                free_i = i;
            }
            continue;
        }
        if (strcmp(g_reg[i].cls.part, part) == 0) {
            return &g_reg[i];
        }
    }
    if (!create || free_i < 0) {
        return NULL;
    }
    g_reg[free_i].used = 1;
    memset(&g_reg[free_i].cls, 0, sizeof(g_reg[free_i].cls));
    memset(&g_reg[free_i].vt, 0, sizeof(g_reg[free_i].vt));
    return &g_reg[free_i];
}

int ns_chip_register(const NsChipClass *cls) {
    NsChipSlot *s;
    if (!cls || !cls->part) {
        return -1;
    }
    s = slot_for_part(cls->part, 1);
    if (!s) {
        return -1;
    }
    s->cls = *cls;
    s->vt.reset = cls->reset;
    s->vt.eval = cls->eval;
    s->vt.tick = cls->tick;
    s->vt.destroy = cls->destroy;
    return 0;
}

const NsChipClass *ns_chip_lookup(const char *part) {
    NsChipSlot *s = slot_for_part(part, 0);
    return s ? &s->cls : NULL;
}

static NsEntity *alloc_entity(size_t impl_size) {
    size_t n = sizeof(NsEntity) + impl_size;
    NsEntity *e = (NsEntity *)calloc(1, n);
    if (!e) {
        return NULL;
    }
    if (impl_size > 0) {
        e->impl = (void *)(e + 1);
    }
    return e;
}

NsEntity *ns_chip_create_blank(const char *part, const char *refdes, size_t impl_size) {
    NsEntity *e = alloc_entity(impl_size);
    if (!e) {
        return NULL;
    }
    ns_entity_init(e, NULL, part, refdes);
    e->health = NS_HEALTH_OK;
    return e;
}

void ns_chip_set_callbacks(NsEntity *e, NsChipFn reset, NsChipFn eval, NsChipFn tick, NsChipFn destroy) {
    NsEntityVTable *vt;
    if (!e) {
        return;
    }
    /* Store vtable in impl if blank, else require registry path. Simple: heap vt. */
    vt = (NsEntityVTable *)calloc(1, sizeof(NsEntityVTable));
    if (!vt) {
        return;
    }
    vt->reset = reset;
    vt->eval = eval;
    vt->tick = tick;
    vt->destroy = destroy;
    e->vt = vt;
}

NsEntity *ns_chip_create(const char *part, const char *refdes) {
    const NsChipClass *cls = ns_chip_lookup(part);
    NsChipSlot *s = slot_for_part(part, 0);
    NsEntity *e;
    int i;
    if (!cls || !s) {
        return NULL;
    }
    e = alloc_entity(cls->impl_size);
    if (!e) {
        return NULL;
    }
    ns_entity_init(e, &s->vt, cls->part, refdes);
    e->health = NS_HEALTH_OK;
    if (cls->len_mm > 0 && cls->wid_mm > 0) {
        ns_entity_set_dip_mm(e, cls->dip_pins, cls->len_mm, cls->wid_mm);
    } else if (cls->dip_pins > 0) {
        ns_entity_set_dip(e, cls->dip_pins);
    }
    if (cls->pins && cls->pin_count > 0) {
        for (i = 0; i < cls->pin_count; i++) {
            ns_entity_add_pin(e, cls->pins[i].number, cls->pins[i].name, cls->pins[i].dir);
        }
    }
    return e;
}

void ns_chip_destroy(NsEntity *e) {
    if (!e) {
        return;
    }
    ns_entity_destroy(e);
    free(e);
}

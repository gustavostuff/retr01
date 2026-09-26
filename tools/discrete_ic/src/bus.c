#include "discrete_ic/bus.h"

#include "discrete_ic/entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_bus_conflicts;
static int g_fatal_conflicts = 0;

void ns_bus_set_fatal_conflicts(int enable) {
    g_fatal_conflicts = enable ? 1 : 0;
}

int ns_bus_fatal_conflicts(void) {
    return g_fatal_conflicts;
}

unsigned ns_bus_conflict_count(void) {
    return g_bus_conflicts;
}

void ns_bus_clear_conflicts(void) {
    g_bus_conflicts = 0;
}

const char *ns_level_name(NsLevel level) {
    switch (level) {
    case NS_LVL_Z:
        return "Z";
    case NS_LVL_L:
        return "L";
    case NS_LVL_H:
        return "H";
    case NS_LVL_X:
        return "X";
    default:
        return "?";
    }
}

static const char *entity_label(const NsEntity *e) {
    static char buf[64];
    const char *ref = (e && e->refdes) ? e->refdes : "?";
    const char *part = (e && e->part) ? e->part : "?";
    snprintf(buf, sizeof(buf), "%s (%s)", ref, part);
    return buf;
}

static void bus_fight_abort(const char *net, const char *driver_a, NsLevel a, const char *driver_b,
                            NsLevel b, const char *why) {
    g_bus_conflicts++;
    fprintf(stderr, "\n");
    fprintf(stderr, "board_sim: BUS FIGHT -- aborting (fatal conflicts enabled)\n");
    fprintf(stderr, "  net:      %s\n", net ? net : "(unknown)");
    if (driver_a) {
        fprintf(stderr, "  driver A: %s = %s\n", driver_a, ns_level_name(a));
    } else {
        fprintf(stderr, "  level A:  %s\n", ns_level_name(a));
    }
    if (driver_b) {
        fprintf(stderr, "  driver B: %s = %s\n", driver_b, ns_level_name(b));
    } else {
        fprintf(stderr, "  level B:  %s\n", ns_level_name(b));
    }
    fprintf(stderr, "  why:      %s\n", why ? why : "conflicting drive levels on one net");
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(1);
}

static int levels_conflict(NsLevel a, NsLevel b) {
    if (a == NS_LVL_Z || b == NS_LVL_Z) {
        return 0;
    }
    if (a == NS_LVL_X || b == NS_LVL_X) {
        return 1;
    }
    return a != b;
}

void ns_entity_drive(NsEntity *e, const char *name, NsLevel level) {
    NsPin *p = ns_entity_pin_named(e, name);
    if (p) {
        p->level = level;
    }
}

NsLevel ns_entity_sense(const NsEntity *e, const char *name) {
    const NsPin *p = ns_entity_pin_named_const(e, name);
    return p ? p->level : NS_LVL_Z;
}

int ns_level_is_low(NsLevel level) {
    return level == NS_LVL_L;
}

int ns_level_is_high(NsLevel level) {
    return level == NS_LVL_H;
}

NsLevel ns_level_merge_at(NsLevel a, NsLevel b, const char *net, const char *driver_a,
                              const char *driver_b) {
    if (a == NS_LVL_Z) {
        return b;
    }
    if (b == NS_LVL_Z) {
        return a;
    }
    if (!levels_conflict(a, b)) {
        return a;
    }
    if (g_fatal_conflicts) {
        const char *why;
        if (a == NS_LVL_X || b == NS_LVL_X) {
            why = "net already in unknown/conflict (X) state while a second driver is active";
        } else {
            why = "two outputs driving opposite levels (H vs L). "
                  "Usually a chip-select / OE# decode bug so two devices share the bus";
        }
        bus_fight_abort(net, driver_a, a, driver_b, b, why);
    }
    g_bus_conflicts++;
    return NS_LVL_X;
}

NsLevel ns_level_merge(NsLevel a, NsLevel b) {
    return ns_level_merge_at(a, b, "(unnamed net)", NULL, NULL);
}

NsLevel ns_level_pulled(NsLevel level) {
    if (level == NS_LVL_Z) {
        return NS_LVL_H;
    }
    return level;
}

void ns_bus_write(NsEntity *e, const char *prefix, int width, uint32_t value) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        ns_entity_drive(e, name, (value & (1u << i)) ? NS_LVL_H : NS_LVL_L);
    }
}

uint32_t ns_bus_read(const NsEntity *e, const char *prefix, int width) {
    int i;
    uint32_t v = 0;
    char name[16];
    char where[80];
    if (!e || !prefix || width <= 0) {
        return 0;
    }
    for (i = 0; i < width; i++) {
        NsLevel raw;
        NsLevel lvl;
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        raw = ns_entity_sense(e, name);
        if (raw == NS_LVL_X) {
            snprintf(where, sizeof(where), "%s.%s", entity_label(e), name);
            if (g_fatal_conflicts) {
                bus_fight_abort(where, NULL, NS_LVL_X, NULL, NS_LVL_X,
                                "read of a pin already in conflict (X). "
                                "A prior multi-drive left the net invalid");
            }
            g_bus_conflicts++;
        }
        lvl = ns_level_pulled(raw);
        if (ns_level_is_high(lvl)) {
            v |= (1u << i);
        }
    }
    return v;
}

void ns_bus_hiz(NsEntity *e, const char *prefix, int width) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        ns_entity_drive(e, name, NS_LVL_Z);
    }
}

void ns_bus_resolve(NsEntity *dst, const char *dst_prefix, const NsEntity *a, const char *a_prefix,
                      const NsEntity *b, const char *b_prefix, int width) {
    int i;
    char dn[16], an[16], bn[16];
    char net[128];
    char da[96], db[96];
    char la[64], lb[64], ld[64];

    if (!dst || !dst_prefix || !a || !a_prefix || !b || !b_prefix || width <= 0) {
        return;
    }

    /* Snapshot labels once -- entity_label uses a static buffer. */
    snprintf(la, sizeof(la), "%s", entity_label(a));
    snprintf(lb, sizeof(lb), "%s", entity_label(b));
    snprintf(ld, sizeof(ld), "%s", entity_label(dst));

    for (i = 0; i < width; i++) {
        NsLevel la_lvl, lb_lvl;
        snprintf(dn, sizeof(dn), "%s%d", dst_prefix, i);
        snprintf(an, sizeof(an), "%s%d", a_prefix, i);
        snprintf(bn, sizeof(bn), "%s%d", b_prefix, i);
        snprintf(net, sizeof(net), "%s (via %s.%s)", dn, ld, dn);
        snprintf(da, sizeof(da), "%s.%s", la, an);
        snprintf(db, sizeof(db), "%s.%s", lb, bn);
        la_lvl = ns_entity_sense(a, an);
        lb_lvl = ns_entity_sense(b, bn);
        ns_entity_drive(dst, dn, ns_level_merge_at(la_lvl, lb_lvl, net, da, db));
    }
}

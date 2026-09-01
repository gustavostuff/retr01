#include "retr01_sim/bus.h"

#include "retr01_sim/entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned g_bus_conflicts;
static int g_fatal_conflicts = 1;

void r01s_bus_set_fatal_conflicts(int enable) {
    g_fatal_conflicts = enable ? 1 : 0;
}

int r01s_bus_fatal_conflicts(void) {
    return g_fatal_conflicts;
}

unsigned r01s_bus_conflict_count(void) {
    return g_bus_conflicts;
}

void r01s_bus_clear_conflicts(void) {
    g_bus_conflicts = 0;
}

const char *r01s_level_name(R01sLevel level) {
    switch (level) {
    case R01S_LVL_Z:
        return "Z";
    case R01S_LVL_L:
        return "L";
    case R01S_LVL_H:
        return "H";
    case R01S_LVL_X:
        return "X";
    default:
        return "?";
    }
}

static const char *entity_label(const R01sEntity *e) {
    static char buf[64];
    const char *ref = (e && e->refdes) ? e->refdes : "?";
    const char *part = (e && e->part) ? e->part : "?";
    snprintf(buf, sizeof(buf), "%s (%s)", ref, part);
    return buf;
}

static void bus_fight_abort(const char *net, const char *driver_a, R01sLevel a, const char *driver_b,
                            R01sLevel b, const char *why) {
    g_bus_conflicts++;
    fprintf(stderr, "\n");
    fprintf(stderr, "retr01_sim: BUS FIGHT -- simulation aborted\n");
    fprintf(stderr, "  net:      %s\n", net ? net : "(unknown)");
    if (driver_a) {
        fprintf(stderr, "  driver A: %s = %s\n", driver_a, r01s_level_name(a));
    } else {
        fprintf(stderr, "  level A:  %s\n", r01s_level_name(a));
    }
    if (driver_b) {
        fprintf(stderr, "  driver B: %s = %s\n", driver_b, r01s_level_name(b));
    } else {
        fprintf(stderr, "  level B:  %s\n", r01s_level_name(b));
    }
    fprintf(stderr, "  why:      %s\n", why ? why : "conflicting drive levels on one net");
    fprintf(stderr, "\n");
    fflush(stderr);
    exit(1);
}

static int levels_conflict(R01sLevel a, R01sLevel b) {
    if (a == R01S_LVL_Z || b == R01S_LVL_Z) {
        return 0;
    }
    if (a == R01S_LVL_X || b == R01S_LVL_X) {
        return 1;
    }
    return a != b;
}

void r01s_entity_drive(R01sEntity *e, const char *name, R01sLevel level) {
    R01sPin *p = r01s_entity_pin_named(e, name);
    if (p) {
        p->level = level;
    }
}

R01sLevel r01s_entity_sense(const R01sEntity *e, const char *name) {
    const R01sPin *p = r01s_entity_pin_named_const(e, name);
    return p ? p->level : R01S_LVL_Z;
}

int r01s_level_is_low(R01sLevel level) {
    return level == R01S_LVL_L;
}

int r01s_level_is_high(R01sLevel level) {
    return level == R01S_LVL_H;
}

R01sLevel r01s_level_merge_at(R01sLevel a, R01sLevel b, const char *net, const char *driver_a,
                              const char *driver_b) {
    if (a == R01S_LVL_Z) {
        return b;
    }
    if (b == R01S_LVL_Z) {
        return a;
    }
    if (!levels_conflict(a, b)) {
        return a;
    }
    if (g_fatal_conflicts) {
        const char *why;
        if (a == R01S_LVL_X || b == R01S_LVL_X) {
            why = "net already in unknown/conflict (X) state while a second driver is active";
        } else {
            why = "two outputs driving opposite levels (H vs L). "
                  "Usually a chip-select / OE# decode bug so two devices share the bus";
        }
        bus_fight_abort(net, driver_a, a, driver_b, b, why);
    }
    g_bus_conflicts++;
    return R01S_LVL_X;
}

R01sLevel r01s_level_merge(R01sLevel a, R01sLevel b) {
    return r01s_level_merge_at(a, b, "(unnamed net)", NULL, NULL);
}

R01sLevel r01s_level_pulled(R01sLevel level) {
    if (level == R01S_LVL_Z) {
        return R01S_LVL_H;
    }
    return level;
}

void r01s_bus_write(R01sEntity *e, const char *prefix, int width, uint32_t value) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, (value & (1u << i)) ? R01S_LVL_H : R01S_LVL_L);
    }
}

uint32_t r01s_bus_read(const R01sEntity *e, const char *prefix, int width) {
    int i;
    uint32_t v = 0;
    char name[16];
    char where[80];
    if (!e || !prefix || width <= 0) {
        return 0;
    }
    for (i = 0; i < width; i++) {
        R01sLevel raw;
        R01sLevel lvl;
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        raw = r01s_entity_sense(e, name);
        if (raw == R01S_LVL_X) {
            snprintf(where, sizeof(where), "%s.%s", entity_label(e), name);
            if (g_fatal_conflicts) {
                bus_fight_abort(where, NULL, R01S_LVL_X, NULL, R01S_LVL_X,
                                "read of a pin already in conflict (X). "
                                "A prior multi-drive left the net invalid");
            }
            g_bus_conflicts++;
        }
        lvl = r01s_level_pulled(raw);
        if (r01s_level_is_high(lvl)) {
            v |= (1u << i);
        }
    }
    return v;
}

void r01s_bus_hiz(R01sEntity *e, const char *prefix, int width) {
    int i;
    char name[16];
    if (!e || !prefix || width <= 0) {
        return;
    }
    for (i = 0; i < width; i++) {
        snprintf(name, sizeof(name), "%s%d", prefix, i);
        r01s_entity_drive(e, name, R01S_LVL_Z);
    }
}

void r01s_bus_resolve(R01sEntity *dst, const char *dst_prefix, const R01sEntity *a, const char *a_prefix,
                      const R01sEntity *b, const char *b_prefix, int width) {
    int i;
    char dn[16], an[16], bn[16];
    char net[96];
    char da[96], db[96];
    char la[64], lb[64];

    if (!dst || !dst_prefix || !a || !a_prefix || !b || !b_prefix || width <= 0) {
        return;
    }

    /* Snapshot labels once -- entity_label uses a static buffer. */
    snprintf(la, sizeof(la), "%s", entity_label(a));
    snprintf(lb, sizeof(lb), "%s", entity_label(b));

    for (i = 0; i < width; i++) {
        R01sLevel la_lvl, lb_lvl;
        snprintf(dn, sizeof(dn), "%s%d", dst_prefix, i);
        snprintf(an, sizeof(an), "%s%d", a_prefix, i);
        snprintf(bn, sizeof(bn), "%s%d", b_prefix, i);
        snprintf(net, sizeof(net), "%s (via %s.%s)", dn, entity_label(dst), dn);
        snprintf(da, sizeof(da), "%s.%s", la, an);
        snprintf(db, sizeof(db), "%s.%s", lb, bn);
        la_lvl = r01s_entity_sense(a, an);
        lb_lvl = r01s_entity_sense(b, bn);
        r01s_entity_drive(dst, dn, r01s_level_merge_at(la_lvl, lb_lvl, net, da, db));
    }
}

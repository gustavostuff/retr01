#include "netlist_sim/timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_now_ns;
static int g_env_cached;
static int g_env_enabled;
static NsTpdCorner g_env_corner;
static int g_override = -1; /* -1 env, 0 off, 1 on */
static NsTpdCorner g_override_corner = NS_TPD_TYP;

static void timing_cache_env(void) {
    const char *v;
    if (g_env_cached) {
        return;
    }
    g_env_cached = 1;
    g_env_enabled = 0;
    g_env_corner = NS_TPD_TYP;
    v = getenv("NS_PROP_DELAY");
    if (!v || !v[0] || strcmp(v, "0") == 0) {
        return;
    }
    g_env_enabled = 1;
    if (strcmp(v, "max") == 0 || strcmp(v, "MAX") == 0) {
        g_env_corner = NS_TPD_MAX;
    } else {
        g_env_corner = NS_TPD_TYP; /* 1, typ, anything else */
    }
}

void ns_timing_reset(void) {
    /* Keep CLI / test override across board reset (Ctrl+R). */
    g_now_ns = 0;
    g_env_cached = 0;
}

uint64_t ns_timing_now_ns(void) {
    return g_now_ns;
}

void ns_timing_set_now_ns(uint64_t ns) {
    g_now_ns = ns;
}

void ns_timing_advance_ns(uint32_t delta_ns) {
    g_now_ns += (uint64_t)delta_ns;
}

void ns_timing_set_prop_override(int enabled, NsTpdCorner corner) {
    g_override = enabled;
    g_override_corner = corner;
    if (enabled < 0) {
        g_env_cached = 0;
    }
}

int ns_timing_prop_enabled(void) {
    if (g_override >= 0) {
        return g_override != 0;
    }
    timing_cache_env();
    return g_env_enabled;
}

NsTpdCorner ns_timing_corner(void) {
    if (g_override >= 0) {
        return g_override_corner;
    }
    timing_cache_env();
    return g_env_corner;
}

uint32_t ns_timing_tpd_ns(NsTpdPart part) {
    int maxc = (ns_timing_corner() == NS_TPD_MAX);
    switch (part) {
    case NS_TPD_PART_HC245:
        return maxc ? NS_TPD_HC245_MAX_NS : NS_TPD_HC245_TYP_NS;
    case NS_TPD_PART_HC157:
        return maxc ? NS_TPD_HC157_MAX_NS : NS_TPD_HC157_TYP_NS;
    case NS_TPD_PART_ATF22:
        return maxc ? NS_TPD_ATF22_MAX_NS : NS_TPD_ATF22_TYP_NS;
    case NS_TPD_PART_SRAM_TAA:
        return maxc ? NS_TAA_SRAM_MAX_NS : NS_TAA_SRAM_TYP_NS;
    default:
        return 0;
    }
}

uint32_t ns_timing_pin_tpd_ns(NsTpdPart part) {
    (void)part;
    /* Board has no intra-half micro-settle; deferred D/Q misses LE-sampled writes. */
    return 0;
}

uint32_t ns_timing_path_decode_bus_reg_ns(void) {
    /* Decode PLD + bus transceiver + registered PLD output (scroll/raster/MAP). */
    return ns_timing_tpd_ns(NS_TPD_PART_ATF22) + ns_timing_tpd_ns(NS_TPD_PART_HC245) +
           ns_timing_tpd_ns(NS_TPD_PART_ATF22);
}

void ns_timing_print_budget(FILE *out) {
    NsTpdCorner c;
    uint32_t path;
    uint32_t sram;
    if (!out) {
        out = stderr;
    }
    c = ns_timing_corner();
    path = ns_timing_path_decode_bus_reg_ns();
    sram = ns_timing_tpd_ns(NS_TPD_PART_SRAM_TAA);
    fprintf(out,
            "timing: budget corner=%s  decode+245+pld_reg=%uns  SRAM tAA=%uns  PHI2 half=%uns  %s\n",
            c == NS_TPD_MAX ? "max" : "typical", (unsigned)path, (unsigned)sram,
            (unsigned)NS_PHI2_HALF_NS, path > NS_PHI2_HALF_NS ? "WARN path>half" : "path OK");
    fprintf(out,
            "timing: note pin netlist stays combinatorial (DELAY is budget-only); pin tpd would "
            "miss register writes in this settle model.\n");
}

void ns_delay_u8_reset(NsDelayU8 *d, uint8_t v) {
    if (!d) {
        return;
    }
    d->out = v;
    d->next = v;
    d->pending = 0;
    d->ready_ns = 0;
}

uint8_t ns_delay_u8_update(NsDelayU8 *d, uint8_t ideal, uint32_t tpd_ns) {
    uint64_t now;
    if (!d) {
        return ideal;
    }
    if (!ns_timing_prop_enabled() || tpd_ns == 0) {
        d->out = ideal;
        d->next = ideal;
        d->pending = 0;
        return ideal;
    }
    now = ns_timing_now_ns();
    if (d->pending && now >= d->ready_ns) {
        d->out = d->next;
        d->pending = 0;
    }
    if (ideal == d->out) {
        if (d->pending && d->next != ideal) {
            /* New ideal matches current out; cancel stale schedule. */
            d->pending = 0;
        }
        return d->out;
    }
    if (!d->pending || d->next != ideal) {
        d->next = ideal;
        d->ready_ns = now + (uint64_t)tpd_ns;
        d->pending = 1;
    }
    return d->out;
}

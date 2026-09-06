#include "retr01_sim/timing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_now_ns;
static int g_env_cached;
static int g_env_enabled;
static R01sTpdCorner g_env_corner;
static int g_override = -1; /* -1 env, 0 off, 1 on */
static R01sTpdCorner g_override_corner = R01S_TPD_TYP;

static void timing_cache_env(void) {
    const char *v;
    if (g_env_cached) {
        return;
    }
    g_env_cached = 1;
    g_env_enabled = 0;
    g_env_corner = R01S_TPD_TYP;
    v = getenv("R01S_PROP_DELAY");
    if (!v || !v[0] || strcmp(v, "0") == 0) {
        return;
    }
    g_env_enabled = 1;
    if (strcmp(v, "max") == 0 || strcmp(v, "MAX") == 0) {
        g_env_corner = R01S_TPD_MAX;
    } else {
        g_env_corner = R01S_TPD_TYP; /* 1, typ, anything else */
    }
}

void r01s_timing_reset(void) {
    /* Keep CLI / test override across board reset (Ctrl+R). */
    g_now_ns = 0;
    g_env_cached = 0;
}

uint64_t r01s_timing_now_ns(void) {
    return g_now_ns;
}

void r01s_timing_set_now_ns(uint64_t ns) {
    g_now_ns = ns;
}

void r01s_timing_advance_ns(uint32_t delta_ns) {
    g_now_ns += (uint64_t)delta_ns;
}

void r01s_timing_set_prop_override(int enabled, R01sTpdCorner corner) {
    g_override = enabled;
    g_override_corner = corner;
    if (enabled < 0) {
        g_env_cached = 0;
    }
}

int r01s_timing_prop_enabled(void) {
    if (g_override >= 0) {
        return g_override != 0;
    }
    timing_cache_env();
    return g_env_enabled;
}

R01sTpdCorner r01s_timing_corner(void) {
    if (g_override >= 0) {
        return g_override_corner;
    }
    timing_cache_env();
    return g_env_corner;
}

uint32_t r01s_timing_tpd_ns(R01sTpdPart part) {
    int maxc = (r01s_timing_corner() == R01S_TPD_MAX);
    switch (part) {
    case R01S_TPD_PART_HC245:
        return maxc ? R01S_TPD_HC245_MAX_NS : R01S_TPD_HC245_TYP_NS;
    case R01S_TPD_PART_HC157:
        return maxc ? R01S_TPD_HC157_MAX_NS : R01S_TPD_HC157_TYP_NS;
    case R01S_TPD_PART_ATF22:
        return maxc ? R01S_TPD_ATF22_MAX_NS : R01S_TPD_ATF22_TYP_NS;
    case R01S_TPD_PART_SRAM_TAA:
        return maxc ? R01S_TAA_SRAM_MAX_NS : R01S_TAA_SRAM_TYP_NS;
    default:
        return 0;
    }
}

uint32_t r01s_timing_pin_tpd_ns(R01sTpdPart part) {
    (void)part;
    /* Board has no intra-half micro-settle; deferred D/Q misses LE-sampled writes. */
    return 0;
}

uint32_t r01s_timing_path_decode_bus_reg_ns(void) {
    /* Decode PLD + bus transceiver + registered PLD output (scroll/raster/MAP). */
    return r01s_timing_tpd_ns(R01S_TPD_PART_ATF22) + r01s_timing_tpd_ns(R01S_TPD_PART_HC245) +
           r01s_timing_tpd_ns(R01S_TPD_PART_ATF22);
}

void r01s_timing_print_budget(FILE *out) {
    R01sTpdCorner c;
    uint32_t path;
    uint32_t sram;
    if (!out) {
        out = stderr;
    }
    c = r01s_timing_corner();
    path = r01s_timing_path_decode_bus_reg_ns();
    sram = r01s_timing_tpd_ns(R01S_TPD_PART_SRAM_TAA);
    fprintf(out,
            "timing: budget corner=%s  decode+245+pld_reg=%uns  SRAM tAA=%uns  PHI2 half=%uns  %s\n",
            c == R01S_TPD_MAX ? "max" : "typical", (unsigned)path, (unsigned)sram,
            (unsigned)R01S_PHI2_HALF_NS, path > R01S_PHI2_HALF_NS ? "WARN path>half" : "path OK");
    fprintf(out,
            "timing: note pin netlist stays combinatorial (DELAY is budget-only); pin tpd would "
            "miss STA $FExx in this settle model.\n");
}

void r01s_delay_u8_reset(R01sDelayU8 *d, uint8_t v) {
    if (!d) {
        return;
    }
    d->out = v;
    d->next = v;
    d->pending = 0;
    d->ready_ns = 0;
}

uint8_t r01s_delay_u8_update(R01sDelayU8 *d, uint8_t ideal, uint32_t tpd_ns) {
    uint64_t now;
    if (!d) {
        return ideal;
    }
    if (!r01s_timing_prop_enabled() || tpd_ns == 0) {
        d->out = ideal;
        d->next = ideal;
        d->pending = 0;
        return ideal;
    }
    now = r01s_timing_now_ns();
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

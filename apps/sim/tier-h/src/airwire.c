#include "retr01_sim/airwire.h"
#include "retr01_sim/entity.h"

#include <string.h>

static long long air_dist2(const R01sAirPt *a, const R01sAirPt *b) {
    long long dx = (long long)a->x - b->x;
    long long dy = (long long)a->y - b->y;
    return dx * dx + dy * dy;
}

static int air_isqrt(unsigned long long x) {
    unsigned long long r = 0;
    unsigned long long bit = 1ULL << 62;
    while (bit > x) {
        bit >>= 2;
    }
    while (bit) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (int)r;
}

int r01s_air_mst(const R01sAirPt *pts, int n, R01sAirSeg *out, int out_cap) {
    unsigned char in_tree[R01S_AIR_PT_MAX];
    int from[R01S_AIR_PT_MAX];
    long long best[R01S_AIR_PT_MAX];
    int placed;
    int i;

    if (!pts || !out || n < 2 || n > R01S_AIR_PT_MAX || out_cap < n - 1) {
        return 0;
    }
    memset(in_tree, 0, (size_t)n);
    in_tree[0] = 1;
    for (i = 1; i < n; i++) {
        from[i] = 0;
        best[i] = air_dist2(&pts[0], &pts[i]);
    }
    for (placed = 0; placed < n - 1; placed++) {
        int pick = -1;
        long long pick_d = 0;
        for (i = 1; i < n; i++) {
            if (in_tree[i]) {
                continue;
            }
            if (pick < 0 || best[i] < pick_d) {
                pick = i;
                pick_d = best[i];
            }
        }
        if (pick < 0) {
            return 0;
        }
        out[placed].a = from[pick];
        out[placed].b = pick;
        in_tree[pick] = 1;
        for (i = 1; i < n; i++) {
            long long d;
            if (in_tree[i]) {
                continue;
            }
            d = air_dist2(&pts[pick], &pts[i]);
            if (d < best[i]) {
                best[i] = d;
                from[i] = pick;
            }
        }
    }
    return n - 1;
}

int r01s_air_dist_mm(int x0, int y0, int x1, int y1) {
    long long dx = (long long)x0 - x1;
    long long dy = (long long)y0 - y1;
    int px = air_isqrt((unsigned long long)(dx * dx + dy * dy));
    if (NS_PX_PER_MM <= 0) {
        return px;
    }
    return px / NS_PX_PER_MM;
}

int r01s_air_too_long(int dist_mm, int limit_mm) {
    return limit_mm > 0 && dist_mm > limit_mm;
}

static int ref_is(const char *ref, const char *want) {
    return ref && want && strcmp(ref, want) == 0;
}

static int pin_is(const char *pin, const char *want) {
    return pin && want && strcmp(pin, want) == 0;
}

static int ref_number(const char *ref, char prefix, int *num_out) {
    int n = 0;
    const char *p;
    if (!ref || ref[0] != prefix || ref[1] < '0' || ref[1] > '9') {
        return 0;
    }
    p = ref + 1;
    while (*p >= '0' && *p <= '9') {
        n = n * 10 + (*p - '0');
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    if (num_out) {
        *num_out = n;
    }
    return 1;
}

static int pin_is_gnd(const char *pin) {
    return pin_is(pin, "GND") || pin_is(pin, "GND2") || pin_is(pin, "VSS") || pin_is(pin, "AGND") ||
           pin_is(pin, "DGND");
}

static int pin_is_pwr(const char *pin) {
    return pin_is(pin, "VCC") || pin_is(pin, "VDD") || pin_is(pin, "VDDIO2") || pin_is(pin, "AVDD") ||
           pin_is(pin, "APOS") || pin_is(pin, "DPOS");
}

static int pin_is_a_lo(const char *pin) {
    return pin && pin[0] == 'A' && pin[1] >= '0' && pin[1] <= '5' && pin[2] == '\0';
}

static int pin_is_prom_o(const char *pin) {
    return pin && pin[0] == 'O' && pin[1] >= '0' && pin[1] <= '7' && pin[2] == '\0';
}

static int ref_is_s1_local(const char *ref) {
    return ref_is(ref, "US1") || ref_is(ref, "U573") || ref_is(ref, "U41");
}

static int ref_is_clock_island(const char *ref) {
    return ref_is(ref, "U04") || ref_is(ref, "U74");
}

static int air_bypass_cap(const char *refdes) {
    int n = 0;
    if (!ref_number(refdes, 'C', &n)) {
        return 0;
    }
    return n >= 1 && n <= 20;
}

static int bypass_hop(const R01sAirEnd *a, const R01sAirEnd *b) {
    int a_cap;
    int b_cap;
    if (!a || !b) {
        return 0;
    }
    a_cap = air_bypass_cap(a->refdes) && pin_is(a->pin, "1");
    b_cap = air_bypass_cap(b->refdes) && pin_is(b->pin, "1");
    if (a_cap && pin_is_pwr(b->pin)) {
        return 1;
    }
    if (b_cap && pin_is_pwr(a->pin)) {
        return 1;
    }
    return 0;
}

void r01s_air_style(const char *net_name, const R01sAirEnd *ends, int n, R01sAirStyle *out) {
    int i;
    int all_gnd = n > 0;
    int color = 0;
    int analog = 0;
    int quiet_slow = 0;
    int s1_local = n > 0;

    if (!out) {
        return;
    }
    out->layer = R01S_AIR_LAYER_NOISY;
    out->limit_mm = 0;
    out->pwr = 0;
    if (!ends || n < 1) {
        ends = NULL;
        n = 0;
        all_gnd = 0;
        s1_local = 0;
    }
    if (net_name && strcmp(net_name, "GND") == 0) {
        out->layer = R01S_AIR_LAYER_GND;
        return;
    }
    if (net_name && strcmp(net_name, "+5V") == 0) {
        out->pwr = 1;
        return;
    }
    if (net_name && (strncmp(net_name, "XTAL_", 5) == 0 || strncmp(net_name, "FSC_", 4) == 0)) {
        out->layer = R01S_AIR_LAYER_QUIET;
        out->limit_mm = 20;
        return;
    }
    if (net_name && strcmp(net_name, "CLK_21M") == 0) {
        out->limit_mm = 20;
        return;
    }
    for (i = 0; i < n; i++) {
        const char *ref = ends[i].refdes;
        const char *pin = ends[i].pin;
        if (!pin_is_gnd(pin)) {
            all_gnd = 0;
        }
        if (!ref_is_s1_local(ref)) {
            s1_local = 0;
        }
        if ((ref_is(ref, "U24") || ref_is(ref, "UPLDV")) && pin_is_a_lo(pin)) {
            color = 1;
        }
        if (pin_is(pin, "RIN") || pin_is(pin, "GIN") || pin_is(pin, "BIN") || pin_is(pin, "COMP") ||
            (ref_is(ref, "U24") && pin_is_prom_o(pin))) {
            analog = 1;
        }
        if (pin_is(pin, "AUDIO_PWM") || pin_is(pin, "SDA") || pin_is(pin, "SCL") || pin_is(pin, "RESB") ||
            pin_is(pin, "RDY") || pin_is(pin, "CPU_RDY") || pin_is(pin, "PAD_DATA")) {
            quiet_slow = 1;
        }
    }
    if (all_gnd) {
        out->layer = R01S_AIR_LAYER_GND;
        return;
    }
    if (analog || (net_name && (strcmp(net_name, "COMPOSITE_OUT") == 0 || strcmp(net_name, "AUDIO_OUT") == 0))) {
        out->layer = R01S_AIR_LAYER_QUIET;
        if (analog || (net_name && strcmp(net_name, "COMPOSITE_OUT") == 0)) {
            out->limit_mm = 20;
        }
        return;
    }
    if (color) {
        out->limit_mm = 25;
        return;
    }
    if (quiet_slow) {
        out->layer = R01S_AIR_LAYER_QUIET;
        return;
    }
    if (s1_local) {
        out->limit_mm = 30;
    }
}

int r01s_air_hop_limit_mm(const R01sAirStyle *st, const R01sAirEnd *a, const R01sAirEnd *b) {
    if (!st) {
        return 0;
    }
    if (st->layer == R01S_AIR_LAYER_GND) {
        return 0;
    }
    if (st->pwr) {
        return bypass_hop(a, b) ? 5 : 0;
    }
    if (st->limit_mm > 0) {
        return st->limit_mm;
    }
    if (a && b && ref_is_clock_island(a->refdes) && ref_is_clock_island(b->refdes) && !pin_is_pwr(a->pin) &&
        !pin_is_pwr(b->pin) && !pin_is_gnd(a->pin) && !pin_is_gnd(b->pin)) {
        return 20;
    }
    return 0;
}

static const struct {
    const char *ref;
    int zone;
} AIR_ZONE[] = {
    {"C1", R01S_ZONE_CPU},  {"C2", R01S_ZONE_Z1},   {"C3", R01S_ZONE_Z3},   {"C4", R01S_ZONE_Z4},
    {"C5", R01S_ZONE_M},    {"C6", R01S_ZONE_Z4},   {"C7", R01S_ZONE_Z5},   {"C8", R01S_ZONE_Z2},
    {"C9", R01S_ZONE_Z2},   {"C10", R01S_ZONE_Z2},  {"C11", R01S_ZONE_Z3},  {"C12", R01S_ZONE_Z3},
    {"C13", R01S_ZONE_Z3},  {"C14", R01S_ZONE_Z4},  {"C15", R01S_ZONE_Z3},  {"C16", R01S_ZONE_Z2},
    {"C17", R01S_ZONE_Z1},  {"C18", R01S_ZONE_Z2},  {"C19", R01S_ZONE_Z2},  {"C20", R01S_ZONE_Z2},
    {"C22", R01S_ZONE_Z2},  {"C23", R01S_ZONE_Z2},  {"C24", R01S_ZONE_Z2},  {"C25", R01S_ZONE_Z2},
    {"C26", R01S_ZONE_Z2},  {"C27", R01S_ZONE_Z2},  {"E1", R01S_ZONE_Z1},   {"J1", R01S_ZONE_J8},
    {"J2", R01S_ZONE_Z2},   {"J3", R01S_ZONE_Z5},   {"J4", R01S_ZONE_Z5},       {"J5", R01S_ZONE_Z5},
    {"J7", R01S_ZONE_Z5},   {"J8", R01S_ZONE_J8},   {"J9", R01S_ZONE_J8},
    {"J36", R01S_ZONE_SPINE}, {"R1", R01S_ZONE_Z2}, {"R2", R01S_ZONE_Z2},   {"R3", R01S_ZONE_Z2},
    {"R4", R01S_ZONE_Z2},   {"R5", R01S_ZONE_Z2},   {"R6", R01S_ZONE_Z2},   {"R7", R01S_ZONE_Z2},
    {"R8", R01S_ZONE_Z2},   {"R9", R01S_ZONE_Z2},   {"R10", R01S_ZONE_Z2},  {"R11", R01S_ZONE_Z2},
    {"R12", R01S_ZONE_Z2},  {"R13", R01S_ZONE_Z2},  {"R14", R01S_ZONE_SPINE}, {"R15", R01S_ZONE_SPINE},
    {"R16", R01S_ZONE_SPINE}, {"R17", R01S_ZONE_SPINE}, {"R18", R01S_ZONE_SPINE}, {"R19", R01S_ZONE_SPINE},
    {"R20", R01S_ZONE_SPINE}, {"R21", R01S_ZONE_SPINE}, {"R22", R01S_ZONE_SPINE}, {"R23", R01S_ZONE_SPINE},
    {"R24", R01S_ZONE_M},   {"R25", R01S_ZONE_M},   {"R26", R01S_ZONE_Z5},  {"R27", R01S_ZONE_M},
    {"R28", R01S_ZONE_M},   {"R29", R01S_ZONE_CPU}, {"R30", R01S_ZONE_Z1},  {"R31", R01S_ZONE_Z2},
    {"R32", R01S_ZONE_Z2},  {"SW1", R01S_ZONE_J8},  {"SW_RST", R01S_ZONE_Z1}, {"U04", R01S_ZONE_Z2},
    {"U1", R01S_ZONE_CPU},  {"U130", R01S_ZONE_Z1}, {"U24", R01S_ZONE_Z2},  {"U3", R01S_ZONE_Z1},
    {"U41", R01S_ZONE_Z4},  {"U573", R01S_ZONE_Z4}, {"U574", R01S_ZONE_Z3}, {"U6", R01S_ZONE_Z3},
    {"U725", R01S_ZONE_Z2}, {"U74", R01S_ZONE_Z2},  {"U7A", R01S_ZONE_Z3},  {"U7B", R01S_ZONE_Z3},
    {"U7C", R01S_ZONE_Z3},  {"UM", R01S_ZONE_M},    {"UPLDV", R01S_ZONE_Z2}, {"UPLDX", R01S_ZONE_Z2},
    {"UPLDY", R01S_ZONE_Z2}, {"US1", R01S_ZONE_Z4}, {"US2", R01S_ZONE_Z5},  {"Y1", R01S_ZONE_Z2},
    {"Y2", R01S_ZONE_Z2},   {"Y3", R01S_ZONE_Z2},
};

int r01s_air_zone_for_ref(const char *refdes) {
    size_t i;
    if (!refdes) {
        return -1;
    }
    for (i = 0; i < sizeof(AIR_ZONE) / sizeof(AIR_ZONE[0]); i++) {
        if (strcmp(AIR_ZONE[i].ref, refdes) == 0) {
            return AIR_ZONE[i].zone;
        }
    }
    return -1;
}

const char *r01s_air_zone_name(int zone) {
    static const char *names[R01S_ZONE_COUNT] = {
        "Zone 1", "CPU", "Zone 2", "Spine", "Zone 3", "MCU-M", "Zone 4", "Zone 5", "Rear",
    };
    if (zone < 0 || zone >= R01S_ZONE_COUNT) {
        return "";
    }
    return names[zone];
}

int r01s_air_outside_px(int zx, int zy, int zw, int zh, int bx, int by, int bw, int bh) {
    int over = 0;
    int d;
    if (zw <= 0 || zh <= 0) {
        return 0;
    }
    d = zx - bx;
    if (d > over) {
        over = d;
    }
    d = (bx + bw) - (zx + zw);
    if (d > over) {
        over = d;
    }
    d = zy - by;
    if (d > over) {
        over = d;
    }
    d = (by + bh) - (zy + zh);
    if (d > over) {
        over = d;
    }
    return over;
}

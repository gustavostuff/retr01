#ifndef RETR01_SIM_AIRWIRE_H
#define RETR01_SIM_AIRWIRE_H

/* Visual ratsnest and floor-plan zones. Does not change the pin union-find. */

#define R01S_AIR_PT_MAX 256

#define R01S_ZONE_Z1 0
#define R01S_ZONE_CPU 1
#define R01S_ZONE_Z2 2
#define R01S_ZONE_SPINE 3
#define R01S_ZONE_Z3 4
#define R01S_ZONE_M 5
#define R01S_ZONE_Z4 6
#define R01S_ZONE_Z5 7
#define R01S_ZONE_J8 8 /* rear edge: J1, SW1, J8, J9 */
#define R01S_ZONE_COUNT 9

/* layer: 0 = GND (tin), 1 = noisy (red), 4 = quiet (blue). */
#define R01S_AIR_LAYER_GND 0
#define R01S_AIR_LAYER_NOISY 1
#define R01S_AIR_LAYER_QUIET 4

typedef struct R01sAirPt {
    int x;
    int y;
} R01sAirPt;

typedef struct R01sAirSeg {
    int a;
    int b;
} R01sAirSeg;

typedef struct R01sAirEnd {
    const char *refdes;
    const char *pin;
} R01sAirEnd;

typedef struct R01sAirStyle {
    int layer;
    int limit_mm; /* 0 = this net has no length blink */
    int pwr;      /* 1 = +5V. Bypass hops use 5 mm on their own. */
} R01sAirStyle;

/* Minimum spanning tree. Returns n-1, or 0 when n < 2. n must be <= R01S_AIR_PT_MAX. */
int r01s_air_mst(const R01sAirPt *pts, int n, R01sAirSeg *out, int out_cap);

/* Euclidean pin-tip distance in mm (NS_PX_PER_MM). */
int r01s_air_dist_mm(int x0, int y0, int x1, int y1);

/* 1 when limit_mm > 0 and dist_mm is past it. */
int r01s_air_too_long(int dist_mm, int limit_mm);

void r01s_air_style(const char *net_name, const R01sAirEnd *ends, int n, R01sAirStyle *out);

/* Limit for one tree hop. Bypass cap-to-VCC is 5 mm even when the +5V trunk is free. */
int r01s_air_hop_limit_mm(const R01sAirStyle *st, const R01sAirEnd *a, const R01sAirEnd *b);

/* -1 when the refdes is not on the motherboard floor plan. */
int r01s_air_zone_for_ref(const char *refdes);
const char *r01s_air_zone_name(int zone);

/* How far the body sticks out of the zone, in px. 0 when the body is inside. */
int r01s_air_outside_px(int zx, int zy, int zw, int zh, int bx, int by, int bw, int bh);

#endif

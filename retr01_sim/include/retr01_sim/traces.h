#ifndef RETR01_SIM_TRACES_H
#define RETR01_SIM_TRACES_H

#include "retr01_sim/entity.h"

#include <SDL.h>

#define R01S_TRACE_MAX_LINKS 128
#define R01S_TRACE_MAX_SEGS 512
#define R01S_TRACE_JUMP_R 5

typedef struct R01sTraceLink {
    R01sEntity *ea;
    R01sEntity *eb;
    const char *pin_a;
    const char *pin_b;
    int net_id;
    int lane; /* parallel offset for bus bits */
} R01sTraceLink;

typedef struct R01sTraceSeg {
    int x0, y0, x1, y1; /* axis-aligned; either x0==x1 or y0==y1 */
    int net_id;
    R01sLevel level;
} R01sTraceSeg;

typedef struct R01sTraceMap {
    R01sTraceLink links[R01S_TRACE_MAX_LINKS];
    int link_count;
    R01sTraceSeg segs[R01S_TRACE_MAX_SEGS];
    int seg_count;
} R01sTraceMap;

void r01s_traces_clear(R01sTraceMap *m);
int r01s_traces_add(R01sTraceMap *m, R01sEntity *ea, const char *pin_a, R01sEntity *eb, const char *pin_b,
                    int net_id, int lane);

/* DIP pin tip in board space (matches package draw). Returns 0 ok. */
int r01s_pin_tip_xy(const R01sEntity *e, const char *pin_name, int *ox, int *oy);

/* Rebuild orthographic segments from links; sample levels from pin_a. */
void r01s_traces_rebuild(R01sTraceMap *m);

/* Draw segments with schematic U-jumps at crossings (different nets). */
void r01s_traces_draw(const R01sTraceMap *m, SDL_Renderer *r, int pan_x, int pan_y);

#endif

#ifndef retr01_STUDIO_PLAY_H
#define retr01_STUDIO_PLAY_H

#include "retr01_studio/game_runtime.h"
#include "retr01_studio/types.h"

/* Player AABB in world pixels (square for now). */
#define R01_PLAY_PLAYER_W 8
#define R01_PLAY_PLAYER_H 8
/* Default spawn: center of the spawn screen. */
#define R01_PLAY_SPAWN_CENTER_X(col) ((col)*R01_SCREEN_PX_W + (R01_SCREEN_PX_W - R01_PLAY_PLAYER_W) / 2)
#define R01_PLAY_SPAWN_CENTER_Y(row) ((row)*R01_SCREEN_PX_H + (R01_SCREEN_PX_H - R01_PLAY_PLAYER_H) / 2)

#define R01_PLAY_BTN_X 0
#define R01_PLAY_BTN_Y 1

typedef struct R01PlayState {
    int active;
    R01GameCtx ctx;
} R01PlayState;

/* Viewport-relative OAM build (Studio Play / Phase D host). Entry 0 = player. */
typedef struct R01OamEntry {
    int x;
    int y;
    int bank;
    int tile_id;
    int pal;
    int flip_h;
    int flip_v;
} R01OamEntry;

int r01_play_start(R01PlayState *pl, const R01Project *p, const char *project_path);
void r01_play_stop(R01PlayState *pl);
void r01_play_tick(R01PlayState *pl, const R01Project *p, int dx, int dy);
int r01_play_button(R01PlayState *pl, const R01Project *p, int button);
int r01_play_screen_index(const R01PlayState *pl, const R01World *w);

/* Player collision AABB in world pixels. origin_x/y is the state origin (Play position). */
void r01_play_player_hit_rect(const R01World *w, const R01GameCtx *ctx, int origin_x, int origin_y, int *hx,
                              int *hy, int *hw, int *hh);

int r01_play_sample_bg(const R01Project *p, const R01PlayState *pl, int vx, int vy, uint8_t *r, uint8_t *g,
                       uint8_t *b);

/* Build OAM for current Play camera. Returns count written.
 * Player occupies the leading slots (state 0 / frame 0 of marked entity, or stub tile). */
int r01_play_build_oam(const R01Project *p, const R01PlayState *pl, R01OamEntry *out, int cap);
int r01_play_fade_level(const R01PlayState *pl);
int r01_play_fade_color(const R01PlayState *pl);

/* OAM packing: uint8 stores viewport-relative signed coords (-128..127). */
static inline int r01_oam_coord_from_u8(uint8_t v) {
    return (int)(int8_t)v;
}

static inline uint8_t r01_oam_coord_to_u8(int v) {
    return (uint8_t)(int8_t)v;
}

/* True when an 8x8 sprite at (x,y) does not intersect the 128x120 viewport. */
static inline int r01_oam_tile_off_screen(int x, int y) {
    return x + 8 <= 0 || y + 8 <= 0 || x >= R01_SCREEN_PX_W || y >= R01_SCREEN_PX_H;
}

#endif

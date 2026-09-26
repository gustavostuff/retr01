#ifndef R01A_RASTER_H
#define R01A_RASTER_H

#include "netlist_sim/video_sink.h"

#include <stdint.h>

/* Tier A raster (docs/bringup/tier-a-video-lab.md). */
#define R01A_BEAM_DOTS_X NS_RASTER_DOTS_X
#define R01A_BEAM_DOTS_Y NS_RASTER_DOTS_Y
#define R01A_BEAM_VISIBLE_W NS_VIDEO_W
#define R01A_BEAM_VISIBLE_H NS_VIDEO_H

/* Approx NTSC-ish blanking. HSYNC ~4.7 us at DOT (~25 dots) after a short porch. */
#define R01A_HSYNC_START 264
#define R01A_HSYNC_END 289
#define R01A_VSYNC_START 243
#define R01A_VSYNC_END 246

/*
 * Tier B/C test layers (docs/bringup/tier-b-video-lab.md, tier-c-video-lab.md).
 * BG1 bars from X[7:5]. Region 2 is kit 0 so BG0 shows through.
 * Tier C: sprite kit from field SRAM; BG0 from ping-pong line in field (optional).
 */
static const uint8_t R01A_BAR_INDEX[8] = {48, 52, 0, 58, 44, 33, 34, 16};
static const uint8_t R01A_BAND_INDEX[8] = {2, 6, 10, 14, 18, 22, 26, 30};

#define R01C_BG0_LINE_BASE 0x4000u
#define R01C_BG0_PING_STRIDE 128u

/* dot_x/dot_y are beam coordinates in the 128x120 playfield. */
static inline uint8_t r01c_bg0_at(const uint8_t *field_mem, int dot_x, int dot_y, int ping) {
    uint32_t addr;
    (void)ping;
    if (!field_mem || dot_x < 0 || dot_x >= NS_LOGICAL_W || dot_y < 0 || dot_y >= NS_LOGICAL_H) {
        return 0;
    }
    addr = R01C_BG0_LINE_BASE + (uint32_t)dot_y * (uint32_t)NS_LOGICAL_W + (uint32_t)dot_x;
    return field_mem[addr];
}

static inline uint8_t r01a_compositor_index(int dot_x, int dot_y, int xb, int yb, int hblank, int vblank,
                                              const uint8_t *field_mem, int bg0_ping) {
    uint8_t bg1;
    uint8_t spr;
    uint8_t bg0;
    if (hblank || vblank) {
        return 0;
    }
    xb &= 7;
    yb &= 7;
    if (field_mem && dot_x >= 0 && dot_x < NS_LOGICAL_W && dot_y >= 0 && dot_y < NS_LOGICAL_H) {
        spr = field_mem[(size_t)dot_y * (size_t)NS_LOGICAL_W + (size_t)dot_x];
        if (spr != 0) {
            return spr;
        }
    }
    bg1 = R01A_BAR_INDEX[xb];
    if (bg1 != 0) {
        return bg1;
    }
    bg0 = r01c_bg0_at(field_mem, dot_x, dot_y, bg0_ping);
    if (bg0 != 0) {
        return bg0;
    }
    return R01A_BAND_INDEX[yb];
}

#endif

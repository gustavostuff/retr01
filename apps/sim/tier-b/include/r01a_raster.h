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
 * Tier B test layers (docs/bringup/tier-b-video-lab.md).
 * BG1 bars from X[7:5]. Region 2 is kit 0 so BG0 shows through.
 * BG0 bands from Y[7:5]. Sprite box is a fixed kit index.
 */
static const uint8_t R01A_BAR_INDEX[8] = {48, 52, 0, 58, 44, 33, 34, 16};
static const uint8_t R01A_BAND_INDEX[8] = {2, 6, 10, 14, 18, 22, 26, 30};

#define R01A_SPR_INDEX 63
#define R01A_SPR_XB0 3
#define R01A_SPR_XB1 4
#define R01A_SPR_YB0 2
#define R01A_SPR_YB1 3

/* xb/yb are the 32-dot cell (X[7:5] / Y[7:5]). Blanking forces index 0. */
static inline uint8_t r01a_compositor_index(int xb, int yb, int hblank, int vblank) {
    uint8_t bg1;
    if (hblank || vblank) {
        return 0;
    }
    xb &= 7;
    yb &= 7;
    if (xb >= R01A_SPR_XB0 && xb <= R01A_SPR_XB1 && yb >= R01A_SPR_YB0 && yb <= R01A_SPR_YB1) {
        return R01A_SPR_INDEX;
    }
    bg1 = R01A_BAR_INDEX[xb];
    if (bg1 != 0) {
        return bg1;
    }
    return R01A_BAND_INDEX[yb];
}

#endif

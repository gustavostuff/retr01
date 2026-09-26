#ifndef R01A_RASTER_H
#define R01A_RASTER_H

#include "discrete_ic/video_sink.h"

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

/* Method B color bars (active X regions of 32 dots). */
static const uint8_t R01A_BAR_INDEX[8] = {48, 52, 55, 58, 44, 33, 34, 16};

static inline uint8_t r01a_bar_index_at_x(int x) {
    if (x < 0 || x >= R01A_BEAM_VISIBLE_W) {
        return 0;
    }
    return R01A_BAR_INDEX[x >> 5];
}

#endif

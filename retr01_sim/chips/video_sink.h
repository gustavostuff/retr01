#ifndef RETR01_SIM_VIDEO_SINK_H
#define RETR01_SIM_VIDEO_SINK_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_VIDEO_W 128
#define R01S_VIDEO_H 120

/*
 * Island O — logical 128x120 RGBS / LCD sink (SCALE 2x beam maps here).
 * Board feeds dot-sampled PROM bytes; UI reads the expanded framebuffer.
 */
typedef struct R01sVideoSink {
    R01sEntity base;
    uint8_t rgb[R01S_VIDEO_W * R01S_VIDEO_H * 3];
    uint32_t dot_samples;
    uint32_t lit_pixels;
    uint8_t last_packed;
} R01sVideoSink;

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes);
R01sEntity *r01s_video_sink_entity(R01sVideoSink *chip);

void r01s_video_sink_plot(R01sVideoSink *chip, int lx, int ly, uint8_t prom_byte);
const uint8_t *r01s_video_sink_rgb(const R01sVideoSink *chip);
uint32_t r01s_video_sink_lit_pixels(const R01sVideoSink *chip);
uint8_t r01s_video_sink_pixel_packed(const R01sVideoSink *chip, int lx, int ly);

#endif

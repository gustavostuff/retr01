#ifndef RETR01_SIM_VIDEO_SINK_H
#define RETR01_SIM_VIDEO_SINK_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/* RGBS active field (docs/02) — LCD framebuffer matches CRT visible area. */
#define R01S_VIDEO_W 256
#define R01S_VIDEO_H 240
/* Game / Studio logical resolution (16×15 tiles). */
#define R01S_LOGICAL_W 128
#define R01S_LOGICAL_H 120
/* SCALE 1x: center logical playfield inside the RGBS field. */
#define R01S_SCALE_1X_OX ((R01S_VIDEO_W - R01S_LOGICAL_W) / 2) /* 64 */
#define R01S_SCALE_1X_OY ((R01S_VIDEO_H - R01S_LOGICAL_H) / 2) /* 60 */

/*
 * Island O — 256×240 RGBS / LCD sink.
 * Beam timing stays 341×262 (HBLANK X≥256, VBLANK Y≥240).
 * Board SCALE DIP: 1x (default) centers 128×120 with border; 2x fills the field.
 */
typedef struct R01sVideoSink {
    R01sEntity base;
    uint8_t rgb[R01S_VIDEO_W * R01S_VIDEO_H * 3];
    uint32_t dot_samples;
    uint32_t lit_pixels;
    uint8_t last_packed;
    uint8_t scale_2x; /* 1 = 2x fills field, 0 = 1x centered (default) */
} R01sVideoSink;

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes);
R01sEntity *r01s_video_sink_entity(R01sVideoSink *chip);

/* Map visible beam (bx,by) → logical (lx,ly). Returns 1 if playfield, 0 if border (1x). */
int r01s_rgbs_beam_to_logical(int scale_2x, int bx, int by, int *lx, int *ly);

void r01s_video_sink_set_scale_2x(R01sVideoSink *chip, int scale_2x);
int r01s_video_sink_scale_2x(const R01sVideoSink *chip);

void r01s_video_sink_plot(R01sVideoSink *chip, int fx, int fy, uint8_t prom_byte);
void r01s_video_sink_clear(R01sVideoSink *chip);
const uint8_t *r01s_video_sink_rgb(const R01sVideoSink *chip);
uint32_t r01s_video_sink_lit_pixels(const R01sVideoSink *chip);
uint8_t r01s_video_sink_pixel_packed(const R01sVideoSink *chip, int fx, int fy);

#endif

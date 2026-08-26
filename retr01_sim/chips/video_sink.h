#ifndef retr01_SIM_VIDEO_SINK_H
#define retr01_SIM_VIDEO_SINK_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/* RGBS active field (docs/02). LCD framebuffer matches CRT visible area. */
#define R01S_VIDEO_W 256
#define R01S_VIDEO_H 240
/* Game / Studio logical resolution (16×15 tiles). */
#define R01S_LOGICAL_W 128
#define R01S_LOGICAL_H 120
/* SCALE 1x: center logical playfield inside the RGBS field. */
#define R01S_SCALE_1X_OX ((R01S_VIDEO_W - R01S_LOGICAL_W) / 2) /* 64 */
#define R01S_SCALE_1X_OY ((R01S_VIDEO_H - R01S_LOGICAL_H) / 2) /* 60 */

/*
 * Island O: 256×240 RGBS / LCD sink.
 * Beam timing stays 341×262 (HBLANK X≥256, VBLANK Y≥240).
 * Board SCALE DIP: 1x (default) centers 128×120 with border; 2x fills the field.
 */
typedef enum R01sVideoRenderMode {
    R01S_VIDEO_RENDER_NORMAL = 0,
    R01S_VIDEO_RENDER_PERSIST = 1,
    R01S_VIDEO_RENDER_PHOSPHOR = 2,
} R01sVideoRenderMode;

#define R01S_VIDEO_RENDER_DEFAULT R01S_VIDEO_RENDER_PERSIST

/* UI control strip above the LCD preview (matches sim ui.c). */
#define R01S_VIDEO_SINK_CTRL_H 24

typedef struct R01sVideoSink {
    R01sEntity base;
    uint8_t rgb[R01S_VIDEO_W * R01S_VIDEO_H * 3];
    uint32_t dot_samples;
    uint32_t lit_pixels;
    uint8_t last_packed;
    uint8_t scale_2x;     /* 1 = 2x fills field, 0 = 1x centered (default) */
    uint8_t render_mode;  /* R01sVideoRenderMode */
    uint8_t field_active; /* 1 while beam is painting the visible field */
} R01sVideoSink;

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes);
R01sEntity *r01s_video_sink_entity(R01sVideoSink *chip);

/* Map visible beam (bx,by) → logical (lx,ly). Returns 1 if playfield, 0 if border (1x). */
int r01s_rgbs_beam_to_logical(int scale_2x, int bx, int by, int *lx, int *ly);

void r01s_video_sink_set_scale_2x(R01sVideoSink *chip, int scale_2x);
int r01s_video_sink_scale_2x(const R01sVideoSink *chip);
void r01s_video_sink_refresh_glyph(R01sVideoSink *chip);
void r01s_video_sink_lcd_size(const R01sVideoSink *chip, int *w, int *h);

void r01s_video_sink_set_render_mode(R01sVideoSink *chip, int mode);
int r01s_video_sink_render_mode(const R01sVideoSink *chip);
void r01s_video_sink_set_field_active(R01sVideoSink *chip, int active);

void r01s_video_sink_plot(R01sVideoSink *chip, int fx, int fy, uint8_t master_index);
void r01s_video_sink_clear(R01sVideoSink *chip);
/* Field boundary: normal clears, phosphor decays, persist keeps prior pixels. */
void r01s_video_sink_on_vblank(R01sVideoSink *chip);
/* Reserved for future UI hooks (phosphor decays on VBlank). */
void r01s_video_sink_display_tick(R01sVideoSink *chip, int beam_x, int beam_y);
const uint8_t *r01s_video_sink_rgb(const R01sVideoSink *chip);
uint32_t r01s_video_sink_lit_pixels(const R01sVideoSink *chip);
uint8_t r01s_video_sink_pixel_packed(const R01sVideoSink *chip, int fx, int fy);

#endif

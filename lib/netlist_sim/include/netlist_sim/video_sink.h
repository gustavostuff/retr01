#ifndef NETLIST_SIM_VIDEO_SINK_H
#define NETLIST_SIM_VIDEO_SINK_H

#include "netlist_sim/entity.h"

#include <stdint.h>

/* RGBS active field . LCD framebuffer matches CRT visible area. */
#define NS_VIDEO_W 256
#define NS_VIDEO_H 240
/* Game / Studio logical resolution (16x15 tiles). */
#define NS_LOGICAL_W 128
#define NS_LOGICAL_H 120
/* SCALE 1x: center logical playfield inside the RGBS field. */
#define NS_SCALE_1X_OX ((NS_VIDEO_W - NS_LOGICAL_W) / 2) /* 64 */
#define NS_SCALE_1X_OY ((NS_VIDEO_H - NS_LOGICAL_H) / 2) /* 60 */

/*
 * Island O: 256x240 RGBS / LCD sink.
 * Beam timing stays 341x262 (HBLANK X>=256, VBLANK Y>=240).
 * Board SCALE DIP: 1x (default) centers 128x120 with border; 2x fills the field.
 */
typedef enum NsVideoRenderMode {
    NS_VIDEO_RENDER_NORMAL = 0,
    NS_VIDEO_RENDER_PERSIST = 1,
    NS_VIDEO_RENDER_PHOSPHOR = 2,
} NsVideoRenderMode;

#define NS_VIDEO_RENDER_DEFAULT NS_VIDEO_RENDER_PERSIST

/* Optional host palette (e.g. Retr01 AT28C16 kit). NULL = R3G3B2 expand. */
typedef void (*NsVideoPaletteFn)(uint8_t master_index, uint8_t *r, uint8_t *g, uint8_t *b);

/* LCD control overlay is drawn inside the framebuffer (sim UI). */
typedef struct NsVideoSink {
    NsEntity base;
    uint8_t rgb[NS_VIDEO_W * NS_VIDEO_H * 3];
    uint32_t dot_samples;
    uint32_t lit_pixels;
    uint8_t last_packed;
    uint8_t scale_2x;     /* 1 = 2x fills field, 0 = 1x centered (default) */
    uint8_t render_mode;  /* NsVideoRenderMode */
    uint8_t field_active; /* 1 while beam is painting the visible field */
    NsVideoPaletteFn palette;
} NsVideoSink;

void ns_video_sink_init(NsVideoSink *chip, const char *refdes);
NsEntity *ns_video_sink_entity(NsVideoSink *chip);

/* Map visible beam (bx,by) -> logical (lx,ly). Returns 1 if playfield, 0 if border (1x). */
int ns_rgbs_beam_to_logical(int scale_2x, int bx, int by, int *lx, int *ly);

void ns_video_sink_set_scale_2x(NsVideoSink *chip, int scale_2x);
int ns_video_sink_scale_2x(const NsVideoSink *chip);
void ns_video_sink_refresh_glyph(NsVideoSink *chip);
void ns_video_sink_lcd_size(const NsVideoSink *chip, int *w, int *h);

void ns_video_sink_set_render_mode(NsVideoSink *chip, int mode);
int ns_video_sink_render_mode(const NsVideoSink *chip);
void ns_video_sink_set_field_active(NsVideoSink *chip, int active);

void ns_video_sink_set_palette(NsVideoSink *chip, NsVideoPaletteFn fn);
void ns_video_sink_plot(NsVideoSink *chip, int fx, int fy, uint8_t master_index);
void ns_video_sink_clear(NsVideoSink *chip);
/* Field boundary: normal clears, phosphor decays, persist keeps prior pixels. */
void ns_video_sink_on_vblank(NsVideoSink *chip);
/* Reserved for future UI hooks (phosphor decays on VBlank). */
void ns_video_sink_display_tick(NsVideoSink *chip, int beam_x, int beam_y);
const uint8_t *ns_video_sink_rgb(const NsVideoSink *chip);
uint32_t ns_video_sink_lit_pixels(const NsVideoSink *chip);
uint8_t ns_video_sink_pixel_packed(const NsVideoSink *chip, int fx, int fy);

#endif

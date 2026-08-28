#include "video_sink.h"

#include "at28c16.h"

#include "retr01_sim/board_layout.h"

#include <string.h>

/* ~15% / UI frame: prior-field residue and vblank gaps. */
#define R01S_VIDEO_PHOSPHOR_DECAY_NUM 217
#define R01S_VIDEO_PHOSPHOR_DECAY_DEN 256
/* ~2.3% / UI frame: lines the beam already drew this field (stay visible until refresh). */
#define R01S_VIDEO_PHOSPHOR_TRAIL_NUM 250

static void sink_reset(R01sEntity *e) {
    R01sVideoSink *c = (R01sVideoSink *)e;
    memset(c->rgb, 0, sizeof(c->rgb));
    c->dot_samples = 0;
    c->lit_pixels = 0;
    c->last_packed = 0;
    /* SCALE DIP and render mode persist across reset (board switch, not soft reset). */
}

static void sink_eval(R01sEntity *e) {
    (void)e;
}

static void sink_tick(R01sEntity *e) {
    (void)e;
}

static void sink_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable SINK_VT = {sink_reset, sink_eval, sink_tick, sink_destroy};

static int sink_mode_valid(int mode) {
    return mode >= R01S_VIDEO_RENDER_NORMAL && mode <= R01S_VIDEO_RENDER_PHOSPHOR;
}

static void sink_phosphor_decay(R01sVideoSink *chip) {
    size_t i;
    size_t n;

    if (!chip) {
        return;
    }
    n = sizeof(chip->rgb);
    for (i = 0; i < n; i++) {
        chip->rgb[i] = (uint8_t)((chip->rgb[i] * R01S_VIDEO_PHOSPHOR_DECAY_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
    }
}

/* Decay scanlines the beam has finished (mild) and prior-field rows below (strong). */
static void sink_phosphor_decay_after_beam(R01sVideoSink *chip, int beam_y) {
    int y;

    if (!chip) {
        return;
    }
    if (beam_y < 0) {
        beam_y = 0;
    }
    /* Completed this field: gentle fade so top does not go black mid-field. */
    for (y = 0; y < beam_y && y < R01S_VIDEO_H; y++) {
        size_t off = (size_t)y * (size_t)R01S_VIDEO_W * 3u;
        size_t end = off + (size_t)R01S_VIDEO_W * 3u;
        size_t i;
        for (i = off; i < end; i++) {
            chip->rgb[i] =
                (uint8_t)((chip->rgb[i] * R01S_VIDEO_PHOSPHOR_TRAIL_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
        }
    }
    /* Below the beam: previous field residue clears faster. */
    for (y = beam_y + 1; y < R01S_VIDEO_H; y++) {
        size_t off = (size_t)y * (size_t)R01S_VIDEO_W * 3u;
        size_t end = off + (size_t)R01S_VIDEO_W * 3u;
        size_t i;
        for (i = off; i < end; i++) {
            chip->rgb[i] =
                (uint8_t)((chip->rgb[i] * R01S_VIDEO_PHOSPHOR_DECAY_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
        }
    }
}

int r01s_rgbs_beam_to_logical(int scale_2x, int bx, int by, int *lx, int *ly) {
    int x;
    int y;
    if (bx < 0 || by < 0 || bx >= R01S_VIDEO_W || by >= R01S_VIDEO_H) {
        return 0;
    }
    if (scale_2x) {
        x = bx / 2;
        y = by / 2;
    } else {
        x = bx - R01S_SCALE_1X_OX;
        y = by - R01S_SCALE_1X_OY;
    }
    if (x < 0 || y < 0 || x >= R01S_LOGICAL_W || y >= R01S_LOGICAL_H) {
        return 0;
    }
    if (lx) {
        *lx = x;
    }
    if (ly) {
        *ly = y;
    }
    return 1;
}

void r01s_video_sink_lcd_size(const R01sVideoSink *chip, int *w, int *h) {
    if (chip && chip->scale_2x) {
        if (w) {
            *w = R01S_VIDEO_W;
        }
        if (h) {
            *h = R01S_VIDEO_H;
        }
    } else {
        if (w) {
            *w = R01S_LOGICAL_W;
        }
        if (h) {
            *h = R01S_LOGICAL_H;
        }
    }
}

void r01s_video_sink_refresh_glyph(R01sVideoSink *chip) {
    int lcd_w;
    int lcd_h;
    int body_w;
    int body_h;

    if (!chip) {
        return;
    }
    r01s_video_sink_lcd_size(chip, &lcd_w, &lcd_h);
    body_w = lcd_w;
    body_h = lcd_h;
    chip->base.body_w = r01s_snap5_up(body_w);
    chip->base.body_h = r01s_snap5_up(body_h);
}

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &SINK_VT, "SCREEN_SINK", refdes ? refdes : "SCR1");
    chip->base.impl = chip;
    chip->scale_2x = 0; /* 1x centered playfield (toggle to 2x via UI / G) */
    chip->render_mode = R01S_VIDEO_RENDER_DEFAULT;
    r01s_entity_add_pin(&chip->base, 1, "DOT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "HSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "VSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "VCC", R01S_PIN_PWR);
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_DISPLAY, 0, 0);
    r01s_video_sink_refresh_glyph(chip);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_video_sink_entity(R01sVideoSink *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_video_sink_set_scale_2x(R01sVideoSink *chip, int scale_2x) {
    if (!chip) {
        return;
    }
    chip->scale_2x = scale_2x ? 1 : 0;
    r01s_video_sink_refresh_glyph(chip);
}

int r01s_video_sink_scale_2x(const R01sVideoSink *chip) {
    return chip ? (chip->scale_2x ? 1 : 0) : 0;
}

void r01s_video_sink_set_render_mode(R01sVideoSink *chip, int mode) {
    if (!chip || !sink_mode_valid(mode)) {
        return;
    }
    chip->render_mode = (uint8_t)mode;
}

int r01s_video_sink_render_mode(const R01sVideoSink *chip) {
    return chip ? (int)chip->render_mode : R01S_VIDEO_RENDER_DEFAULT;
}

void r01s_video_sink_set_field_active(R01sVideoSink *chip, int active) {
    if (chip) {
        chip->field_active = active ? 1u : 0u;
    }
}

void r01s_video_sink_plot(R01sVideoSink *chip, int fx, int fy, uint8_t master_index) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    size_t off;
    if (!chip || fx < 0 || fy < 0 || fx >= R01S_VIDEO_W || fy >= R01S_VIDEO_H) {
        return;
    }
    chip->dot_samples++;
    /* Studio/emu SoT: full kit RGB. PROM R3G3B2 is still driven on the IC path. */
    r01s_at28c16_kit_rgb((int)master_index, &r, &g, &b);
    chip->last_packed = (uint8_t)(((r * 7 + 127) / 255 << 5) | ((g * 7 + 127) / 255 << 2) |
                                  ((b * 3 + 127) / 255));
    off = (size_t)(fy * R01S_VIDEO_W + fx) * 3u;
    if ((master_index & 63u) != 0) {
        chip->lit_pixels++;
    }
    chip->field_active = 1;
    chip->rgb[off] = r;
    chip->rgb[off + 1] = g;
    chip->rgb[off + 2] = b;
}

void r01s_video_sink_clear(R01sVideoSink *chip) {
    if (!chip) {
        return;
    }
    memset(chip->rgb, 0, sizeof(chip->rgb));
    chip->lit_pixels = 0;
}

void r01s_video_sink_on_vblank(R01sVideoSink *chip) {
    if (!chip) {
        return;
    }
    chip->field_active = 0;
    if (chip->render_mode == R01S_VIDEO_RENDER_NORMAL) {
        r01s_video_sink_clear(chip);
        return;
    }
    /* Persist/Phosphor: keep prior pixels. Phosphor fades on display_tick (~60 Hz). */
}

void r01s_video_sink_display_tick(R01sVideoSink *chip, int beam_x, int beam_y) {
    if (!chip || chip->render_mode != R01S_VIDEO_RENDER_PHOSPHOR) {
        return;
    }
    (void)beam_x;
    /*
     * Fade scanlines after the beam leaves them (y < beam_y). Current line stays
     * full bright while drawn. Rows below fade as prior-field residue.
     */
    if (chip->field_active && beam_y >= 0 && beam_y < R01S_VIDEO_H) {
        sink_phosphor_decay_after_beam(chip, beam_y);
    } else {
        sink_phosphor_decay(chip);
    }
}

const uint8_t *r01s_video_sink_rgb(const R01sVideoSink *chip) {
    return chip ? chip->rgb : NULL;
}

uint32_t r01s_video_sink_lit_pixels(const R01sVideoSink *chip) {
    return chip ? chip->lit_pixels : 0;
}

uint8_t r01s_video_sink_pixel_packed(const R01sVideoSink *chip, int fx, int fy) {
    const uint8_t *p;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (!chip || fx < 0 || fy < 0 || fx >= R01S_VIDEO_W || fy >= R01S_VIDEO_H) {
        return 0;
    }
    p = chip->rgb + (size_t)(fy * R01S_VIDEO_W + fx) * 3u;
    r = p[0];
    g = p[1];
    b = p[2];
    return (uint8_t)(((r * 7 + 127) / 255 << 5) | ((g * 7 + 127) / 255 << 2) | ((b * 3 + 127) / 255));
}

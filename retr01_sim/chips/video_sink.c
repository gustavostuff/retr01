#include "video_sink.h"

#include "at28c16.h"

#include <string.h>

static void sink_reset(R01sEntity *e) {
    R01sVideoSink *c = (R01sVideoSink *)e;
    memset(c->rgb, 0, sizeof(c->rgb));
    c->dot_samples = 0;
    c->lit_pixels = 0;
    c->last_packed = 0;
    /* SCALE DIP persists across reset (board switch, not soft reset). */
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

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &SINK_VT, "LCD_SINK", refdes ? refdes : "LCD1");
    chip->base.impl = chip;
    chip->scale_2x = 0; /* 1x centered playfield (toggle to 2x via UI / G) */
    r01s_entity_add_pin(&chip->base, 1, "DOT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "HSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "VSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "VCC", R01S_PIN_PWR);
    /* Body = 256×240 field + bezel / labels. */
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_DISPLAY, R01S_VIDEO_W + 16, R01S_VIDEO_H + 36);
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
}

int r01s_video_sink_scale_2x(const R01sVideoSink *chip) {
    return chip ? (chip->scale_2x ? 1 : 0) : 0;
}

void r01s_video_sink_plot(R01sVideoSink *chip, int fx, int fy, uint8_t prom_byte) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    size_t off;
    if (!chip || fx < 0 || fy < 0 || fx >= R01S_VIDEO_W || fy >= R01S_VIDEO_H) {
        return;
    }
    chip->dot_samples++;
    chip->last_packed = prom_byte;
    r01s_at28c16_unpack_rgb(prom_byte, &r, &g, &b);
    off = (size_t)(fy * R01S_VIDEO_W + fx) * 3u;
    if (prom_byte != 0) {
        chip->lit_pixels++;
    }
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

#include "video_sink.h"

#include "at28c16.h"

#include <string.h>

static void sink_reset(R01sEntity *e) {
    R01sVideoSink *c = (R01sVideoSink *)e;
    memset(c->rgb, 0, sizeof(c->rgb));
    c->dot_samples = 0;
    c->lit_pixels = 0;
    c->last_packed = 0;
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

void r01s_video_sink_init(R01sVideoSink *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &SINK_VT, "LCD_SINK", refdes ? refdes : "LCD1");
    chip->base.impl = chip;
    r01s_entity_add_pin(&chip->base, 1, "DOT", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "HSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 3, "VSYNC", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 4, "VCC", R01S_PIN_PWR);
    r01s_entity_set_glyph(&chip->base, R01S_ENTITY_VIS_DISPLAY, 72, 52);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_video_sink_entity(R01sVideoSink *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_video_sink_plot(R01sVideoSink *chip, int lx, int ly, uint8_t prom_byte) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    size_t off;
    if (!chip || lx < 0 || ly < 0 || lx >= R01S_VIDEO_W || ly >= R01S_VIDEO_H) {
        return;
    }
    chip->dot_samples++;
    chip->last_packed = prom_byte;
    r01s_at28c16_unpack_rgb(prom_byte, &r, &g, &b);
    off = (size_t)(ly * R01S_VIDEO_W + lx) * 3u;
    if (prom_byte != 0) {
        chip->lit_pixels++;
    }
    chip->rgb[off] = r;
    chip->rgb[off + 1] = g;
    chip->rgb[off + 2] = b;
}

const uint8_t *r01s_video_sink_rgb(const R01sVideoSink *chip) {
    return chip ? chip->rgb : NULL;
}

uint32_t r01s_video_sink_lit_pixels(const R01sVideoSink *chip) {
    return chip ? chip->lit_pixels : 0;
}

uint8_t r01s_video_sink_pixel_packed(const R01sVideoSink *chip, int lx, int ly) {
    const uint8_t *p;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (!chip || lx < 0 || ly < 0 || lx >= R01S_VIDEO_W || ly >= R01S_VIDEO_H) {
        return 0;
    }
    p = chip->rgb + (size_t)(ly * R01S_VIDEO_W + lx) * 3u;
    r = p[0];
    g = p[1];
    b = p[2];
    return (uint8_t)(((r * 7 + 127) / 255 << 5) | ((g * 7 + 127) / 255 << 2) | ((b * 3 + 127) / 255));
}

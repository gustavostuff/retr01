#include "netlist_sim/video_sink.h"

#include <string.h>

/* Default 64-color R3G3B2 expand (generic, no external PROM model). */
static void palette_rgb(uint8_t master_index, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t i = (uint8_t)(master_index & 63u);
    uint8_t r3 = (uint8_t)((i >> 5) & 7u);
    uint8_t g3 = (uint8_t)((i >> 2) & 7u);
    uint8_t b2 = (uint8_t)(i & 3u);
    if (r) {
        *r = (uint8_t)((r3 * 255u) / 7u);
    }
    if (g) {
        *g = (uint8_t)((g3 * 255u) / 7u);
    }
    if (b) {
        *b = (uint8_t)((b2 * 255u) / 3u);
    }
}

/* ~15% / UI frame: prior-field residue and vblank gaps. */
#define NS_VIDEO_PHOSPHOR_DECAY_NUM 217
#define NS_VIDEO_PHOSPHOR_DECAY_DEN 256
/* ~2.3% / UI frame: lines the beam already drew this field (stay visible until refresh). */
#define NS_VIDEO_PHOSPHOR_TRAIL_NUM 250

static void sink_reset(NsEntity *e) {
    NsVideoSink *c = (NsVideoSink *)e;
    memset(c->rgb, 0, sizeof(c->rgb));
    c->dot_samples = 0;
    c->lit_pixels = 0;
    c->last_packed = 0;
    /* SCALE DIP and render mode persist across reset (board switch, not soft reset). */
}

static void sink_eval(NsEntity *e) {
    (void)e;
}

static void sink_tick(NsEntity *e) {
    (void)e;
}

static void sink_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable SINK_VT = {sink_reset, sink_eval, sink_tick, sink_destroy};

static int sink_mode_valid(int mode) {
    return mode >= NS_VIDEO_RENDER_NORMAL && mode <= NS_VIDEO_RENDER_PHOSPHOR;
}

static void sink_phosphor_decay(NsVideoSink *chip) {
    size_t i;
    size_t n;

    if (!chip) {
        return;
    }
    n = sizeof(chip->rgb);
    for (i = 0; i < n; i++) {
        chip->rgb[i] = (uint8_t)((chip->rgb[i] * NS_VIDEO_PHOSPHOR_DECAY_NUM) / NS_VIDEO_PHOSPHOR_DECAY_DEN);
    }
}

/* Decay scanlines the beam has finished (mild) and prior-field rows below (strong). */
static void sink_phosphor_decay_after_beam(NsVideoSink *chip, int beam_y) {
    int y;

    if (!chip) {
        return;
    }
    if (beam_y < 0) {
        beam_y = 0;
    }
    /* Completed this field: gentle fade so top does not go black mid-field. */
    for (y = 0; y < beam_y && y < NS_VIDEO_H; y++) {
        size_t off = (size_t)y * (size_t)NS_VIDEO_W * 3u;
        size_t end = off + (size_t)NS_VIDEO_W * 3u;
        size_t i;
        for (i = off; i < end; i++) {
            chip->rgb[i] =
                (uint8_t)((chip->rgb[i] * NS_VIDEO_PHOSPHOR_TRAIL_NUM) / NS_VIDEO_PHOSPHOR_DECAY_DEN);
        }
    }
    /* Below the beam: previous field residue clears faster. */
    for (y = beam_y + 1; y < NS_VIDEO_H; y++) {
        size_t off = (size_t)y * (size_t)NS_VIDEO_W * 3u;
        size_t end = off + (size_t)NS_VIDEO_W * 3u;
        size_t i;
        for (i = off; i < end; i++) {
            chip->rgb[i] =
                (uint8_t)((chip->rgb[i] * NS_VIDEO_PHOSPHOR_DECAY_NUM) / NS_VIDEO_PHOSPHOR_DECAY_DEN);
        }
    }
}

int ns_rgbs_beam_to_logical(int scale_2x, int bx, int by, int *lx, int *ly) {
    int x;
    int y;
    if (bx < 0 || by < 0 || bx >= NS_VIDEO_W || by >= NS_VIDEO_H) {
        return 0;
    }
    if (scale_2x) {
        x = bx / 2;
        y = by / 2;
    } else {
        x = bx - NS_SCALE_1X_OX;
        y = by - NS_SCALE_1X_OY;
    }
    if (x < 0 || y < 0 || x >= NS_LOGICAL_W || y >= NS_LOGICAL_H) {
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

void ns_video_sink_lcd_size(const NsVideoSink *chip, int *w, int *h) {
    (void)chip;
    /* LCD glyph is the CRT visible field. 1x centers the playfield inside it. */
    if (w) {
        *w = NS_VIDEO_W;
    }
    if (h) {
        *h = NS_VIDEO_H;
    }
}

void ns_video_sink_refresh_glyph(NsVideoSink *chip) {
    int lcd_w;
    int lcd_h;
    int body_w;
    int body_h;

    if (!chip) {
        return;
    }
    ns_video_sink_lcd_size(chip, &lcd_w, &lcd_h);
    body_w = lcd_w;
    body_h = lcd_h;
    chip->base.body_w = body_w > 0 ? body_w : 1;
    chip->base.body_h = body_h > 0 ? body_h : 1;
}

void ns_video_sink_init(NsVideoSink *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    ns_entity_init(&chip->base, &SINK_VT, "SCREEN_SINK", refdes ? refdes : "SCR1");
    chip->base.impl = chip;
    chip->scale_2x = 0; /* 1x centered playfield (toggle to 2x via UI / G) */
    chip->render_mode = NS_VIDEO_RENDER_DEFAULT;
    ns_entity_add_pin(&chip->base, 1, "DOT", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 2, "HSYNC", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 3, "VSYNC", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 4, "VCC", NS_PIN_PWR);
    /* Schematic RGB DAC inputs (AD724 stand-in until encoder chip is modeled). */
    ns_entity_add_pin(&chip->base, 5, "RIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 6, "GIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 7, "BIN", NS_PIN_IN);
    ns_entity_add_pin(&chip->base, 8, "AGND", NS_PIN_PWR);
    ns_entity_set_glyph(&chip->base, NS_ENTITY_VIS_DISPLAY, 0, 0);
    ns_video_sink_refresh_glyph(chip);
    ns_entity_reset(&chip->base);
}

NsEntity *ns_video_sink_entity(NsVideoSink *chip) {
    return chip ? &chip->base : NULL;
}

void ns_video_sink_set_scale_2x(NsVideoSink *chip, int scale_2x) {
    if (!chip) {
        return;
    }
    chip->scale_2x = scale_2x ? 1 : 0;
    ns_video_sink_refresh_glyph(chip);
    ns_video_sink_clear(chip);
}

int ns_video_sink_scale_2x(const NsVideoSink *chip) {
    return chip ? (chip->scale_2x ? 1 : 0) : 0;
}

void ns_video_sink_set_render_mode(NsVideoSink *chip, int mode) {
    if (!chip || !sink_mode_valid(mode)) {
        return;
    }
    chip->render_mode = (uint8_t)mode;
}

int ns_video_sink_render_mode(const NsVideoSink *chip) {
    return chip ? (int)chip->render_mode : NS_VIDEO_RENDER_DEFAULT;
}

void ns_video_sink_set_field_active(NsVideoSink *chip, int active) {
    if (chip) {
        chip->field_active = active ? 1u : 0u;
    }
}

void ns_video_sink_set_palette(NsVideoSink *chip, NsVideoPaletteFn fn) {
    if (chip) {
        chip->palette = fn;
    }
}

void ns_video_sink_plot(NsVideoSink *chip, int fx, int fy, uint8_t master_index) {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    size_t off;
    if (!chip || fx < 0 || fy < 0 || fx >= NS_VIDEO_W || fy >= NS_VIDEO_H) {
        return;
    }
    chip->dot_samples++;
    /* Host palette when set; otherwise generic R3G3B2 expand. */
    if (chip->palette) {
        chip->palette(master_index, &r, &g, &b);
    } else {
        palette_rgb(master_index, &r, &g, &b);
    }
    chip->last_packed = (uint8_t)(((r * 7 + 127) / 255 << 5) | ((g * 7 + 127) / 255 << 2) |
                                  ((b * 3 + 127) / 255));
    off = (size_t)(fy * NS_VIDEO_W + fx) * 3u;
    if ((master_index & 63u) != 0) {
        chip->lit_pixels++;
    }
    chip->field_active = 1;
    chip->rgb[off] = r;
    chip->rgb[off + 1] = g;
    chip->rgb[off + 2] = b;
}

void ns_video_sink_clear(NsVideoSink *chip) {
    if (!chip) {
        return;
    }
    memset(chip->rgb, 0, sizeof(chip->rgb));
    chip->lit_pixels = 0;
}

void ns_video_sink_on_vblank(NsVideoSink *chip) {
    if (!chip) {
        return;
    }
    chip->field_active = 0;
    if (chip->render_mode == NS_VIDEO_RENDER_NORMAL) {
        ns_video_sink_clear(chip);
        return;
    }
    /* Persist/Phosphor: keep prior pixels. Phosphor fades on display_tick (~60 Hz). */
}

void ns_video_sink_display_tick(NsVideoSink *chip, int beam_x, int beam_y) {
    if (!chip || chip->render_mode != NS_VIDEO_RENDER_PHOSPHOR) {
        return;
    }
    (void)beam_x;
    /*
     * Fade scanlines after the beam leaves them (y < beam_y). Current line stays
     * full bright while drawn. Rows below fade as prior-field residue.
     */
    if (chip->field_active && beam_y >= 0 && beam_y < NS_VIDEO_H) {
        sink_phosphor_decay_after_beam(chip, beam_y);
    } else {
        sink_phosphor_decay(chip);
    }
}

const uint8_t *ns_video_sink_rgb(const NsVideoSink *chip) {
    return chip ? chip->rgb : NULL;
}

uint32_t ns_video_sink_lit_pixels(const NsVideoSink *chip) {
    return chip ? chip->lit_pixels : 0;
}

uint8_t ns_video_sink_pixel_packed(const NsVideoSink *chip, int fx, int fy) {
    const uint8_t *p;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    if (!chip || fx < 0 || fy < 0 || fx >= NS_VIDEO_W || fy >= NS_VIDEO_H) {
        return 0;
    }
    p = chip->rgb + (size_t)(fy * NS_VIDEO_W + fx) * 3u;
    r = p[0];
    g = p[1];
    b = p[2];
    return (uint8_t)(((r * 7 + 127) / 255 << 5) | ((g * 7 + 127) / 255 << 2) | ((b * 3 + 127) / 255));
}

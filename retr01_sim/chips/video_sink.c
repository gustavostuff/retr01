#include "video_sink.h"

#include "at28c16.h"
#include "../src/agent_debug_log.h"

#include <string.h>

/* ~15% per UI frame (~60 Hz): trails clear in ~0.5 s while beam still paints. */
#define R01S_VIDEO_PHOSPHOR_DECAY_NUM 217
#define R01S_VIDEO_PHOSPHOR_DECAY_DEN 256

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

static uint32_t sink_nonzero_pixels(const R01sVideoSink *chip) {
    size_t i;
    size_t n;
    uint32_t count = 0;

    if (!chip) {
        return 0;
    }
    n = sizeof(chip->rgb);
    for (i = 0; i < n; i += 3u) {
        if (chip->rgb[i] || chip->rgb[i + 1] || chip->rgb[i + 2]) {
            count++;
        }
    }
    return count;
}

static uint32_t sink_rgb_energy(const R01sVideoSink *chip) {
    size_t i;
    size_t n;
    uint64_t sum = 0;

    if (!chip) {
        return 0;
    }
    n = sizeof(chip->rgb);
    for (i = 0; i < n; i++) {
        sum += chip->rgb[i];
    }
    return (uint32_t)(sum / 256u);
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

/* Decay unscanned residue: rows below beam_y, and on beam_y only x >= beam_x. */
static void sink_phosphor_decay_unscanned(R01sVideoSink *chip, int beam_x, int beam_y) {
    int y;
    int x;

    if (!chip) {
        return;
    }
    if (beam_y < 0) {
        beam_y = 0;
    }
    if (beam_x < 0) {
        beam_x = 0;
    }
    if (beam_y >= R01S_VIDEO_H) {
        return;
    }
    /* Current scanline: only pixels the beam has not rewritten yet. */
    if (beam_x < R01S_VIDEO_W) {
        for (x = beam_x; x < R01S_VIDEO_W; x++) {
            size_t off = (size_t)(beam_y * R01S_VIDEO_W + x) * 3u;
            chip->rgb[off] =
                (uint8_t)((chip->rgb[off] * R01S_VIDEO_PHOSPHOR_DECAY_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
            chip->rgb[off + 1] =
                (uint8_t)((chip->rgb[off + 1] * R01S_VIDEO_PHOSPHOR_DECAY_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
            chip->rgb[off + 2] =
                (uint8_t)((chip->rgb[off + 2] * R01S_VIDEO_PHOSPHOR_DECAY_NUM) / R01S_VIDEO_PHOSPHOR_DECAY_DEN);
        }
    }
    /* Rows below the beam: previous field. */
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
    static int vblank_logs;

    if (!chip) {
        return;
    }
    chip->field_active = 0;
    if (chip->render_mode == R01S_VIDEO_RENDER_NORMAL) {
        r01s_video_sink_clear(chip);
        if (vblank_logs < 40) {
            /* #region agent log */
            r01s_agent_debug_log("H1", "video_sink.c:on_vblank", "normal_clear", (int)chip->render_mode, 0, 0,
                                 0);
            /* #endregion */
            vblank_logs++;
        }
        return;
    }
    /* Persist/Phosphor: keep prior pixels. Phosphor fades on display_tick (~60 Hz). */
    if (vblank_logs < 40) {
        /* #region agent log */
        r01s_agent_debug_log("H1", "video_sink.c:on_vblank", "field_end", (int)chip->render_mode,
                             (int)sink_nonzero_pixels(chip), (int)sink_rgb_energy(chip),
                             (int)chip->field_active);
        /* #endregion */
        vblank_logs++;
    }
}

void r01s_video_sink_display_tick(R01sVideoSink *chip, int beam_x, int beam_y) {
    static int tick_logs;
    uint32_t e_before;
    uint32_t e_after;

    if (!chip || chip->render_mode != R01S_VIDEO_RENDER_PHOSPHOR) {
        return;
    }
    e_before = sink_rgb_energy(chip);
    /*
     * Decay only unscanned residue so a slow beam does not fade pixels it already
     * painted this field (that caused horizontal phosphor streaks).
     */
    if (chip->field_active && beam_y >= 0 && beam_y < R01S_VIDEO_H) {
        sink_phosphor_decay_unscanned(chip, beam_x, beam_y);
    } else {
        sink_phosphor_decay(chip);
    }
    e_after = sink_rgb_energy(chip);
    if (tick_logs < 40) {
        /* #region agent log */
        r01s_agent_debug_log("H15", "video_sink.c:display_tick", "phosphor_decay", (int)e_before, (int)e_after,
                             beam_x, beam_y);
        /* #endregion */
        tick_logs++;
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

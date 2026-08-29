#include "board_debug.h"

#include "as6c62256.h"
#include "atmega1284p.h"
#include "beam_xy.h"
#include "sst39sf040.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define R01S_DEBUG_INTERVAL_MS 1000u

static FILE *g_log;
static int g_enabled;
static uint32_t g_last_snap_ms;
static uint32_t g_start_ms;
static uint32_t g_prev_health_bits;
static uint32_t g_prev_nmi;
static uint16_t g_prev_pc;
static char g_path[512];

static uint32_t health_bits(const R01sBoard *b) {
    return ((uint32_t)b->health_saw_latch << 0) | ((uint32_t)b->health_saw_vram << 1) |
           ((uint32_t)b->health_saw_vram_read << 2) | ((uint32_t)b->health_saw_pad << 3) |
           ((uint32_t)b->health_saw_beam << 4) | ((uint32_t)b->health_saw_bg_fetch << 5) |
           ((uint32_t)b->health_saw_video << 6) | ((uint32_t)b->health_saw_map << 7) |
           ((uint32_t)b->health_saw_apu << 8) | ((uint32_t)b->health_saw_oam << 9) |
           ((uint32_t)b->health_saw_linebuf << 10) | ((uint32_t)b->health_saw_sprites << 11) |
           ((uint32_t)b->health_saw_nmi << 12);
}

static void ensure_parent_dir(const char *path) {
    char dir[512];
    char *slash;
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if (!slash) {
        return;
    }
    *slash = '\0';
    if (dir[0] != '\0') {
        mkdir(dir, 0755);
    }
}

static int count_vram_nonzero(const R01sBoard *b, uint16_t base, int n) {
    int i;
    int c = 0;
    for (i = 0; i < n; i++) {
        if (r01s_as6c62256_peek(&b->vram, (uint16_t)(base + i)) != 0) {
            c++;
        }
    }
    return c;
}

static void dump_hex_line(FILE *f, const char *tag, const R01sBoard *b, uint16_t base, int n) {
    int i;
    fprintf(f, "  %s @%04X:", tag, (unsigned)base);
    for (i = 0; i < n; i++) {
        fprintf(f, " %02X", r01s_as6c62256_peek(&b->vram, (uint16_t)(base + i)));
    }
    fputc('\n', f);
}

static void dump_rgb(FILE *f, const R01sBoard *b, int x, int y) {
    const uint8_t *rgb = r01s_video_sink_rgb(&b->video_sink);
    size_t off;
    if (!rgb || x < 0 || y < 0 || x >= R01S_VIDEO_W || y >= R01S_VIDEO_H) {
        fprintf(f, " rgb(%d,%d)=n/a", x, y);
        return;
    }
    off = (size_t)(y * R01S_VIDEO_W + x) * 3u;
    fprintf(f, " rgb(%d,%d)=#%02X%02X%02X pk=%02X", x, y, rgb[off], rgb[off + 1], rgb[off + 2],
            r01s_video_sink_pixel_packed(&b->video_sink, x, y));
}

static void write_snapshot(R01sBoard *b, uint32_t wall_ms, const char *why) {
    uint16_t pc;
    uint8_t a;
    uint32_t bits;
    int bx, by;
    int i;
    uint32_t prg0;
    if (!g_log || !b) {
        return;
    }
    pc = r01s_w65c02s_pc(&b->cpu);
    a = r01s_w65c02s_a(&b->cpu);
    bits = health_bits(b);
    bx = r01s_beam_xy_x(&b->pld_beam_x);
    by = r01s_beam_xy_y(&b->pld_beam_x);
    prg0 = b->cart_off_prg;

    fprintf(g_log, "\n==== snap t=%ums (+%ums) why=%s ====\n", (unsigned)wall_ms,
            (unsigned)(wall_ms - g_start_ms), why ? why : "-");
    fprintf(g_log, "cpu: PC=%04X A=%02X cycles=%u reset_hold=%d\n", pc, a, (unsigned)b->cycles,
            b->reset_hold);
    fprintf(g_log,
            "cart: loaded=%d label=%s off_prg=$%06X len_prg=$%04X off_chr=$%06X map_s0=$%06X "
            "pal_bg=$%06X magic=%02X%02X%02X%02X%02X%02X "
            "prg0=%02X%02X%02X%02X (expect A9..=LDA if bring-up overlay)\n",
            b->cart_loaded, b->cart_label[0] ? b->cart_label : "-", (unsigned)prg0,
            (unsigned)b->cart_len_prg, (unsigned)b->cart_off_chr, (unsigned)b->cart_off_map_screen0,
            (unsigned)b->cart_off_pal_bg, r01s_sst39sf040_peek(&b->cart_flash, 0),
            r01s_sst39sf040_peek(&b->cart_flash, 1), r01s_sst39sf040_peek(&b->cart_flash, 2),
            r01s_sst39sf040_peek(&b->cart_flash, 3), r01s_sst39sf040_peek(&b->cart_flash, 4),
            r01s_sst39sf040_peek(&b->cart_flash, 5), r01s_sst39sf040_peek(&b->cart_flash, prg0 + 0),
            r01s_sst39sf040_peek(&b->cart_flash, prg0 + 1),
            r01s_sst39sf040_peek(&b->cart_flash, prg0 + 2),
            r01s_sst39sf040_peek(&b->cart_flash, prg0 + 3));
    fprintf(g_log,
            "health bits=%04X latch=%d vram_w=%d vram_r=%d pad=%d beam=%d bg=%d video=%d map=%d "
            "apu=%d oam=%d linebuf=%d spr=%d nmi=%d nmi_pulses=%u\n",
            (unsigned)bits, b->health_saw_latch, b->health_saw_vram, b->health_saw_vram_read,
            b->health_saw_pad, b->health_saw_beam, b->health_saw_bg_fetch, b->health_saw_video,
            b->health_saw_map, b->health_saw_apu, b->health_saw_oam, b->health_saw_linebuf,
            b->health_saw_sprites, b->health_saw_nmi, (unsigned)b->nmi_pulses);
    fprintf(g_log, "beam: x=%d y=%d hblank=%d vblank=%d\n", bx, by, r01s_beam_xy_hblank(&b->pld_beam_x),
            r01s_beam_xy_vblank(&b->pld_beam_x));
    fprintf(g_log, "vram: nonzero_tiles_slot0=%d/240 nonzero_attrs_slot0=%d/240 vram_addr=$%04X\n",
            count_vram_nonzero(b, 0, 240), count_vram_nonzero(b, 0xF0, 240),
            (unsigned)b->vram_addr);
    dump_hex_line(g_log, "tiles", b, 0, 16);
    dump_hex_line(g_log, "attrs", b, 0xF0, 16);
    fprintf(g_log, "  pal:");
    for (i = 0; i < 16; i++) {
        fprintf(g_log, " %02X", b->active_pal[i]);
    }
    fprintf(g_log, "  chr_last=%02X\n", b->chr_last_master);
    fprintf(g_log, "  oam:");
    for (i = 0; i < 16; i++) {
        fprintf(g_log, " %02X", r01s_atmega1284p_oam_peek(&b->mcu1284, (uint8_t)i));
    }
    fputc('\n', g_log);
    fprintf(g_log, "video: lit=%u samples=%u scale=%dx", (unsigned)r01s_video_sink_lit_pixels(&b->video_sink),
            (unsigned)b->video_sink.dot_samples, r01s_video_sink_scale_2x(&b->video_sink) ? 2 : 1);
    dump_rgb(g_log, b, 0, 0);
    dump_rgb(g_log, b, 128, 120);
    dump_rgb(g_log, b, 64, 60);
    fputc('\n', g_log);
    fflush(g_log);
}

void r01s_board_debug_begin(R01sBoard *board, int enabled) {
    const char *env_path;
    time_t now;
    g_enabled = enabled ? 1 : 0;
    g_log = NULL;
    g_last_snap_ms = 0;
    g_start_ms = 0;
    g_prev_health_bits = 0;
    g_prev_nmi = 0;
    g_prev_pc = 0;
    g_path[0] = '\0';
    if (!g_enabled) {
        return;
    }
    now = time(NULL);
    env_path = getenv("R01S_DEBUG_LOG");
    if (env_path && env_path[0]) {
        snprintf(g_path, sizeof(g_path), "%s", env_path);
    } else {
        /* Unique path per run so an editor open on an old log isn't overwritten. */
        snprintf(g_path, sizeof(g_path), "retr01_sim/debug/sim_trace_%ld.log", (long)now);
    }
    ensure_parent_dir(g_path);
    g_log = fopen(g_path, "w");
    if (!g_log) {
        fprintf(stderr, "debug: cannot open %s -- tracing disabled\n", g_path);
        g_enabled = 0;
        return;
    }
    /* Always close on process exit (Esc, window close, or abort paths that still unwind). */
    atexit(r01s_board_debug_end);
    fprintf(g_log, "retr01_sim board debug trace\n");
    fprintf(g_log, "started: %s", ctime(&now));
    fprintf(g_log, "note: cart PRG from flash image (Studio export); synthetic cart uses sim overlay.\n");
    fprintf(g_log, "expected LCD after bring-up MAP/CHR: world-0 screen (sky), not only smoke red.\n");
    if (board) {
        write_snapshot(board, 0, "begin");
    }
    fflush(g_log);
    fprintf(stderr, "debug: writing %s (~1 Hz snapshots; quit after ~20s and send this file)\n", g_path);
    fprintf(stderr, "debug: also linked as retr01_sim/debug/sim_trace.log (symlink to this run)\n");
    {
        /* Convenience symlink for "the latest run"; safe if link fails. */
        (void)remove("retr01_sim/debug/sim_trace.log");
        (void)symlink(strrchr(g_path, '/') ? strrchr(g_path, '/') + 1 : g_path,
                      "retr01_sim/debug/sim_trace.log");
    }
}

void r01s_board_debug_tick(R01sBoard *board, uint32_t wall_ms) {
    uint32_t bits;
    uint16_t pc;
    if (!g_enabled || !g_log || !board) {
        return;
    }
    if (g_start_ms == 0) {
        g_start_ms = wall_ms ? wall_ms : 1;
        g_last_snap_ms = wall_ms;
        g_prev_health_bits = health_bits(board);
        g_prev_nmi = board->nmi_pulses;
        g_prev_pc = r01s_w65c02s_pc(&board->cpu);
        return;
    }
    bits = health_bits(board);
    pc = r01s_w65c02s_pc(&board->cpu);
    if (bits != g_prev_health_bits) {
        fprintf(g_log, "event: health %04X -> %04X at t=%ums PC=%04X\n", (unsigned)g_prev_health_bits,
                (unsigned)bits, (unsigned)wall_ms, pc);
        write_snapshot(board, wall_ms, "health_edge");
        g_prev_health_bits = bits;
        g_last_snap_ms = wall_ms;
    }
    if (board->nmi_pulses != g_prev_nmi) {
        fprintf(g_log, "event: nmi_pulses %u -> %u at t=%ums beam_y=%d\n", (unsigned)g_prev_nmi,
                (unsigned)board->nmi_pulses, (unsigned)wall_ms, r01s_beam_xy_y(&board->pld_beam_x));
        g_prev_nmi = board->nmi_pulses;
    }
    if (wall_ms - g_last_snap_ms >= R01S_DEBUG_INTERVAL_MS) {
        write_snapshot(board, wall_ms, pc == g_prev_pc ? "periodic_pc_stable" : "periodic");
        g_prev_pc = pc;
        g_last_snap_ms = wall_ms;
    }
}

void r01s_board_debug_end(void) {
    if (g_log) {
        fprintf(g_log, "\n==== end ====\n");
        fflush(g_log);
        fclose(g_log);
        g_log = NULL;
        fprintf(stderr, "debug: closed %s\n", g_path);
    }
    g_enabled = 0;
}

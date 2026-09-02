#include "retr01_sim/frame_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct R01sFrameLog {
    int enabled;
    int sealed;
    int bg_ready;
    int sprite_ready;
    int page;
    int line_count;
    R01sFrameLogLine lines[R01S_FLOG_MAX_LINES];
    /* open xfer run */
    int xfer_open;
    R01sFrameLogCat xfer_cat;
    char xfer_tag[24];
    uint32_t xfer_addr0;
    uint32_t xfer_addr1;
    uint8_t xfer_first;
    uint8_t xfer_last;
    uint32_t xfer_n;
} R01sFrameLog;

static R01sFrameLog g_flog;

static int env_truthy(const char *v) {
    if (!v || !v[0] || strcmp(v, "0") == 0) {
        return 0;
    }
    if (strcmp(v, "false") == 0 || strcmp(v, "FALSE") == 0 || strcmp(v, "no") == 0 ||
        strcmp(v, "NO") == 0) {
        return 0;
    }
    return 1;
}

static void ensure_debug_dir(void) {
    struct stat st;
    if (stat("retr01_sim/debug", &st) == 0) {
        return;
    }
    (void)mkdir("retr01_sim", 0755);
    (void)mkdir("retr01_sim/debug", 0755);
}

static void flush_xfer(void) {
    char buf[R01S_FLOG_LINE_LEN];
    if (!g_flog.xfer_open || g_flog.xfer_n == 0) {
        g_flog.xfer_open = 0;
        return;
    }
    if (g_flog.xfer_n == 1) {
        snprintf(buf, sizeof(buf), "%s $%06X=%02X", g_flog.xfer_tag, (unsigned)g_flog.xfer_addr0,
                 g_flog.xfer_first);
    } else {
        snprintf(buf, sizeof(buf), "%s $%06X..$%06X n=%u first=%02X last=%02X", g_flog.xfer_tag,
                 (unsigned)g_flog.xfer_addr0, (unsigned)g_flog.xfer_addr1, (unsigned)g_flog.xfer_n,
                 g_flog.xfer_first, g_flog.xfer_last);
    }
    g_flog.xfer_open = 0;
    r01s_frame_log_note(g_flog.xfer_cat, "%s", buf);
}

static void dump_to_file(void) {
    FILE *f;
    int i;
    time_t now = time(NULL);
    ensure_debug_dir();
    f = fopen("retr01_sim/debug/frame1_trace.log", "w");
    if (!f) {
        fprintf(stderr, "1_FRAME_DEBUG: cannot write frame1_trace.log\n");
        return;
    }
    fprintf(f, "retr01_sim 1_FRAME_DEBUG trace\n");
    fprintf(f, "sealed: %s", ctime(&now));
    fprintf(f, "lines: %d  bg_ready=%d sprite_ready=%d\n\n", g_flog.line_count, g_flog.bg_ready,
            g_flog.sprite_ready);
    for (i = 0; i < g_flog.line_count; i++) {
        const R01sFrameLogLine *ln = &g_flog.lines[i];
        if (ln->count > 1) {
            fprintf(f, "[%d] %s  x%u\n", (int)ln->cat, ln->text, (unsigned)ln->count);
        } else {
            fprintf(f, "[%d] %s\n", (int)ln->cat, ln->text);
        }
    }
    fclose(f);
    fprintf(stderr, "1_FRAME_DEBUG: sealed %d lines -> retr01_sim/debug/frame1_trace.log\n",
            g_flog.line_count);
}

void r01s_frame_log_configure_from_env(void) {
    const char *v = getenv("1_FRAME_DEBUG");
    memset(&g_flog, 0, sizeof(g_flog));
    if (!env_truthy(v)) {
        return;
    }
    g_flog.enabled = 1;
    r01s_frame_log_note(R01S_FLOG_SYS, "1_FRAME_DEBUG armed (byte-scale IC log, paginate [ ] or PgUp/PgDn)");
}

int r01s_frame_log_enabled(void) {
    return g_flog.enabled;
}

int r01s_frame_log_active(void) {
    return g_flog.enabled && !g_flog.sealed;
}

int r01s_frame_log_sealed(void) {
    return g_flog.enabled && g_flog.sealed;
}

void r01s_frame_log_note(R01sFrameLogCat cat, const char *fmt, ...) {
    char buf[R01S_FLOG_LINE_LEN];
    va_list ap;
    R01sFrameLogLine *ln;

    if (!g_flog.enabled || g_flog.sealed || !fmt) {
        return;
    }
    flush_xfer();
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (buf[0] == '\0') {
        return;
    }
    if (g_flog.line_count > 0) {
        ln = &g_flog.lines[g_flog.line_count - 1];
        if (ln->cat == cat && strcmp(ln->text, buf) == 0 && ln->count < 65000u) {
            ln->count++;
            return;
        }
    }
    if (g_flog.line_count >= R01S_FLOG_MAX_LINES) {
        /* Keep logging room for seal markers: drop oldest half when full. */
        int keep = R01S_FLOG_MAX_LINES / 2;
        int drop = g_flog.line_count - keep;
        memmove(g_flog.lines, g_flog.lines + drop, (size_t)keep * sizeof(g_flog.lines[0]));
        g_flog.line_count = keep;
        ln = &g_flog.lines[g_flog.line_count++];
        ln->cat = R01S_FLOG_SYS;
        ln->count = 1;
        snprintf(ln->text, sizeof(ln->text), "log trimmed (dropped %d oldest lines)", drop);
        /* fall through to append the new line below if space; else overwrite last */
        if (g_flog.line_count >= R01S_FLOG_MAX_LINES) {
            ln = &g_flog.lines[R01S_FLOG_MAX_LINES - 1];
            ln->cat = cat;
            ln->count = 1;
            snprintf(ln->text, sizeof(ln->text), "%s", buf);
            return;
        }
    }
    ln = &g_flog.lines[g_flog.line_count++];
    ln->cat = cat;
    ln->count = 1;
    snprintf(ln->text, sizeof(ln->text), "%s", buf);
}

void r01s_frame_log_xfer(R01sFrameLogCat cat, const char *tag, uint32_t addr, uint8_t data) {
    if (!g_flog.enabled || g_flog.sealed || !tag) {
        return;
    }
    if (g_flog.xfer_open && g_flog.xfer_cat == cat && strcmp(g_flog.xfer_tag, tag) == 0 &&
        addr == g_flog.xfer_addr1 + 1u) {
        g_flog.xfer_addr1 = addr;
        g_flog.xfer_last = data;
        g_flog.xfer_n++;
        return;
    }
    flush_xfer();
    g_flog.xfer_open = 1;
    g_flog.xfer_cat = cat;
    snprintf(g_flog.xfer_tag, sizeof(g_flog.xfer_tag), "%s", tag);
    g_flog.xfer_addr0 = addr;
    g_flog.xfer_addr1 = addr;
    g_flog.xfer_first = data;
    g_flog.xfer_last = data;
    g_flog.xfer_n = 1;
}

void r01s_frame_log_mark_bg_ready(void) {
    if (!g_flog.enabled || g_flog.bg_ready) {
        return;
    }
    g_flog.bg_ready = 1;
    r01s_frame_log_note(R01S_FLOG_SYS, "BG ready (MAP->VRAM / video hold lifted)");
    r01s_frame_log_try_seal();
}

void r01s_frame_log_mark_sprite_field(void) {
    if (!g_flog.enabled || g_flog.sprite_ready) {
        return;
    }
    g_flog.sprite_ready = 1;
    r01s_frame_log_note(R01S_FLOG_SPR, "sprite field fill complete (120x128)");
    r01s_frame_log_try_seal();
}

void r01s_frame_log_try_seal(void) {
    char seal[R01S_FLOG_LINE_LEN];
    R01sFrameLogLine *ln;

    if (!g_flog.enabled || g_flog.sealed) {
        return;
    }
    if (!g_flog.bg_ready || !g_flog.sprite_ready) {
        return;
    }
    flush_xfer();
    snprintf(seal, sizeof(seal), "SEALED: BG + sprite fields ready (%d lines). [ ] or PgUp/PgDn",
             g_flog.line_count + 1);
    if (g_flog.line_count < R01S_FLOG_MAX_LINES) {
        ln = &g_flog.lines[g_flog.line_count++];
        ln->cat = R01S_FLOG_SYS;
        ln->count = 1;
        snprintf(ln->text, sizeof(ln->text), "%s", seal);
    }
    g_flog.sealed = 1;
    g_flog.page = r01s_frame_log_page_count() - 1;
    if (g_flog.page < 0) {
        g_flog.page = 0;
    }
    dump_to_file();
}

int r01s_frame_log_line_count(void) {
    return g_flog.line_count;
}

const R01sFrameLogLine *r01s_frame_log_line(int index) {
    if (index < 0 || index >= g_flog.line_count) {
        return NULL;
    }
    return &g_flog.lines[index];
}

void r01s_frame_log_cat_rgb(R01sFrameLogCat cat, uint8_t *r, uint8_t *g, uint8_t *b) {
    static const uint8_t rgb[R01S_FLOG_CAT_N][3] = {
        {200, 200, 200}, /* SYS */
        {120, 200, 255}, /* CPU */
        {255, 180, 80},  /* FLASH */
        {255, 220, 100}, /* MAP */
        {160, 255, 160}, /* VRAM */
        {200, 160, 255}, /* IO */
        {255, 140, 180}, /* BEAM */
        {140, 220, 180}, /* BG0 */
        {255, 120, 120}, /* SPR */
        {180, 200, 255}, /* VIDEO */
        {220, 200, 140}, /* PLAY */
    };
    if (!r || !g || !b) {
        return;
    }
    if ((int)cat < 0 || cat >= R01S_FLOG_CAT_N) {
        cat = R01S_FLOG_SYS;
    }
    *r = rgb[cat][0];
    *g = rgb[cat][1];
    *b = rgb[cat][2];
}

int r01s_frame_log_page(void) {
    return g_flog.page;
}

int r01s_frame_log_page_count(void) {
    int n = g_flog.line_count;
    if (n <= 0) {
        return 1;
    }
    return (n + R01S_FLOG_PAGE_LINES - 1) / R01S_FLOG_PAGE_LINES;
}

void r01s_frame_log_page_delta(int delta) {
    int pages = r01s_frame_log_page_count();
    if (!g_flog.enabled) {
        return;
    }
    g_flog.page += delta;
    if (g_flog.page < 0) {
        g_flog.page = 0;
    }
    if (g_flog.page >= pages) {
        g_flog.page = pages - 1;
    }
}

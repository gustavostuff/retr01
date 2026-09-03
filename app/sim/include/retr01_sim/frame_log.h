#ifndef retr01_SIM_FRAME_LOG_H
#define retr01_SIM_FRAME_LOG_H

#include <stdint.h>

/*
 * One-frame transaction log (env 1_FRAME_DEBUG=true).
 * Byte-scale events with coalescing, not bit toggles.
 * Stops after BG (MAP->VRAM) is done and the first sprite field fill completes.
 * UI paginates; dump also written to retr01_sim/debug/frame1_trace.log when sealed.
 */

typedef enum R01sFrameLogCat {
    R01S_FLOG_SYS = 0,
    R01S_FLOG_CPU,   /* W65C02 / PRG */
    R01S_FLOG_FLASH, /* SST39SF040 CE owners */
    R01S_FLOG_MAP,   /* $FE93 MAP stream */
    R01S_FLOG_VRAM,  /* $FE12 VRAM */
    R01S_FLOG_IO,    /* other $FExx */
    R01S_FLOG_BEAM,  /* raster / VBlank / HBlank */
    R01S_FLOG_BG0,   /* HBlank BG0 line fill */
    R01S_FLOG_SPR,   /* VBlank sprite field */
    R01S_FLOG_VIDEO, /* compositor / LCD */
    R01S_FLOG_PLAY,  /* Host Play OAM / scroll */
    R01S_FLOG_CAT_N
} R01sFrameLogCat;

#define R01S_FLOG_MAX_LINES 8192
#define R01S_FLOG_LINE_LEN 112
#define R01S_FLOG_PAGE_LINES 18

typedef struct R01sFrameLogLine {
    R01sFrameLogCat cat;
    uint16_t count; /* coalesced repeats (>=1) */
    char text[R01S_FLOG_LINE_LEN];
} R01sFrameLogLine;

void r01s_frame_log_configure_from_env(void);
int r01s_frame_log_enabled(void);
int r01s_frame_log_active(void); /* enabled and not yet sealed */
int r01s_frame_log_sealed(void);

void r01s_frame_log_note(R01sFrameLogCat cat, const char *fmt, ...);
/* Sequential byte xfer: coalesces addr runs under the same tag. */
void r01s_frame_log_xfer(R01sFrameLogCat cat, const char *tag, uint32_t addr, uint8_t data);

void r01s_frame_log_mark_bg_ready(void);     /* MAP->VRAM catchup / hold lift */
void r01s_frame_log_mark_sprite_field(void); /* first full sprite field fill */
void r01s_frame_log_try_seal(void);          /* seal when BG+sprites ready */

int r01s_frame_log_line_count(void);
const R01sFrameLogLine *r01s_frame_log_line(int index);
void r01s_frame_log_cat_rgb(R01sFrameLogCat cat, uint8_t *r, uint8_t *g, uint8_t *b);

int r01s_frame_log_page(void);
int r01s_frame_log_page_count(void);
void r01s_frame_log_page_delta(int delta);

#endif

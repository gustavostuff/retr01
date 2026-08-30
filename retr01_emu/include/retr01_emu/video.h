#ifndef retr01_EMU_VIDEO_H
#define retr01_EMU_VIDEO_H

#include "retr01_emu/cart.h"
#include "retr01_emu/types.h"

#include <stdint.h>

struct R01eMachine;

typedef struct R01eVideo {
    uint8_t vram[R01E_VRAM_BYTES];
    uint8_t chr[8][R01E_CHR_BANK_BYTES]; /* BG0-3 + SPR0-3 for active world */
    int chr_loaded;
    /* 1 if VRAM camera slot 0-3 holds a present screen (else backdrop). */
    uint8_t slot_present[4];

    /* 2x2 camera workbench origin in world grid (docs/graphics). */
    int cam_origin_col;
    int cam_origin_row;
    int cam_x;
    int cam_y;
    int cam_max_x;
    int cam_max_y;

    uint8_t fb[R01E_VISIBLE_W * R01E_VISIBLE_H * 3]; /* SCALE 2x RGB */
    /* Debug: 2x2 VRAM workbench at 1:1 (256x240). */
    uint8_t vram_atlas[R01E_VRAM_ATLAS_W * R01E_VRAM_ATLAS_H * 3];
    /* Expanded H-band slice table: additive dx per logical row (docs/graphics). Unused = 0. */
    int8_t plane_h_slice[R01E_PARALLAX_SLICE_MAX];
} R01eVideo;

void r01e_video_kit_rgb(int master_index, uint8_t *r, uint8_t *g, uint8_t *b);

void r01e_video_reset(R01eVideo *vid);

/* Copy one global palette row (BG+SPR) into the active $FE08/$FE09 buffer. */
void r01e_video_load_active_pals(struct R01eMachine *m);

/* R01E_SOFTBOOT=1 enables host memcpy soft-boot (debug). Default = off. */
int r01e_video_softboot_enabled(void);

/*
 * Load CHR + camera bounds for a world. Does not fill VRAM/pals (PRG or softboot does).
 */
int r01e_video_prepare_world(struct R01eMachine *m, int world);

/*
 * Soft-boot world assets from cart (opt-in via R01E_SOFTBOOT=1).
 * Loads CHR, palettes, 2x2 screen workbench into VRAM slots 0-3.
 */
int r01e_video_boot_world(struct R01eMachine *m, int world);

/* Reload VRAM slots 0-3 at cam_origin; mirror scroll regs from camera. */
int r01e_video_sync_camera(struct R01eMachine *m);

/*
 * Host atlas pan (tests / debug). dx/dy in logical pixels.
 */
int r01e_video_host_pan(struct R01eMachine *m, int dx, int dy);

/* Full-frame BG (VRAM+scroll) + OAM sprite composite. SCALE 2x RGB. */
void r01e_video_render_frame(struct R01eMachine *m);

/* Render 2x2 VRAM slots into vram_atlas (256x240, 1:1). */
void r01e_video_render_vram_atlas(struct R01eMachine *m);

/* Phase 2+: load parallax payloads into VRAM slots 4-5. */
void r01e_video_load_parallax(struct R01eMachine *m, const R01eWorldView *wv);

/*
 * Expanded H-band plane slices (max R01E_PARALLAX_SLICE_MAX rows).
 * Authoring uses 1..120 variable-thickness bands; runtime stores per-row dx.
 */
void r01e_video_plane_slices_clear(R01eVideo *vid);
void r01e_video_plane_slice_set(R01eVideo *vid, int row, int8_t dx);
int8_t r01e_video_plane_slice_get(const R01eVideo *vid, int row);

#endif

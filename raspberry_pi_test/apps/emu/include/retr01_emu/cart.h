#ifndef retr01_EMU_CART_H
#define retr01_EMU_CART_H

#include "retr01_emu/types.h"

#include <stddef.h>
#include <stdint.h>

typedef struct R01eCart {
    uint8_t *data;
    size_t len;
    uint8_t format_ver;
    uint8_t world_count;
    uint8_t flags;
    uint32_t off_prg;
    uint32_t len_prg;
    uint32_t off_pal_bg;
    uint32_t len_pal_bg;
    uint32_t off_pal_spr;
    uint32_t len_pal_spr;
    uint32_t off_chr;
    uint32_t len_chr;
    uint32_t off_entities;
    uint32_t len_entities;
    uint32_t off_other;
    uint32_t len_other;
    uint32_t off_world_table;
    uint32_t len_world_table;
    uint32_t off_credits;
    uint32_t len_credits;
} R01eCart;

typedef struct R01eWorldView {
    int present;
    uint32_t base; /* absolute offset of world blob in cart */
    uint32_t len;
    uint8_t start_col;
    uint8_t start_row;
    uint8_t default_bg_bank;
    uint8_t default_pal_row;
    uint8_t screen_count;
    uint8_t bg0_count; /* L0 / structured second BG present screens (hdr[6]; was parallax) */
    uint8_t bg0_cols;  /* BG0 authored grid W (hdr[3] low nibble; 0 = infer from dir) */
    uint8_t bg0_rows;  /* BG0 authored grid H (hdr[3] high nibble) */
    uint32_t off_chr; /* relative to world base */
    uint32_t off_screen_dir;
    uint32_t off_bg0_dir; /* 12 B/entry like screen dir; 0 if none */
    /* Phase 3D entity type catalog (relative to world base). Placements in PRG. */
    uint8_t entity_type_count;
    uint8_t entity_inst_count; /* always 0 in cart; PRG holds placements */
    uint32_t off_entity_types;
    uint32_t off_entity_insts; /* end of types / start of optional PA blob */
    /* Play player type index, or R01E_CART_PLAYER_ENTITY_NONE. Hitbox from state 0. */
    uint8_t player_entity;
    uint8_t player_hit_x;
    uint8_t player_hit_y;
    uint8_t player_hit_w;
    uint8_t player_hit_h;
    uint8_t world_flags;
    uint8_t cam_deadzone_x;
    uint8_t cam_deadzone_y;
    uint32_t off_player_anim; /* relative to world base; valid when has_player_anim */
    int has_player_anim;
} R01eWorldView;

/* Shared world-header offsets live in r01_cart_caps.h (via types.h). */
#define R01E_CART_WHDR_TYPE_COUNT R01_CART_WHDR_TYPE_COUNT
#define R01E_CART_WHDR_INST_COUNT R01_CART_WHDR_INST_COUNT
#define R01E_CART_WHDR_OFF_TYPES R01_CART_WHDR_OFF_TYPES
#define R01E_CART_WHDR_OFF_INSTS R01_CART_WHDR_OFF_INSTS
#define R01E_CART_WHDR_PLAYER_ENTITY R01_CART_WHDR_PLAYER_ENTITY
#define R01E_CART_WHDR_PLAYER_HIT_X R01_CART_WHDR_PLAYER_HIT_X
#define R01E_CART_WHDR_PLAYER_HIT_Y R01_CART_WHDR_PLAYER_HIT_Y
#define R01E_CART_WHDR_PLAYER_HIT_W R01_CART_WHDR_PLAYER_HIT_W
#define R01E_CART_WHDR_PLAYER_HIT_H R01_CART_WHDR_PLAYER_HIT_H
#define R01E_CART_WHDR_CAM_DEADZONE_X R01_CART_WHDR_CAM_DEADZONE_X
#define R01E_CART_WHDR_CAM_DEADZONE_Y R01_CART_WHDR_CAM_DEADZONE_Y
#define R01E_CART_WHDR_FLAGS R01_CART_WHDR_FLAGS
#define R01E_CART_WHDR_FLAG_PLAYER_ANIM R01_CART_WHDR_FLAG_PLAYER_ANIM
#define R01E_CART_WHDR_FLAG_BG0_WRAP_X R01_CART_WHDR_FLAG_BG0_WRAP_X
#define R01E_CART_WHDR_FLAG_BG0_WRAP_Y R01_CART_WHDR_FLAG_BG0_WRAP_Y
#define R01E_CART_WHDR_FLAG_BG0_CLIP_BG1 R01_CART_WHDR_FLAG_BG0_CLIP_BG1
#define R01E_CART_WHDR_FLAG_PLATFORMER R01_CART_WHDR_FLAG_PLATFORMER
#define R01E_CART_PLAYER_ENTITY_NONE R01_CART_PLAYER_ENTITY_NONE
#define R01E_CART_ENTITY_PARTS_MAX R01_CART_ENTITY_PARTS_MAX
#define R01E_CART_ENTITY_DEF_MAX R01_CART_ENTITY_DEF_MAX
#define R01E_CART_INSTANCE_SIZE R01_CART_INSTANCE_SIZE

#define R01E_CART_OTHER_TITLE R01_CART_OTHER_TITLE
#define R01E_CART_OTHER_INTER R01_CART_OTHER_INTER
#define R01E_CART_OTHER_DIR_ID R01_CART_OTHER_DIR_ID
#define R01E_CART_OTHER_DIR_FLAGS R01_CART_OTHER_DIR_FLAGS
#define R01E_CART_OTHER_DIR_LEN R01_CART_OTHER_DIR_LEN
#define R01E_CART_OTHER_DIR_OFF R01_CART_OTHER_DIR_OFF

/* Load packed .retr01 (or 512 KB flash image). Owns *out->data. */
int r01e_cart_load_path(R01eCart *out, const char *path, char *err, size_t err_cap);
int r01e_cart_load_mem(R01eCart *out, const uint8_t *img, size_t len, char *err, size_t err_cap);
void r01e_cart_free(R01eCart *c);

const uint8_t *r01e_cart_prg(const R01eCart *c);
int r01e_cart_world(const R01eCart *c, int index, R01eWorldView *out);

/* EntityDef catalog (docs/general/software-api.md). */
const uint8_t *r01e_cart_entity_def(const R01eCart *c, const R01eWorldView *wv, int type_id);
int r01e_cart_entity_frame(const uint8_t *def, int state, int frame, const uint8_t **out_sprites,
                           int *out_count);

/* Return 1 if world has a screen at grid col,row. */
int r01e_cart_has_screen(const R01eCart *c, int world, int col, int row);

/* BG attr at world pixel; -1 if no screen. */
int r01e_cart_attr_at(const R01eCart *c, int world, int wx, int wy, uint8_t *out_attr);

/* BG1 cell is solid when its bank+tile is in the packed pattern list (PRG $8700). */
int r01e_cart_solid_at(const R01eCart *c, int world, int wx, int wy);

/* Player AABB vs present screens and BG solid (Studio play.c SoT). */
int r01e_cart_player_aabb_ok(const R01eCart *c, int world, int px, int py);

/* General AABB (bw x bh) vs present screens and all overlapping BG solid tiles. */
int r01e_cart_aabb_ok(const R01eCart *c, int world, int px, int py, int bw, int bh);

/* Absolute byte in cart image, or NULL if OOB. */
const uint8_t *r01e_cart_ptr(const R01eCart *c, uint32_t abs_off, size_t need);

uint8_t r01e_cart_read(const R01eCart *c, uint32_t abs_off);

/* Raw other-screen payload (may be RLE). len/flags optional. */
const uint8_t *r01e_cart_other_raw(const R01eCart *c, int id, size_t *out_len, int *out_flags);

/* Decode other screen id into 480 B (tiles||attrs). 0 ok, -1 missing/bad. */
int r01e_cart_other_decode(const R01eCart *c, int id, uint8_t out[R01E_SCREEN_PAYLOAD]);

/* Deprecated alias: raw payload only when uncompressed; prefer decode. */
const uint8_t *r01e_cart_other_payload(const R01eCart *c, int id);

#endif

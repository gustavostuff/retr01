#ifndef R01_PLAY_ANIM_CART_H
#define R01_PLAY_ANIM_CART_H

#include "r01_play_anim.h"

#include <stddef.h>
#include <stdint.h>

#define R01_CART_WHDR_FLAGS 7
#define R01_CART_WHDR_FLAG_PLAYER_ANIM 0x01u
#define R01_CART_WHDR_FLAG_BG0_WRAP_X 0x02u
#define R01_CART_WHDR_FLAG_BG0_WRAP_Y 0x04u
#define R01_CART_WHDR_FLAG_BG0_CLIP_BG1 0x08u
#define R01_CART_WHDR_FLAG_PLATFORMER 0x10u

#define R01_CART_PLAYER_ANIM_MAGIC0 'P'
#define R01_CART_PLAYER_ANIM_MAGIC1 'A'

#define R01_CART_PLAYER_ANIM_PARTS_MAX 6
#define R01_CART_OAM_FLIP_H 0x40u
#define R01_CART_OAM_FLIP_V 0x80u

typedef struct R01CartPlayerAnim {
    const uint8_t *blob;
    size_t len;
    int state_count;
} R01CartPlayerAnim;

int r01_cart_player_anim_parse(const uint8_t *blob, size_t len, R01CartPlayerAnim *out);

int r01_cart_player_anim_state_count(const R01CartPlayerAnim *anim);

/* Returns 0 on success; state header is 1 byte at *out_state (drawable_count). */
int r01_cart_player_anim_state_hdr(const R01CartPlayerAnim *anim, int state_idx, const uint8_t **out_state);

int r01_cart_player_anim_drawable_count(const R01CartPlayerAnim *anim, int state_idx);

/* Frame header is 8 bytes: origin_x/y, state's compose hitbox xywh, part_count, delay. Returns NULL if slot missing. */
const uint8_t *r01_cart_player_anim_frame_hdr(const R01CartPlayerAnim *anim, int state_idx, int frame_slot);

/* Parts are tile, attr, dx, dy tuples; returns NULL if slot missing. */
const uint8_t *r01_cart_player_anim_frame_parts(const R01CartPlayerAnim *anim, int state_idx, int frame_slot,
                                              int *out_part_count);

void r01_cart_part_pose(int origin_x, int origin_y, int part_dx, int part_dy, uint8_t attr, int inst_flip_h,
                        int inst_flip_v, int *out_dx, int *out_dy, uint8_t *out_attr);

void r01_play_anim_tick_cart(R01PlayAnimCtx *ctx, const R01CartPlayerAnim *anim);

#endif

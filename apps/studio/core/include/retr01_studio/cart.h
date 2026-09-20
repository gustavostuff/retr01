#ifndef retr01_STUDIO_CART_H
#define retr01_STUDIO_CART_H

#include "retr01_studio/types.h"

/* Phase 3D: entity *type* catalog in world header; placements live in PRG. */
#define R01_CART_WHDR_TYPE_COUNT 17
#define R01_CART_WHDR_INST_COUNT 18 /* always 0; placements are PRG-side */
#define R01_CART_WHDR_OFF_TYPES 19
#define R01_CART_WHDR_OFF_INSTS 22 /* points at end of types (PA blob start) */
/* Play player: type index, or 0xFF = CHR stub. Hitbox snapshot from that type's state 0 frame 0. */
#define R01_CART_WHDR_FLAGS 7
#define R01_CART_WHDR_FLAG_PLAYER_ANIM 0x01u
#define R01_CART_WHDR_FLAG_BG0_WRAP_X 0x02u
#define R01_CART_WHDR_FLAG_BG0_WRAP_Y 0x04u
#define R01_CART_WHDR_FLAG_BG0_CLIP_BG1 0x08u
#define R01_CART_WHDR_FLAG_PLATFORMER 0x10u
#define R01_CART_PLAYER_ANIM_MAGIC0 'P'
#define R01_CART_PLAYER_ANIM_MAGIC1 'A'
#define R01_CART_WHDR_PLAYER_ENTITY 25
#define R01_CART_WHDR_PLAYER_HIT_X 26
#define R01_CART_WHDR_PLAYER_HIT_Y 27
#define R01_CART_WHDR_PLAYER_HIT_W 28
#define R01_CART_WHDR_PLAYER_HIT_H 29
#define R01_CART_WHDR_CAM_DEADZONE_X 30
#define R01_CART_WHDR_CAM_DEADZONE_Y 31
#define R01_CART_PLAYER_ENTITY_NONE 0xFFu
#define R01_CART_ENTITY_PARTS_MAX 6
#define R01_CART_ENTITY_STATES_MAX 4
#define R01_CART_ENTITY_FRAMES_MAX 8
#define R01_CART_ENTITY_DEF_MAX 1044 /* locked fully-populated size (general_docs/software-api.md) */
#define R01_CART_ENTITY_FRAME_DELAY_DEFAULT 1
#define R01_CART_INSTANCE_SIZE 6 /* type_id, flags (bit0 flip_h, bit1 flip_v), world_x u16 LE, world_y u16 LE */

/* Other-screens dir entry (relative off_payload to other blob base). */
#define R01_CART_OTHER_DIR_ID 0
#define R01_CART_OTHER_DIR_FLAGS 1
#define R01_CART_OTHER_DIR_LEN 2
#define R01_CART_OTHER_DIR_OFF_PAYLOAD 4

void r01_prom_fill(uint8_t out64[R01_MASTER_COLORS]);
int r01_prom_write(const char *path, char *err_buf, size_t err_cap);
int r01_prg_write_asm(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_cart_write_flash(const R01Project *p, const char *path, char *err_buf, size_t err_cap);
int r01_export_bundle(const R01Project *p, const char *path_stem, char *err_buf, size_t err_cap);

#endif

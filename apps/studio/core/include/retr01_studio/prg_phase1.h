#ifndef retr01_STUDIO_PRG_PHASE1_H
#define retr01_STUDIO_PRG_PHASE1_H

#include "retr01_studio/types.h"

/* Flash layout fields needed to patch MAP/palette boot seeks in PRG. */
typedef struct R01PrgCartLayout {
    uint32_t off_pal_bg;
    uint32_t len_pal_bg;
    uint32_t off_pal_spr;
    uint32_t len_pal_spr;
    uint32_t off_map_screen0;
    uint32_t off_bgm;
    uint32_t off_world0;
    uint8_t default_pal_row;
} R01PrgCartLayout;

#define R01_PRG_BOOTMAP_OFF 0x00E0u
#define R01_PRG_BOOTMAP_BYTES 16u
#define R01_PRG_C_OFF 0x4400u /* CPU $C400 */
#define R01_PRG_COLL_GRID_OFF 0x0500u /* CPU $8500, 16x16 u16 probe addrs */

#define R01_PLAY_SOLID_RAM 0x0200u
#define R01_PLAY_SOLID_LIST_CPU 0x8700u

#define R01_PRG_PLAY_INST_COUNT_OFF 0x01C0u
#define R01_PRG_PLAY_INST_TABLE_OFF 0x01C1u
#define R01_PRG_PLAY_SPAWN_CELL_OFF 0x0120u
#define R01_PRG_R01P_OFF 0x00F0u
#define R01_PRG_R01P_VER 5u /* C runtime tables; gameplay is llvm-mos PRG */
#define R01_PRG_PLAT_GRAVITY_OFF 0x00F7u
#define R01_PRG_PLAT_JUMP_OFF 0x00F8u
#define R01_PRG_PLAT_METER_OFF 0x00F9u
#define R01_PRG_PLAT_CROUCH_OFF 0x00FAu
#define R01_PRG_PLAYER_ANIM_IDLE_OFF 0x00FBu
#define R01_PRG_PLAYER_ANIM_WALK_OFF 0x00FCu
#define R01_PRG_PLAYER_ANIM_JUMP_OFF 0x00FDu

/* Overlay present/spawn/instance/solid tables and boot MAP offsets. Does not wipe C code. */
void r01_prg_overlay_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout);

int r01_prg_needs_rebuild(const char *prg_path, const char *logic_c);

/* Overlay onto a zeroed 32 KB buffer (tests). */
void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout);

/* llvm-mos compile of game_logic.c into 32 KB PRG. */
int r01_prg_compile_sdk(const char *logic_c, uint8_t prg[R01_PRG_BYTES], const char *out_prg_path, char *err_buf,
                        size_t err_cap);
int r01_prg_load_or_compile(const char *cart_path, uint8_t prg[R01_PRG_BYTES], char *err_buf, size_t err_cap);

#endif

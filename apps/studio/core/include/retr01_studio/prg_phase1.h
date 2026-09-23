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
#define R01_PRG_PLAY_TAB_BYTES 0x0400u /* CPU $8100-$84FF */
#define R01_PRG_COLLGRID_BYTES 0x0200u
#define R01_PRG_R01P_BYTES 16u
#define R01_PRG_SOLIDS_MAX (R01_PRG_C_OFF - 0x0700u)

#define R01_PLAY_SOLID_RAM 0x0200u
#define R01_PLAY_SOLID_LIST_CPU 0x8700u

#define R01_PRG_PLAY_INST_COUNT_OFF 0x01C0u
#define R01_PRG_PLAY_INST_TABLE_OFF 0x01C1u
#define R01_PRG_PLAY_SPAWN_CELL_OFF 0x0120u
#define R01_PRG_R01P_OFF 0x00F0u
#define R01_PRG_R01P_VER 5u /* C runtime tables; gameplay is llvm-mos PRG */
/* $80F7-$80FE reserved. Live platformer / anim / BGM start are author C RAM. */
#define R01_PRG_PLAT_GRAVITY_OFF 0x00F7u
#define R01_PRG_PLAT_JUMP_OFF 0x00F8u
#define R01_PRG_PLAT_METER_OFF 0x00F9u
#define R01_PRG_PLAT_CROUCH_OFF 0x00FAu
#define R01_PRG_PLAYER_ANIM_IDLE_OFF 0x00FBu
#define R01_PRG_PLAYER_ANIM_WALK_OFF 0x00FCu
#define R01_PRG_PLAYER_ANIM_JUMP_OFF 0x00FDu

/* Present/spawn/coll/instances/R01P. No boot MAP. */
void r01_prg_fill_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p);
/* Last-step 16 B boot MAP after cart layout. */
void r01_prg_patch_boot_map(uint8_t prg[R01_PRG_BYTES], const R01PrgCartLayout *layout);
/* Linker bins: play8100.bin, collgrid.bin, solids.bin, r01p.bin. */
int r01_prg_write_table_bins(const R01Project *p, const char *data_dir, char *err_buf, size_t err_cap);
/* Tests: fill_tables + patch_boot_map. Packer does not stamp tables. */
void r01_prg_overlay_tables(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout);

int r01_prg_needs_rebuild(const char *prg_path, const char *logic_c);

/* Zeroed 32 KB then fill_tables + boot MAP (tests). */
void r01_prg_fill_phase1(uint8_t prg[R01_PRG_BYTES], const R01Project *p, const R01PrgCartLayout *layout);

/* llvm-mos compile of game_logic.c into 32 KB PRG. */
int r01_prg_compile_sdk(const char *logic_c, uint8_t prg[R01_PRG_BYTES], const char *out_prg_path, char *err_buf,
                        size_t err_cap);
int r01_prg_load_or_compile(const char *cart_path, uint8_t prg[R01_PRG_BYTES], char *err_buf, size_t err_cap);

#endif

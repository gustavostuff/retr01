#ifndef retr01_SIM_BOARD_H
#define retr01_SIM_BOARD_H

#include "at27c256r.h"
#include "as6c62256.h"
#include "avr128db28_m.h"
#include "avr128db28_s1.h"
#include "avr128db28_s2.h"
#include "attiny85.h"
#include "atf22v10.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "compositor.h"
#include "i2c_eeprom.h"
#include "osc8m.h"
#include "osc_dot.h"
#include "pads.h"
#include "pad_uart.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "retr01_sim/bom32.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "retr01_sim/types.h"
#include "sn74hc157.h"
#include "sn74hc573.h"
#include "sn74hc574.h"
#include "integration.h"
#include "sprite_fetch.h"
#include "retr01_sim/cart_module.h"
#include "retr01_sim/cart_slot.h"
#include "sst39sf040.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdint.h>

/* Combinatorial settle passes per wire/eval half-step (PLD/glue depth). */
#define R01S_SETTLE_PASSES 2

/* 9 canvas islands: motherboard + detachable cart. Soft $7Fxx on MCU-M. */
enum {
    R01S_ISLAND_VIDEO = 0,     /* O: LCD / RGBS (top-left) */
    R01S_ISLAND_POWER_CLK = 1, /* A+B: 5V + canned PHI2 */
    R01S_ISLAND_CPU = 2,       /* C: CPU + RAM (decode folded helper) */
    R01S_ISLAND_VRAM = 3,      /* G */
    R01S_ISLAND_BEAM = 4,      /* H: beam + HC574 scroll X */
    R01S_ISLAND_CART = 5,      /* J: console cart socket */
    R01S_ISLAND_APU = 6,       /* K: MCU-S2 pads+APU */
    R01S_ISLAND_MCU_LB = 7,    /* L: MCU-M + MCU-S1 + field + HC573 */
    R01S_ISLAND_CART_MOD = 8,  /* N: detachable cart module (SST39SF040 + 24C64) */
    R01S_ISLAND_COUNT = 9,
};

typedef struct R01sIslandPowerClkImpl {
    R01sPwr5v *pwr;
    R01sOsc8m *osc;
} R01sIslandPowerClkImpl;

typedef struct R01sIslandCpuMemImpl {
    R01sW65C02S *cpu;
    R01sAs6c62256 *ram;
    R01sPrgRom *prg;
    R01sAtf22v10 *pld_decode; /* non-BOM helper */
} R01sIslandCpuMemImpl;

typedef struct R01sIslandPadsImpl {
    R01sPads *pads;
} R01sIslandPadsImpl;

typedef struct R01sIslandVramImpl {
    R01sAs6c62256 *vram;
    R01sSn74hc157 *mux157[R01S_BOM_HC157_N];
    R01sBgFetch *bg_pld;
    R01sAtf22v10 *pld_vram; /* non-BOM helper */
} R01sIslandVramImpl;

typedef struct R01sIslandBeamImpl {
    R01sOscDot *osc_dot;
    R01sBeamXy *beam_x;
    R01sAtf22v10 *beam_y;
    R01sSn74hc574 *scroll_x;
} R01sIslandBeamImpl;

typedef struct R01sIslandBgFetchImpl {
    R01sBgFetch *fetch;
} R01sIslandBgFetchImpl;

typedef struct R01sIslandVideoImpl {
    R01sCompositor *comp;
    R01sAt27c256r *prom;
    R01sVideoSink *sink;
} R01sIslandVideoImpl;

typedef struct R01sIslandCartImpl {
    R01sSst39sf040 *flash;
    R01sI2cEeprom *save_eeprom;
} R01sIslandCartImpl;

typedef struct R01sIslandApuImpl {
    R01sAvr128db28S2 *apu;
} R01sIslandApuImpl;

typedef struct R01sIslandMcuLbImpl {
    R01sAvr128db28M *mcu_m;
    R01sAvr128db28S1 *mcu_s1;
    R01sAs6c62256 *sram;
    R01sSn74hc573 *field_ale;
} R01sIslandMcuLbImpl;

typedef struct R01sIslandSpritesImpl {
    R01sSpriteFetch *fetch;
} R01sIslandSpritesImpl;

typedef struct R01sIslandIntegrationImpl {
    R01sIntegration *integ;
} R01sIslandIntegrationImpl;

/* Soft $7Fxx. 74HC14 is not seated. AD724 is not in this tree yet. */
typedef struct R01sBoard {
    /* Support. 74HC14 stays off while PHI2 is a canned square clock. */
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sOscDot osc_dot;
    R01sVideoSink video_sink;
    R01sPrgRom prg;
    R01sPads pads;
    R01sAttiny85 pad_mcu[2];
    R01sSpriteFetch sprite_fetch;
    R01sIntegration integration;
    /* Motherboard silicon plus cart flash and save. */
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sAs6c62256 vram;
    R01sAs6c62256 linebuf;
    R01sCartModule cart_module;
    R01sCartSlotMgr cart_slot;
    R01sAvr128db28S2 mcu_s2;
    R01sAvr128db28M mcu_m;
    R01sAvr128db28S1 mcu_s1;
    R01sAt27c256r color_prom;
    R01sSn74hc157 mux157[R01S_BOM_HC157_N];
    R01sSn74hc573 field_ale;
    R01sSn74hc574 scroll_x;
    R01sAtf22v10 pld_decode;
    R01sAtf22v10 pld_vram;
    R01sBeamXy pld_beam_x;
    R01sAtf22v10 pld_beam_y;
    R01sBgFetch bg_fetch;
    R01sCompositor compositor;
    R01sIslandCartModuleImpl cart_mod_impl;
    R01sIslandPowerClkImpl power_clk_impl;
    R01sIslandCpuMemImpl cpu_mem_impl;
    R01sIslandPadsImpl pads_impl;
    R01sIslandVramImpl vram_impl;
    R01sIslandBeamImpl beam_impl;
    R01sIslandBgFetchImpl bg_fetch_impl;
    R01sIslandVideoImpl video_impl;
    R01sIslandCartImpl cart_impl;
    R01sIslandApuImpl apu_impl;
    R01sIslandMcuLbImpl mcu_lb_impl;
    R01sIslandSpritesImpl sprites_impl;
    R01sIslandIntegrationImpl integration_impl;
    /* Soft $7Fxx register file. */
    uint8_t fe00_ctrl;
    uint8_t fe02_scroll_x;
    uint8_t fe03_scroll_y;
    uint8_t fe04_raster_y;
    uint8_t fe05_raster_ctrl;
    uint8_t fe06_bg0_x;
    uint8_t fe07_bg0_y;
    uint8_t cart_a14_18; /* optional (map_addr>>14)&0x1F */
    /* VRAM addr soft ($7F10|$7F11); $7F12 auto-inc via poke. */
    uint16_t vram_addr;
    int vram_fe12_armed;
    /* MAP seek soft ($7F90|$7F91|$7F92); $7F93 auto-inc via poke. */
    uint32_t map_addr;
    int map_fe93_armed;
    /* Flash /CE owner: PRG, MAP, and CHR are mutually exclusive. */
    uint8_t flash_ce_owner;
    /* Cart image metadata (flash absolute offsets). */
    uint32_t cart_off_prg;
    uint32_t cart_len_prg;
    uint32_t cart_off_chr;         /* world-0 CHR base; 0 = stub tile&0x3F */
    uint32_t cart_off_map_screen0; /* absolute MAP payload for world start screen */
    uint32_t cart_off_pal_bg;
    uint32_t cart_len_pal_bg;
    uint32_t cart_off_pal_spr;
    uint32_t cart_len_pal_spr;
    uint8_t cart_default_pal_row; /* 0..7 into global planes */
    uint32_t cart_world_base;      /* world-0 blob base in flash */
    uint32_t cart_off_sdir;        /* absolute screen directory */
    uint8_t cart_screen_count;
    uint8_t cart_start_col;
    uint8_t cart_start_row;
    uint8_t cart_bg0_count;       /* BG0 present count (hdr[6]) */
    uint8_t cart_bg0_cols_hdr;    /* authored extent nibble (informational) */
    uint8_t cart_bg0_rows_hdr;
    uint32_t cart_off_bg0_dir;    /* absolute BG0 dir, 0 if none */
    /* Phase 3D entity tables (absolute flash offsets). */
    uint8_t cart_entity_type_count;
    uint8_t cart_entity_inst_count;
    uint32_t cart_off_entity_types;
    uint32_t cart_off_entity_insts;
    uint8_t cart_player_entity; /* 0xFF = stub */
    uint8_t cart_player_hit_x;
    uint8_t cart_player_hit_y;
    uint8_t cart_player_hit_w;
    uint8_t cart_player_hit_h;
    uint8_t cart_world_flags;
    uint8_t cart_cam_deadzone_x;
    uint8_t cart_cam_deadzone_y;
    uint32_t cart_off_player_anim; /* absolute flash offset; 0 if none */
    /* PRG spawn table copied before the boot overlay NOPs $8000. */
    uint8_t cart_prg_spawn_n;
    uint8_t cart_prg_spawn[64 * 6];
    /* PRG $8700 solid patterns: bank, tile pairs. Copied before the boot overlay. */
    uint8_t cart_solid_n;
    uint8_t cart_solid[32 * 2];
    uint8_t cart_format_ver;
    uint32_t cart_off_other;
    uint32_t cart_len_other;
    uint32_t cart_off_other_title;
    uint32_t cart_off_other_inter;
    uint32_t cart_off_credits;
    uint32_t cart_len_credits;
    int cart_loaded;
    char cart_label[48];
    char cart_path[256];
    /* Active palette RAM (soft); addr index from soft $7F08. */
    uint8_t active_pal[32];
    uint8_t pal_addr;
    int pal_fe09_wrote; /* one write+inc per DATA cycle */
    int soft_fe21_wrote; /* OAM data auto-inc once per DATA cycle */
    uint8_t chr_last_master; /* hold last BG/sprite master when CHR CE denied */
    /* 2x2 workbench: 1 = screen dir hit loaded into slot (absent -> backdrop / L0). */
    uint8_t vram_slot_present[4];
    /* Host Play BG0 cache (cart-backed show-through under BG1 color 0). Not IC path. */
    struct {
        uint8_t present;
        uint8_t col;
        uint8_t row;
        uint8_t map[R01S_CART_SCREEN_PAYLOAD];
    } bg0[R01S_BG0_SCREENS_MAX];
    int bg0_count;
    int bg0_cols; /* present BG0 bbox */
    int bg0_rows;
    int l1_cols; /* present BG1 bbox */
    int l1_rows;
    int l1_origin_x;
    int l1_origin_y;
    int cam_max_x; /* Host Play clamp (emu cam_max_*: max present screen * px) */
    int cam_max_y;
    int l0_cam_x;
    int l0_cam_y;
    /* Host Play scaffold (enabled after catchup). */
    R01sPlay play;
    int catchup_cancel; /* cooperative cancel for threaded IC catchup */
    int reset_hold;
    int reset_nmi_pulse; /* post-reset NMI strobe (NES reset+ hook) */
    uint32_t cycles;
    R01sLevel phi2_prev;
    /* Bring-up milestones (sticky, cleared on reset). */
    uint8_t health_saw_fexx;
    uint8_t health_saw_vram;
    uint8_t health_saw_vram_read;
    uint8_t health_saw_pad;
    uint8_t health_saw_beam;
    uint8_t health_saw_bg_fetch;
    uint8_t health_saw_video;
    uint8_t health_saw_map;
    uint8_t health_saw_apu;
    uint8_t health_saw_oam;
    uint8_t health_saw_linebuf;
    uint8_t health_saw_sprites;
    uint32_t oam_fill_cycles_est; /* last VBlank sprite-field fill (MCU-S1 @ 24 MHz model) */
    uint8_t health_saw_nmi;
    R01sLevel nmi_prev; /* beam NMI# edge detect */
    uint32_t nmi_pulses;
    /* Island M: sprite field (VBlank) + BG0 line ping-pong (HBlank). */
    uint8_t l0_show_half; /* 0/1 within BG0 line region at R01S_L0_LINE_BASE */
    uint8_t linebuf_prev_hblank;
    uint8_t vblank_prev;
    uint8_t field_saw_mcu;
    uint8_t field_saw_beam;
    uint32_t health_phi2_edges;
    /* 0 = soft netlist (live). 1 = manual: power/clock only, data via breadboard later. */
    int wire_mode;
} R01sBoard;

#define R01S_BOARD_WIRE_LIVE 0
#define R01S_BOARD_WIRE_MANUAL 1

void r01s_board_set_wire_mode(R01sBoard *board, int wire_mode);

int r01s_board_build(R01sBoard *board, R01sIslandBuilder *builder);

/* Soft $7Fxx register file . */
uint8_t r01s_board_peek_fe(const R01sBoard *b, uint8_t port);
void r01s_board_poke_fe(R01sBoard *b, uint8_t port, uint8_t v);

/* Host Play: push S2 APU regs (FD-applied) through MCU-M soft $7F4x + SPI mailbox. */
void r01s_board_host_apu_fe4x_push(R01sBoard *b);

/* Load `.retr01` or a raw flash image into the cart. Leaves PRG as stored in the image. */
int r01s_board_load_cart(R01sBoard *board, const char *path);

/* Overlay the short $7Fxx palette + MAP stream into the cart PRG window and point
 * reset at it. The app runs this before IC catchup. Unit tests that inspect PRG do not. */
void r01s_board_arm_ic_boot(R01sBoard *board);

/*
 * Opt-in host soft-load of start-screen MAP+pals (R01S_SOFTBOOT=1). Default LCD
 * path is IC bring-up PRG streaming via $7F93->$7F12: see catchup_bringup.
 */
int r01s_board_softboot_start_screen(R01sBoard *board);

/*
 * Run bring-up palette + MAP->VRAM until the start screen is in VRAM and the LCD
 * hold lifts (~12k board steps). Opt-in softboot via R01S_SOFTBOOT=1. Returns 0
 * on success, -1 on timeout / missing meta.
 */
int r01s_board_catchup_bringup(R01sBoard *board, R01sIslandGroup *group);

/* After MAP stream (pin worker or sync path): 2x2 camera, map_addr, slot flags. */
void r01s_board_catchup_finish(R01sBoard *board);

/* Cart screen directory helpers (world 0). */
int r01s_board_has_screen(const R01sBoard *board, int col, int row);
int r01s_board_first_screen(const R01sBoard *board, int *out_col, int *out_row);

/* BG attr at world pixel from cart MAP (-1 if no screen). */
int r01s_board_attr_at(const R01sBoard *board, int wx, int wy, uint8_t *out_attr);
int r01s_board_solid_at(const R01sBoard *board, int wx, int wy);
/* Player AABB vs present screens + BG solid (Studio play.c SoT). */
int r01s_board_player_aabb_ok(const R01sBoard *board, int px, int py);
int r01s_board_aabb_ok(const R01sBoard *board, int px, int py, int bw, int bh);

int r01s_board_load_camera_2x2(R01sBoard *board, int origin_col, int origin_row);
void r01s_board_set_scroll(R01sBoard *board, uint8_t scroll_x, uint8_t scroll_y);
/* Host Play: MAP/VRAM already loaded via catchup; keep stream gate open. */
void r01s_board_mark_map_ready(R01sBoard *board);

/* Host Play BG0: load from cart meta and update proportional scroll from play cam. */
void r01s_board_load_bg0(R01sBoard *board);
void r01s_board_update_bg0_scroll(R01sBoard *board, int cam_x, int cam_y);

R01sBoard *r01s_board_from_group(R01sIslandGroup *group);

#endif

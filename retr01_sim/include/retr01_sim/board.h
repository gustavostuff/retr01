#ifndef RETR01_SIM_BOARD_H
#define RETR01_SIM_BOARD_H

#include "at28c16.h"
#include "as6c62256.h"
#include "atmega1284p.h"
#include "atmega328p.h"
#include "atf22v10.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "compositor.h"
#include "i2c_eeprom.h"
#include "osc8m.h"
#include "osc_dot.h"
#include "pads.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "retr01_sim/bom32.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/play.h"
#include "retr01_sim/types.h"
#include "sn74hc14.h"
#include "sn74hc157.h"
#include "sn74hc245.h"
#include "sn74hc573.h"
#include "integration.h"
#include "sprite_fetch.h"
#include "sst39sf040.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdint.h>

/* Combinatorial settle passes per wire/eval half-step (PLD/glue depth). */
#define R01S_SETTLE_PASSES 2

/* 9 canvas islands: support chips co-located; pads/sprites/NMI/BG-fetch stay wired-only. */
enum {
    R01S_ISLAND_VIDEO = 0,     /* O: LCD / RGBS (top-left) */
    R01S_ISLAND_POWER_CLK = 1, /* A+B: 5V + OSC/HC14 */
    R01S_ISLAND_CPU = 2,       /* C: CPU RAM PLD + CPU HC245 */
    R01S_ISLAND_IO_LATCH = 3,  /* D */
    R01S_ISLAND_VRAM = 4,      /* G */
    R01S_ISLAND_BEAM = 5,      /* H */
    R01S_ISLAND_CART = 6,      /* J: flash + cart HC245 */
    R01S_ISLAND_APU = 7,       /* K */
    R01S_ISLAND_MCU_LB = 8,    /* L+M: 1284 + linebuf */
    R01S_ISLAND_COUNT = 9,
};

typedef struct R01sIslandPowerClkImpl {
    R01sPwr5v *pwr;
    R01sOsc8m *osc;
    R01sSn74hc14 *hc14;
} R01sIslandPowerClkImpl;

typedef struct R01sIslandCpuMemImpl {
    R01sW65C02S *cpu;
    R01sAs6c62256 *ram;
    R01sPrgRom *prg; /* bench fallback: not mounted when cart owns PRG */
    R01sAtf22v10 *pld_decode;
    R01sSn74hc245 *bus245_cpu;
} R01sIslandCpuMemImpl;

typedef struct R01sIslandIoLatchImpl {
    R01sSn74hc573 *latch573[R01S_BOM_HC573_N];
} R01sIslandIoLatchImpl;

typedef struct R01sIslandPadsImpl {
    R01sPads *pads; /* $FE60 / $FE61 */
} R01sIslandPadsImpl;

typedef struct R01sIslandVramImpl {
    R01sAs6c62256 *vram;
    R01sSn74hc157 *mux157[R01S_BOM_HC157_N];
    R01sBgFetch *bg_pld;
    R01sAtf22v10 *pld_vram;
} R01sIslandVramImpl;

typedef struct R01sIslandBeamImpl {
    R01sOscDot *osc_dot;
    R01sBeamXy *beam_x;
    R01sAtf22v10 *beam_y;
} R01sIslandBeamImpl;

typedef struct R01sIslandBgFetchImpl {
    R01sBgFetch *fetch;
} R01sIslandBgFetchImpl;

typedef struct R01sIslandVideoImpl {
    R01sCompositor *comp;
    R01sAt28c16 *prom;
    R01sVideoSink *sink;
    R01sSn74hc245 *bus245_video;
} R01sIslandVideoImpl;

typedef struct R01sIslandCartImpl {
    R01sSst39sf040 *flash;
    R01sI2cEeprom *save_eeprom;
    R01sSn74hc245 *bus245_cart;
} R01sIslandCartImpl;

typedef struct R01sIslandApuImpl {
    R01sAtmega328p *apu; /* $FE40–$FE5F */
} R01sIslandApuImpl;

typedef struct R01sIslandMcuLbImpl {
    R01sAtmega1284p *mcu; /* OAM $FE20/$FE21; EEPROM mb $FE70–$FE72 */
    R01sAs6c62256 *sram;
    R01sSn74hc157 *mux157[R01S_BOM_HC157_N];
} R01sIslandMcuLbImpl;

typedef struct R01sIslandBusImpl {
    R01sSn74hc245 *bus245[R01S_BOM_HC245_N];
} R01sIslandBusImpl;

typedef struct R01sIslandSpritesImpl {
    R01sSpriteFetch *fetch; /* OAM→linebuf fill stats (Island N) */
} R01sIslandSpritesImpl;

typedef struct R01sIslandIntegrationImpl {
    R01sIntegration *integ; /* NMI / system-ok stats (Island P) */
} R01sIslandIntegrationImpl;

/* Full 32-IC BOM netlist + bench/support parts (PWR/OSC/LCD). */
typedef struct R01sBoard {
    /* Support (not in 32-IC count). */
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sSn74hc14 hc14;
    R01sOscDot osc_dot;
    R01sVideoSink video_sink;
    R01sPrgRom prg; /* bench PRG: not mounted when cart owns $8000+ */
    R01sPads pads;  /* wired via 1284 on silicon; kept for bring-up tests */
    R01sSpriteFetch sprite_fetch;
    R01sIntegration integration;
    /* 32-IC BOM silicon. */
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sAs6c62256 vram;
    R01sAs6c62256 linebuf;
    R01sSst39sf040 cart_flash;
    R01sAtmega328p apu;
    R01sAtmega1284p mcu1284;
    R01sAt28c16 color_prom;
    R01sSn74hc573 latch573[R01S_BOM_HC573_N];
    R01sSn74hc157 mux157[R01S_BOM_HC157_N];
    R01sSn74hc245 bus245[R01S_BOM_HC245_N];
    R01sAtf22v10 pld_decode;
    R01sAtf22v10 pld_vram;
    R01sBeamXy pld_beam_x;
    R01sAtf22v10 pld_beam_y;
    R01sBgFetch bg_fetch;
    R01sCompositor compositor;
    R01sI2cEeprom cart_eeprom;
    R01sIslandPowerClkImpl power_clk_impl;
    R01sIslandCpuMemImpl cpu_mem_impl;
    R01sIslandIoLatchImpl io_latch_impl;
    R01sIslandPadsImpl pads_impl;
    R01sIslandVramImpl vram_impl;
    R01sIslandBeamImpl beam_impl;
    R01sIslandBgFetchImpl bg_fetch_impl;
    R01sIslandVideoImpl video_impl;
    R01sIslandCartImpl cart_impl;
    R01sIslandApuImpl apu_impl;
    R01sIslandMcuLbImpl mcu_lb_impl;
    R01sIslandBusImpl bus_impl;
    R01sIslandSpritesImpl sprites_impl;
    R01sIslandIntegrationImpl integration_impl;
    /* VRAM addr = HC573 FE10|FE11 (cache); $FE12 auto-inc via poke. */
    uint16_t vram_addr;
    int vram_fe12_armed;
    /* MAP seek = HC573 FE90|FE91|FE92 (cache); $FE93 auto-inc via poke. */
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
    uint32_t cart_off_pal_spr;
    uint32_t cart_world_base;      /* world-0 blob base in flash */
    uint32_t cart_off_sdir;        /* absolute screen directory */
    uint8_t cart_screen_count;
    uint8_t cart_start_col;
    uint8_t cart_start_row;
    int cart_loaded;
    char cart_label[48];
    /* Active palette RAM (soft); addr index from HC573 FE08. */
    uint8_t active_pal[32];
    uint8_t pal_addr;
    int pal_fe09_wrote; /* one write+inc per DATA cycle */
    uint8_t chr_last_master; /* hold last BG/sprite master when CHR CE denied */
    /* Host Play scaffold (unused in default UI: game PRG owns play later). */
    R01sPlay play;
    int catchup_cancel; /* cooperative cancel for threaded IC catchup */
    /*
     * sim_fast: word/transaction shortcuts (MAP catchup poke, thin settle/beam).
     * Default 0 = full pin-level netlist. Toggle via UI or R01S_FAST=1.
     */
    int sim_fast;
    int reset_hold;
    uint32_t cycles;
    R01sLevel phi2_prev;
    /* Bring-up milestones (sticky, cleared on reset). */
    uint8_t health_saw_latch;
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
    uint8_t health_saw_nmi;
    R01sLevel nmi_prev; /* beam NMI# edge detect */
    uint32_t nmi_pulses;
    /* Island M ping-pong state (no CPU port; filled by Island N OAM eval). */
    uint8_t linebuf_show_half; /* 0 = $000–$07F showing, 1 = $080–$0FF */
    uint8_t linebuf_prev_hblank;
    uint8_t vblank_prev;
    uint8_t linebuf_saw_mux_mcu;
    uint8_t linebuf_saw_mux_beam;
    uint32_t health_phi2_edges;
} R01sBoard;

int r01s_board_build(R01sBoard *board, R01sIslandBuilder *builder);

/* Load `.retr01` or 512 KB flash image into Island J. Re-applies bring-up smoke PRG
 * into the cart PRG window so A–O island checks still run (overlay: not Studio ROM). */
int r01s_board_load_cart(R01sBoard *board, const char *path);

/*
 * Opt-in host soft-load of start-screen MAP+pals (R01S_SOFTBOOT=1). Default LCD
 * path is IC bring-up PRG streaming via $FE93→$FE12: see catchup_bringup.
 */
int r01s_board_softboot_start_screen(R01sBoard *board);

/*
 * Run bring-up palette + MAP→VRAM until the start screen is in VRAM and the LCD
 * hold lifts. Pin mode (~12k board steps). Fast mode (sim_fast / R01S_FAST): word
 * transaction copy of MAP+pals (same end state, no pin settle). Opt-in softboot via
 * R01S_SOFTBOOT=1. Returns 0 on success, -1 on timeout / missing meta.
 */
int r01s_board_catchup_bringup(R01sBoard *board, R01sIslandGroup *group);

/* Pin-accurate (0) vs word/fast (1). Env R01S_FAST=1 at build selects default. */
void r01s_board_set_sim_fast(R01sBoard *board, int enable);
int r01s_board_sim_fast(const R01sBoard *board);

/* Cart screen directory helpers (world 0). */
int r01s_board_has_screen(const R01sBoard *board, int col, int row);
int r01s_board_first_screen(const R01sBoard *board, int *out_col, int *out_row);
int r01s_board_load_camera_2x2(R01sBoard *board, int origin_col, int origin_row);
void r01s_board_set_scroll(R01sBoard *board, uint8_t scroll_x, uint8_t scroll_y);

R01sBoard *r01s_board_from_group(R01sIslandGroup *group);

#endif

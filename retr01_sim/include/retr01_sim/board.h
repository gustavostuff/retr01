#ifndef RETR01_SIM_BOARD_H
#define RETR01_SIM_BOARD_H

#include "at28c16.h"
#include "as6c62256.h"
#include "atmega1284p.h"
#include "atmega328p.h"
#include "beam_xy.h"
#include "bg_fetch.h"
#include "compositor.h"
#include "osc8m.h"
#include "osc_dot.h"
#include "pads.h"
#include "prg_rom.h"
#include "pwr5v.h"
#include "retr01_sim/island_builder.h"
#include "retr01_sim/types.h"
#include "sn74hc14.h"
#include "sn74hc157.h"
#include "sn74hc573.h"
#include "sn74hc688.h"
#include "integration.h"
#include "sprite_fetch.h"
#include "sst39sf040.h"
#include "video_sink.h"
#include "w65c02s.h"

#include <stdint.h>

/* Combinatorial settle passes per wire/eval half-step (PLD/glue depth). */
#define R01S_SETTLE_PASSES 4

enum {
    R01S_ISLAND_POWER = 0,
    R01S_ISLAND_CLOCK = 1,
    R01S_ISLAND_CPU = 2,
    R01S_ISLAND_IO_LATCH = 3,
    R01S_ISLAND_PADS = 4,
    R01S_ISLAND_VRAM = 5,
    R01S_ISLAND_BEAM = 6,
    R01S_ISLAND_BG_FETCH = 7,
    R01S_ISLAND_VIDEO = 8,
    R01S_ISLAND_CART = 9,
    R01S_ISLAND_APU = 10,
    R01S_ISLAND_MCU1284 = 11,
    R01S_ISLAND_LINEBUF = 12,
    R01S_ISLAND_SPRITES = 13,
    R01S_ISLAND_INTEGRATION = 14,
};

typedef struct R01sIslandPowerImpl {
    R01sPwr5v *pwr;
} R01sIslandPowerImpl;

typedef struct R01sIslandClockImpl {
    R01sOsc8m *osc;
    R01sSn74hc14 *hc14;
} R01sIslandClockImpl;

typedef struct R01sIslandCpuMemImpl {
    R01sW65C02S *cpu;
    R01sAs6c62256 *ram;
    R01sPrgRom *prg; /* breadboard leftover; deselected when cart owns $8000+ */
} R01sIslandCpuMemImpl;

typedef struct R01sIslandIoLatchImpl {
    R01sSn74hc573 *latch;    /* $FE02 scroll X */
    R01sSn74hc573 *scroll_y; /* $FE03 scroll Y */
    R01sSn74hc573 *raster;   /* $FE04 raster compare Y */
} R01sIslandIoLatchImpl;

typedef struct R01sIslandPadsImpl {
    R01sPads *pads; /* $FE60 / $FE61 */
} R01sIslandPadsImpl;

typedef struct R01sIslandVramImpl {
    R01sAs6c62256 *vram;
    R01sSn74hc157 *mux;
} R01sIslandVramImpl;

typedef struct R01sIslandBeamImpl {
    R01sOscDot *osc_dot;
    R01sBeamXy *beam;
    R01sSn74hc688 *cmp; /* Y[7:0] vs $FE04 */
} R01sIslandBeamImpl;

typedef struct R01sIslandBgFetchImpl {
    R01sBgFetch *fetch;
} R01sIslandBgFetchImpl;

typedef struct R01sIslandVideoImpl {
    R01sCompositor *comp;
    R01sAt28c16 *prom;
    R01sVideoSink *sink;
} R01sIslandVideoImpl;

typedef struct R01sIslandCartImpl {
    R01sSst39sf040 *flash;
} R01sIslandCartImpl;

typedef struct R01sIslandApuImpl {
    R01sAtmega328p *apu; /* $FE40–$FE5F */
} R01sIslandApuImpl;

typedef struct R01sIslandMcu1284Impl {
    R01sAtmega1284p *mcu; /* OAM $FE20/$FE21; EEPROM mb $FE70–$FE72 */
} R01sIslandMcu1284Impl;

typedef struct R01sIslandLinebufImpl {
    R01sAs6c62256 *sram; /* ping-pong $000–$07F / $080–$0FF */
    R01sSn74hc157 *mux;  /* MCU fill addr vs beam X */
} R01sIslandLinebufImpl;

typedef struct R01sIslandSpritesImpl {
    R01sSpriteFetch *fetch; /* OAM→linebuf fill stats (Island N) */
} R01sIslandSpritesImpl;

typedef struct R01sIslandIntegrationImpl {
    R01sIntegration *integ; /* NMI / system-ok stats (Island P) */
} R01sIslandIntegrationImpl;

/* Cached pin indices for fast wire_* (Playbook Target 1). Built in r01s_board_build(). */
#define R01S_WIRE_CACHE_A 16
#define R01S_WIRE_CACHE_D 8

typedef struct R01sBoardWireCache {
    int built;
    int cpu_a[R01S_WIRE_CACHE_A];
    int cpu_d[R01S_WIRE_CACHE_D];
    int cpu_rwb;
    int cpu_be;
    int ram_a[15];
    int ram_dq[R01S_WIRE_CACHE_D];
    int prg_a[15];
    int prg_dq[R01S_WIRE_CACHE_D];
    int flash_dq[R01S_WIRE_CACHE_D];
    int vram_dq[R01S_WIRE_CACHE_D];
    int mcu_dq[R01S_WIRE_CACHE_D];
    int apu_dq[R01S_WIRE_CACHE_D];
    int pads_dq[R01S_WIRE_CACHE_D];
    int latch_d[R01S_WIRE_CACHE_D]; /* 573 1D..8D */
    int latch_q[R01S_WIRE_CACHE_D]; /* 573 1Q..8Q */
} R01sBoardWireCache;

/* Bring-up board: islands A–E + G + H + I + O + J + K + L + M + N + P (F deferred). */
typedef struct R01sBoard {
    R01sPwr5v pwr;
    R01sOsc8m osc;
    R01sSn74hc14 hc14;
    R01sW65C02S cpu;
    R01sAs6c62256 ram;
    R01sPrgRom prg;
    R01sSn74hc573 latch;
    R01sSn74hc573 scroll_y_latch;
    R01sSn74hc573 raster_latch;
    R01sPads pads;
    R01sAs6c62256 vram;
    R01sSn74hc157 vram_mux;
    R01sOscDot osc_dot;
    R01sBeamXy beam;
    R01sSn74hc688 raster_cmp;
    R01sBgFetch bg_fetch;
    R01sCompositor compositor;
    R01sAt28c16 color_prom;
    R01sVideoSink video_sink;
    R01sSst39sf040 cart_flash;
    R01sAtmega328p apu;
    R01sAtmega1284p mcu1284;
    R01sAs6c62256 linebuf;
    R01sSn74hc157 linebuf_mux;
    R01sSpriteFetch sprite_fetch;
    R01sIntegration integration;
    R01sIslandPowerImpl power_impl;
    R01sIslandClockImpl clock_impl;
    R01sIslandCpuMemImpl cpu_mem_impl;
    R01sIslandIoLatchImpl io_latch_impl;
    R01sIslandPadsImpl pads_impl;
    R01sIslandVramImpl vram_impl;
    R01sIslandBeamImpl beam_impl;
    R01sIslandBgFetchImpl bg_fetch_impl;
    R01sIslandVideoImpl video_impl;
    R01sIslandCartImpl cart_impl;
    R01sIslandApuImpl apu_impl;
    R01sIslandMcu1284Impl mcu1284_impl;
    R01sIslandLinebufImpl linebuf_impl;
    R01sIslandSpritesImpl sprites_impl;
    R01sIslandIntegrationImpl integration_impl;
    /* Soft $FE10/$FE11 latch + $FE12 auto-inc (pre-full PLD). */
    uint16_t vram_addr;
    int vram_fe12_armed;
    /* Soft MAP $FE90-$FE92 seek + $FE93 data auto-inc. */
    uint32_t map_addr;
    int map_fe93_armed;
    /* Cart image metadata (flash absolute offsets). */
    uint32_t cart_off_prg;
    uint32_t cart_len_prg;
    uint32_t cart_off_chr;         /* world-0 CHR base; 0 = stub tile&0x3F */
    uint32_t cart_off_map_screen0; /* absolute MAP payload for screen (0,0) */
    uint32_t cart_off_pal_bg;
    uint32_t cart_off_pal_spr;
    int cart_loaded;
    char cart_label[48];
    /* Soft $FE08/$FE09 active palette (4 BG + 4 sprite x 4). */
    uint8_t active_pal[32];
    uint8_t pal_addr;
    int pal_fe09_wrote; /* one write+inc per DATA cycle */
    uint8_t chr_last_master; /* hold when CHR CE would fight PRG/MAP */
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
    uint8_t linebuf_saw_mux_mcu;
    uint8_t linebuf_saw_mux_beam;
    uint32_t health_phi2_edges;
    R01sBoardWireCache wire_cache;
} R01sBoard;

int r01s_board_build(R01sBoard *board, R01sIslandBuilder *builder);

/* Load `.retr01` or 512 KB flash image into Island J. Re-applies bring-up smoke PRG
 * into the cart PRG window so A–O island checks still run (overlay — not Studio ROM). */
int r01s_board_load_cart(R01sBoard *board, const char *path);

R01sBoard *r01s_board_from_group(R01sIslandGroup *group);

#endif

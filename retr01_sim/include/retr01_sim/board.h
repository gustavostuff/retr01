#ifndef RETR01_SIM_BOARD_H
#define RETR01_SIM_BOARD_H

#include "at28c16.h"
#include "as6c62256.h"
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

/* Bring-up board: islands A–E + G + H + I + O + J + K (F deferred). */
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
    /* Soft $FE10/$FE11 latch + $FE12 auto-inc (pre-full PLD). */
    uint16_t vram_addr;
    int vram_fe12_armed;
    /* Soft MAP $FE90-$FE92 seek + $FE93 data auto-inc. */
    uint32_t map_addr;
    int map_fe93_armed;
    /* Cart image metadata (flash absolute offsets). */
    uint32_t cart_off_prg;
    uint32_t cart_len_prg;
    int cart_loaded;
    char cart_label[48];
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
    uint32_t health_phi2_edges;
} R01sBoard;

int r01s_board_build(R01sBoard *board, R01sIslandBuilder *builder);

/* Load `.retr01` or 512 KB flash image into Island J. Re-applies bring-up smoke PRG
 * into the cart PRG window so A–O island checks still run (overlay — not Studio ROM). */
int r01s_board_load_cart(R01sBoard *board, const char *path);

R01sBoard *r01s_board_from_group(R01sIslandGroup *group);

#endif

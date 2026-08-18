#ifndef RETR01_EMU_H
#define RETR01_EMU_H

#include "retr01/cart.h"
#include "retr01_emu/cpu.h"
#include "retr01_emu/io_regs.h"
#include "retr01_emu/ppu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RETR01_RAM_SIZE 0x8000
#define RETR01_VRAM_SIZE 0x8000
#define RETR01_IO_SIZE 0x100
#define RETR01_CYCLES_PER_FRAME 133333u

typedef struct retr01_emu {
    retr01_cpu_t cpu;
    retr01_ppu_t ppu;
    retr01_cart_t cart;
    uint8_t ram[RETR01_RAM_SIZE];
    uint8_t vram[RETR01_VRAM_SIZE];
    uint8_t io[RETR01_IO_SIZE];
    uint8_t oam[256];
    uint8_t prg_bank;
    uint16_t vram_addr;
    uint8_t vram_inc;
    uint8_t oam_addr;
    uint32_t map_addr;
    uint8_t vblank;
    uint8_t raster_hit;
    uint8_t vram_written;
    uint8_t beam_y;
    uint64_t frame;
} retr01_emu_t;

void retr01_emu_init(retr01_emu_t *e);
void retr01_emu_free(retr01_emu_t *e);
int retr01_emu_load_cart(retr01_emu_t *e, const char *path);
int retr01_emu_has_prg(const retr01_emu_t *e);
void retr01_emu_reset(retr01_emu_t *e);
void retr01_emu_set_pad(retr01_emu_t *e, int index, uint8_t value);
void retr01_emu_sync_ppu(retr01_emu_t *e);
void retr01_emu_run_frame(retr01_emu_t *e);
int retr01_emu_run_frames(retr01_emu_t *e, int frames);

uint8_t retr01_bus_read(retr01_emu_t *e, uint16_t addr);
void retr01_bus_write(retr01_emu_t *e, uint16_t addr, uint8_t data);

#ifdef __cplusplus
}
#endif

#endif

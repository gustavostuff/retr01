#include "retr01_emu/emu.h"

#include <string.h>

void retr01_emu_init(retr01_emu_t *e)
{
    memset(e, 0, sizeof(*e));
    e->vram_inc = 1;
    retr01_ppu_init(&e->ppu);
    retr01_cart_init(&e->cart);
}

void retr01_emu_free(retr01_emu_t *e)
{
    retr01_cart_free(&e->cart);
}

int retr01_emu_load_cart(retr01_emu_t *e, const char *path)
{
    return retr01_cart_load_file(path, &e->cart);
}

int retr01_emu_has_prg(const retr01_emu_t *e)
{
    return e->cart.prg && e->cart.prg_size >= 0x8000;
}

void retr01_emu_reset(retr01_emu_t *e)
{
    memset(e->ram, 0, sizeof(e->ram));
    memset(e->vram, 0, sizeof(e->vram));
    memset(e->io, 0, sizeof(e->io));
    memset(e->oam, 0, sizeof(e->oam));
    e->prg_bank = 0;
    e->vram_addr = 0;
    e->vram_inc = 1;
    e->oam_addr = 0;
    e->map_addr = 0;
    e->vblank = 0;
    e->raster_hit = 0;
    e->vram_written = 0;
    e->beam_y = 0;
    e->frame = 0;
    e->cpu.cycles = 0;
    e->ppu.chr = e->cart.chr;
    e->ppu.chr_size = e->cart.chr_size;
    retr01_cpu_reset(e);
}

void retr01_emu_set_pad(retr01_emu_t *e, int index, uint8_t value)
{
    if (index < 0 || index > 3) {
        return;
    }
    e->io[RETR01_IO_PAD0 + index] = value;
}

void retr01_emu_sync_ppu(retr01_emu_t *e)
{
    e->ppu.scroll_x = e->io[RETR01_IO_SCROLL_X];
    e->ppu.scroll_y = e->io[RETR01_IO_SCROLL_Y];
    e->ppu.bg_bank = e->io[RETR01_IO_BG_BANK] & 3;
    e->ppu.spr_bank = e->io[RETR01_IO_SPR_BANK] & 3;
    e->ppu.world = e->io[RETR01_IO_WORLD] & 7;
    e->ppu.chr = e->cart.chr;
    e->ppu.chr_size = e->cart.chr_size;
    if (e->vram_written) {
        memcpy(e->ppu.screen.tiles, e->vram, RETR01_SCREEN_TILE_BYTES);
        memcpy(e->ppu.screen.attrs, e->vram + 0x3C0, RETR01_SCREEN_ATTR_BYTES);
    }
}

void retr01_emu_run_frame(retr01_emu_t *e)
{
    uint64_t target = e->cpu.cycles + RETR01_CYCLES_PER_FRAME;
    int guard = 0;

    e->vblank = 0;
    e->beam_y = 0;
    while (e->cpu.cycles < target && guard < 2000000) {
        retr01_cpu_step(e);
        guard++;
        if (e->cpu.stopped) {
            e->cpu.cycles = target;
            break;
        }
    }

    e->vblank = 1;
    e->beam_y = 240;
    e->io[RETR01_IO_BEAM_Y] = 240;
    e->frame++;
    if (e->io[RETR01_IO_PPUCTRL] & RETR01_PPUCTRL_NMI) {
        uint16_t resume = e->cpu.pc;
        int n = 0;
        retr01_cpu_nmi(e);
        while (n++ < 100000 && !e->cpu.stopped && e->cpu.pc != resume) {
            retr01_cpu_step(e);
        }
    }
    retr01_emu_sync_ppu(e);
}

int retr01_emu_run_frames(retr01_emu_t *e, int frames)
{
    int i;
    if (frames < 0) {
        return -1;
    }
    for (i = 0; i < frames; i++) {
        retr01_emu_run_frame(e);
    }
    return 0;
}

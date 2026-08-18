#include "retr01_emu/emu.h"

#include <string.h>

static uint8_t map_data_read(retr01_emu_t *e)
{
    uint8_t v = 0xFF;
    if (e->cart.map && e->cart.map_size > 0) {
        v = e->cart.map[e->map_addr % e->cart.map_size];
    }
    e->map_addr = (e->map_addr + 1) & 0xFFFFFFu;
    e->io[RETR01_IO_MAP_LO] = (uint8_t)e->map_addr;
    e->io[RETR01_IO_MAP_MID] = (uint8_t)(e->map_addr >> 8);
    e->io[RETR01_IO_MAP_HI] = (uint8_t)(e->map_addr >> 16);
    return v;
}

static uint8_t vram_data_read(retr01_emu_t *e)
{
    uint8_t v = e->vram[e->vram_addr & 0x7FFF];
    e->vram_addr = (uint16_t)(e->vram_addr + (e->vram_inc ? e->vram_inc : 1));
    e->io[RETR01_IO_VADDR_LO] = (uint8_t)e->vram_addr;
    e->io[RETR01_IO_VADDR_HI] = (uint8_t)(e->vram_addr >> 8);
    return v;
}

static void vram_data_write(retr01_emu_t *e, uint8_t data)
{
    e->vram[e->vram_addr & 0x7FFF] = data;
    e->vram_written = 1;
    e->vram_addr = (uint16_t)(e->vram_addr + (e->vram_inc ? e->vram_inc : 1));
    e->io[RETR01_IO_VADDR_LO] = (uint8_t)e->vram_addr;
    e->io[RETR01_IO_VADDR_HI] = (uint8_t)(e->vram_addr >> 8);
}

static void oam_dma(retr01_emu_t *e, uint8_t page)
{
    uint16_t src = (uint16_t)page << 8;
    int i;
    for (i = 0; i < 256; i++) {
        e->oam[i] = retr01_bus_read(e, (uint16_t)(src + i));
    }
    e->cpu.cycles += 512;
}

static uint8_t io_read(retr01_emu_t *e, uint8_t off)
{
    switch (off) {
    case RETR01_IO_PPUSTATUS: {
        uint8_t st = 0;
        if (e->vblank) {
            st |= RETR01_PPUSTATUS_VBLANK;
        }
        if (e->raster_hit) {
            st |= RETR01_PPUSTATUS_RASTER;
        }
        e->vblank = 0;
        e->io[off] = st;
        return st;
    }
    case RETR01_IO_BEAM_Y:
        return e->beam_y;
    case RETR01_IO_VDATA:
        return vram_data_read(e);
    case RETR01_IO_OAM_DATA:
        return e->oam[e->oam_addr];
    case RETR01_IO_MAP_DATA:
        return map_data_read(e);
    case RETR01_IO_PRG_BANK:
        return e->prg_bank;
    default:
        return e->io[off];
    }
}

static void io_write(retr01_emu_t *e, uint8_t off, uint8_t data)
{
    if (off >= RETR01_IO_PAD0 && off <= RETR01_IO_PAD3) {
        return;
    }
    e->io[off] = data;
    switch (off) {
    case RETR01_IO_SCROLL_X:
        e->ppu.scroll_x = data;
        break;
    case RETR01_IO_SCROLL_Y:
        e->ppu.scroll_y = data;
        break;
    case RETR01_IO_RASTER_IRQ:
        if (data & 0x80) {
            e->raster_hit = 0;
        }
        break;
    case RETR01_IO_VADDR_LO:
        e->vram_addr = (uint16_t)((e->vram_addr & 0xFF00) | data);
        break;
    case RETR01_IO_VADDR_HI:
        e->vram_addr = (uint16_t)((e->vram_addr & 0x00FF) | ((uint16_t)data << 8));
        break;
    case RETR01_IO_VDATA:
        vram_data_write(e, data);
        break;
    case RETR01_IO_VINC:
        e->vram_inc = data ? data : 1;
        break;
    case RETR01_IO_OAM_ADDR:
        e->oam_addr = data;
        break;
    case RETR01_IO_OAM_DATA:
        e->oam[e->oam_addr++] = data;
        break;
    case RETR01_IO_OAM_DMA:
        oam_dma(e, data);
        break;
    case RETR01_IO_WORLD:
        e->ppu.world = data & 7;
        break;
    case RETR01_IO_BG_BANK:
        e->ppu.bg_bank = data & 3;
        break;
    case RETR01_IO_SPR_BANK:
        e->ppu.spr_bank = data & 3;
        break;
    case RETR01_IO_PRG_BANK:
        e->prg_bank = data;
        break;
    case RETR01_IO_MAP_LO:
        e->map_addr = (e->map_addr & 0xFFFF00u) | data;
        break;
    case RETR01_IO_MAP_MID:
        e->map_addr = (e->map_addr & 0xFF00FFu) | ((uint32_t)data << 8);
        break;
    case RETR01_IO_MAP_HI:
        e->map_addr = (e->map_addr & 0x00FFFFu) | ((uint32_t)(data & 0xFF) << 16);
        break;
    default:
        break;
    }
}

static uint8_t prg_read(retr01_emu_t *e, uint16_t addr)
{
    size_t off;
    if (!e->cart.prg || e->cart.prg_size == 0) {
        return 0xFF;
    }
    off = (size_t)e->prg_bank * 0x8000u + (addr & 0x7FFFu);
    return e->cart.prg[off % e->cart.prg_size];
}

uint8_t retr01_bus_read(retr01_emu_t *e, uint16_t addr)
{
    if (addr < 0x8000) {
        return e->ram[addr];
    }
    if (addr >= 0xFE00 && addr <= 0xFEFF) {
        return io_read(e, (uint8_t)addr);
    }
    return prg_read(e, addr);
}

void retr01_bus_write(retr01_emu_t *e, uint16_t addr, uint8_t data)
{
    if (addr < 0x8000) {
        e->ram[addr] = data;
        return;
    }
    if (addr >= 0xFE00 && addr <= 0xFEFF) {
        io_write(e, (uint8_t)addr, data);
    }
}

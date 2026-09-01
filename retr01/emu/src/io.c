#include "retr01_emu/io.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/video.h"

#include <string.h>

void r01e_io_reset(R01eIo *io) {
    if (!io) {
        return;
    }
    memset(io, 0, sizeof(*io));
    io->ctrl = R01E_PPUCTRL_BG_EN;
    memset(io->oam, 0xFF, sizeof(io->oam));
}

uint8_t r01e_io_read(R01eMachine *m, uint16_t addr) {
    R01eIo *io = &m->io;
    uint8_t v;

    switch (addr) {
    case 0xFE01:
        v = io->status;
        /* VBlank-clear poll => game is idle-waiting; set clears => work / sync path. */
        if (m) {
            m->prof_waiting = (v & R01E_PPUSTATUS_VBLANK) ? 0 : 1;
        }
        io->status = (uint8_t)(io->status & (uint8_t)~(R01E_PPUSTATUS_VBLANK | R01E_PPUSTATUS_HIT));
        return v;
    case 0xFE02:
        return io->scroll_x;
    case 0xFE03:
        return io->scroll_y;
    case 0xFE04:
        return io->raster_y;
    case 0xFE05:
        return io->raster_ctrl;
    case 0xFE09:
        v = io->pal[io->pal_addr & 31u];
        io->pal_addr = (uint8_t)((io->pal_addr + 1) & 31u);
        return v;
    case 0xFE12:
        v = m->video.vram[io->vram_addr & (R01E_VRAM_BYTES - 1)];
        io->vram_addr = (uint16_t)((io->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        return v;
    case 0xFE21:
        v = io->oam[io->oam_addr];
        io->oam_addr++;
        return v;
    case 0xFE30:
        return io->world;
    case 0xFE38:
        return io->pal_row;
    case 0xFE60:
        return io->pad0;
    case 0xFE61:
        return io->pad1;
    case 0xFE93:
        v = r01e_cart_read(&m->cart, io->map_addr);
        io->map_addr = (io->map_addr + 1) & 0xFFFFFFu;
        return v;
    default:
        if (addr >= 0xFE40 && addr <= 0xFE5F) {
            return io->apu[addr - 0xFE40];
        }
        if (addr >= 0xFE31 && addr <= 0xFE37) {
            return io->bank_helper[addr - 0xFE30];
        }
        if (addr >= 0xFE70 && addr <= 0xFE72) {
            return io->eeprom[addr - 0xFE70];
        }
        return 0;
    }
}

void r01e_io_write(R01eMachine *m, uint16_t addr, uint8_t v) {
    R01eIo *io = &m->io;

    switch (addr) {
    case 0xFE00:
        io->ctrl = v;
        break;
    case 0xFE02:
        io->scroll_x = (uint8_t)(v & 127u);
        break;
    case 0xFE03:
        io->scroll_y = (uint8_t)(v < 120u ? v : 119u);
        break;
    case 0xFE04:
        io->raster_y = v;
        break;
    case 0xFE05:
        io->raster_ctrl = v;
        break;
    case 0xFE06:
        io->plane_lo = v;
        break;
    case 0xFE07:
        io->plane_hi = v;
        break;
    case 0xFE08:
        io->pal_addr = (uint8_t)(v & 31u);
        break;
    case 0xFE09:
        io->pal[io->pal_addr & 31u] = (uint8_t)(v & 63u);
        io->pal_addr = (uint8_t)((io->pal_addr + 1) & 31u);
        break;
    case 0xFE10:
        io->vram_addr = (uint16_t)((io->vram_addr & 0xFF00u) | v);
        break;
    case 0xFE11:
        io->vram_addr = (uint16_t)((io->vram_addr & 0x00FFu) | ((uint16_t)v << 8));
        io->vram_addr = (uint16_t)(io->vram_addr & (R01E_VRAM_BYTES - 1));
        break;
    case 0xFE12:
        m->video.vram[io->vram_addr & (R01E_VRAM_BYTES - 1)] = v;
        io->vram_addr = (uint16_t)((io->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        break;
    case 0xFE20:
        io->oam_addr = v;
        break;
    case 0xFE21:
        io->oam[io->oam_addr++] = v;
        break;
    case 0xFE30:
        io->world = (uint8_t)(v & 7u);
        if (r01e_video_softboot_enabled()) {
            (void)r01e_video_boot_world(m, (int)io->world);
        } else {
            (void)r01e_video_prepare_world(m, (int)io->world);
        }
        break;
    case 0xFE38:
        io->pal_row = (uint8_t)(v & 7u);
        if (r01e_video_softboot_enabled()) {
            r01e_video_load_active_pals(m);
        }
        break;
    case 0xFE60:
    case 0xFE61:
        break; /* host-driven pads */
    case 0xFE90:
        io->map_addr = (io->map_addr & 0xFFFF00u) | v;
        break;
    case 0xFE91:
        io->map_addr = (io->map_addr & 0xFF00FFu) | ((uint32_t)v << 8);
        break;
    case 0xFE92:
        io->map_addr = (io->map_addr & 0x00FFFFu) | ((uint32_t)v << 16);
        break;
    case 0xFE93:
        break; /* read-only auto-inc port */
    default:
        if (addr >= 0xFE40 && addr <= 0xFE5F) {
            io->apu[addr - 0xFE40] = v;
        } else if (addr >= 0xFE31 && addr <= 0xFE37) {
            io->bank_helper[addr - 0xFE30] = v;
        } else if (addr >= 0xFE70 && addr <= 0xFE72) {
            io->eeprom[addr - 0xFE70] = v;
        }
        break;
    }
}

void r01e_io_dot(R01eMachine *m) {
    R01eIo *io = &m->io;
    int entered_vblank = 0;

    io->dot_x++;
    if (io->dot_x >= R01E_DOTS_X) {
        io->dot_x = 0;
        io->dot_y++;
        if (io->dot_y >= R01E_DOTS_Y) {
            io->dot_y = 0;
            io->frame++;
        }
        if (io->dot_y == R01E_VISIBLE_H) {
            entered_vblank = 1;
        }
    }

    if (entered_vblank) {
        io->status |= R01E_PPUSTATUS_VBLANK;
        if (io->ctrl & R01E_PPUCTRL_NMI_EN) {
            m->nmi_pending = 1;
        }
    }
    if (io->dot_y == 0 && io->dot_x == 0) {
        io->status = (uint8_t)(io->status & (uint8_t)~R01E_PPUSTATUS_VBLANK);
    }
}

#include "retr01_emu/io.h"

#include "retr01_emu/cart.h"
#include "retr01_emu/cpu.h"
#include "retr01_emu/machine.h"
#include "retr01_emu/play.h"
#include "retr01_emu/video.h"
#include "r01_apu_tracker.h"
#include "r01_hw_regs.h"

#include <string.h>

static void rdy_hold(R01eMachine *m, uint32_t holds) {
    if (m) {
        r01e_cpu_rdy_hold(&m->cpu, holds);
    }
}

static uint16_t cartee_addr(const R01eIo *io) {
    return (uint16_t)(((uint16_t)io->cartee_hi << 8) | io->cartee_lo) & (uint16_t)(R01E_CARTEE_BYTES - 1u);
}

static uint16_t meeprom_addr(const R01eIo *io) {
    /* 9-bit into 512 B (AH bit0 only; high bits ignored). */
    return (uint16_t)(((uint16_t)(io->meeprom_ah & 0x01u) << 8) | io->meeprom_al) &
           (uint16_t)(R01E_MEEPROM_BYTES - 1u);
}

static uint8_t cartee_data_access(R01eMachine *m, uint8_t write_val, int is_write) {
    uint16_t addr;
    uint8_t cmd;

    if (!m) {
        return 0;
    }
    cmd = m->io.cartee_fe22_last;
    addr = cartee_addr(&m->io);
    if (is_write) {
        if (cmd == R01E_CARTEE_CMD_WRITE) {
            m->cart_save[addr] = write_val;
        }
        return write_val;
    }
    if (cmd == R01E_CARTEE_CMD_READ) {
        return m->cart_save[addr];
    }
    return 0;
}

static uint8_t meeprom_data_access(R01eMachine *m, uint8_t write_val, int is_write) {
    uint16_t addr;

    if (!m) {
        return 0;
    }
    addr = meeprom_addr(&m->io);
    if (is_write) {
        m->machine_eeprom[addr] = write_val;
        return write_val;
    }
    return m->machine_eeprom[addr];
}

/* Scroll / palette: VBlank, video off, or first CRT frame (boot/catchup). */
static int scroll_pal_immediate(const R01eMachine *m) {
    const R01eIo *io = &m->io;
    uint8_t layers = (uint8_t)(R01E_PPUCTRL_L1_EN | R01E_PPUCTRL_L0_EN | R01E_PPUCTRL_SPR_EN);

    if ((io->ctrl & layers) == 0) {
        return 1;
    }
    if (io->dot_y >= R01E_VISIBLE_H) {
        return 1;
    }
    if (io->frame == 0) {
        return 1;
    }
    return 0;
}

static void seed_scroll_pal_next(R01eIo *io) {
    io->scroll_x_next = io->scroll_x;
    io->scroll_y_next = io->scroll_y;
    io->bg0_scroll_x_next = io->bg0_scroll_x;
    io->bg0_scroll_y_next = io->bg0_scroll_y;
    io->pal_row_next = io->pal_row;
    io->pal_addr_next = io->pal_addr;
    memcpy(io->pal_next, io->pal, sizeof(io->pal_next));
}

static void flush_scroll_pal(R01eMachine *m) {
    R01eIo *io = &m->io;

    if (!io->scroll_pal_pending) {
        return;
    }
    io->scroll_x = io->scroll_x_next;
    io->scroll_y = io->scroll_y_next;
    io->bg0_scroll_x = io->bg0_scroll_x_next;
    io->bg0_scroll_y = io->bg0_scroll_y_next;
    if (m->video.bg0_scroll_manual) {
        m->video.l0_cam_x = io->bg0_scroll_x;
        m->video.l0_cam_y = io->bg0_scroll_y;
    }
    io->pal_row = io->pal_row_next;
    io->pal_addr = io->pal_addr_next;
    memcpy(io->pal, io->pal_next, sizeof(io->pal));
    io->scroll_pal_pending = 0;
    if (r01e_video_softboot_enabled()) {
        r01e_video_load_active_pals(m);
    }
}

static void latch_pads(R01eIo *io) {
    io->pad0 = io->pad0_host;
    io->pad1 = io->pad1_host;
}

void r01e_io_reset(R01eIo *io) {
    if (!io) {
        return;
    }
    memset(io, 0, sizeof(*io));
    io->ctrl = R01E_PPUCTRL_L1_EN;
    memset(io->oam, 0xFF, sizeof(io->oam));
}

uint8_t r01e_io_read(R01eMachine *m, uint16_t addr) {
    R01eIo *io = &m->io;
    uint8_t v;

    switch (addr) {
    case 0x7F00:
        return io->ctrl;
    case 0x7F01:
        v = io->status;
        /* VBlank-clear poll => game is idle-waiting; set clears => work / sync path. */
        if (m) {
            m->prof_waiting = (v & R01E_PPUSTATUS_VBLANK) ? 0 : 1;
        }
        io->status = (uint8_t)(io->status & (uint8_t)~(R01E_PPUSTATUS_VBLANK | R01E_PPUSTATUS_HIT));
        return v;
    case 0x7F02:
        return io->scroll_x;
    case 0x7F03:
        return io->scroll_y;
    case 0x7F04:
        return io->raster_y;
    case 0x7F05:
        return io->raster_ctrl;
    case 0x7F06:
        return io->bg0_scroll_x;
    case 0x7F07:
        return io->bg0_scroll_y;
    case 0x7F08:
        return (uint8_t)(io->pal_row & 7u);
    case 0x7F09:
        v = io->pal[io->pal_addr & 31u];
        io->pal_addr = (uint8_t)((io->pal_addr + 1) & 31u);
        return v;
    case 0x7F12:
        v = m->video.vram[io->vram_addr & (R01E_VRAM_BYTES - 1)];
        io->vram_addr = (uint16_t)((io->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        return v;
    case 0x7F21:
        v = io->oam[io->oam_addr];
        io->oam_addr = (uint16_t)((io->oam_addr + 1u) % (R01E_OAM_ENTRIES * R01E_OAM_ENTRY_BYTES));
        return v;
    case 0x7F22:
        return io->cartee_fe22_last;
    case 0x7F23:
        return io->cartee_lo;
    case 0x7F24:
        rdy_hold(m, R01_RDY_CARTEE_READ_HOLDS);
        return cartee_data_access(m, 0, 0);
    case 0x7F30:
        return io->world;
    case 0x7F60:
        return io->pad0;
    case 0x7F61:
        return io->pad1;
    case 0x7F70:
        return io->meeprom_al;
    case 0x7F71:
        return io->meeprom_ah;
    case 0x7F72:
        rdy_hold(m, R01_RDY_MEEPROM_READ_HOLDS);
        return meeprom_data_access(m, 0, 0);
    case 0x7F93:
        v = r01e_cart_read(&m->cart, io->map_addr);
        io->map_addr = (io->map_addr + 1) & 0xFFFFFFu;
        return v;
    default:
        if (addr >= 0x7F40 && addr <= 0x7F5F) {
            return io->apu[addr - 0x7F40];
        }
        if (addr >= 0x7F31 && addr <= 0x7F37) {
            return io->bank_helper[addr - 0x7F30];
        }
        return 0;
    }
}

void r01e_io_write(R01eMachine *m, uint16_t addr, uint8_t v) {
    R01eIo *io = &m->io;
    int apply_now;

    switch (addr) {
    case 0x7F00:
        io->ctrl = v;
        break;
    case 0x7F02:
        v = (uint8_t)(v & 127u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        io->scroll_x_next = v;
        if (apply_now) {
            io->scroll_x = v;
        } else {
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F03:
        v = (uint8_t)(v < 120u ? v : 119u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        io->scroll_y_next = v;
        if (apply_now) {
            io->scroll_y = v;
        } else {
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F04:
        io->raster_y = v;
        break;
    case 0x7F05:
        io->raster_ctrl = v;
        break;
    case 0x7F06:
        v = (uint8_t)(v & 127u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        io->bg0_scroll_x_next = v;
        m->video.bg0_scroll_manual = 1;
        if (apply_now) {
            io->bg0_scroll_x = v;
            m->video.l0_cam_x = v;
        } else {
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F07:
        v = (uint8_t)(v < 120u ? v : 119u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        io->bg0_scroll_y_next = v;
        m->video.bg0_scroll_manual = 1;
        if (apply_now) {
            io->bg0_scroll_y = v;
            m->video.l0_cam_y = v;
        } else {
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F08:
        v = (uint8_t)(v & 7u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        io->pal_row_next = v;
        io->pal_addr_next = 0;
        if (apply_now) {
            io->pal_row = v;
            io->pal_addr = 0;
            if (r01e_video_softboot_enabled()) {
                r01e_video_load_active_pals(m);
            }
        } else {
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F09:
        v = (uint8_t)(v & 63u);
        apply_now = scroll_pal_immediate(m);
        if (!apply_now && !io->scroll_pal_pending) {
            seed_scroll_pal_next(io);
        }
        if (apply_now) {
            uint8_t a = (uint8_t)(io->pal_addr & 31u);
            io->pal[a] = v;
            io->pal_next[a] = v;
            io->pal_addr = (uint8_t)((a + 1u) & 31u);
            io->pal_addr_next = io->pal_addr;
        } else {
            io->pal_next[io->pal_addr_next & 31u] = v;
            io->pal_addr_next = (uint8_t)((io->pal_addr_next + 1) & 31u);
            io->scroll_pal_pending = 1;
        }
        break;
    case 0x7F10:
        io->vram_addr = (uint16_t)((io->vram_addr & 0xFF00u) | v);
        break;
    case 0x7F11:
        io->vram_addr = (uint16_t)((io->vram_addr & 0x00FFu) | ((uint16_t)v << 8));
        io->vram_addr = (uint16_t)(io->vram_addr & (R01E_VRAM_BYTES - 1));
        break;
    case 0x7F12:
        /* Play on: drop leftover boot/ASM MAP streams into the host 2x2. */
        if (!m->play.enabled) {
            m->video.vram[io->vram_addr & (R01E_VRAM_BYTES - 1)] = v;
        }
        io->vram_addr = (uint16_t)((io->vram_addr + 1) & (R01E_VRAM_BYTES - 1));
        break;
    case 0x7F20:
        /* Load bits [7:0]. Sequential $7F21 fill wraps at 64*4 = 256 B. */
        io->oam_addr = v;
        break;
    case 0x7F21:
        io->oam[io->oam_addr] = v;
        io->oam_addr = (uint16_t)((io->oam_addr + 1u) % (R01E_OAM_ENTRIES * R01E_OAM_ENTRY_BYTES));
        break;
    case 0x7F22:
        io->cartee_fe22_last = v;
        if (v != R01E_CARTEE_CMD_READ && v != R01E_CARTEE_CMD_WRITE) {
            io->cartee_hi = v;
        }
        break;
    case 0x7F23:
        io->cartee_lo = v;
        break;
    case 0x7F24:
        (void)cartee_data_access(m, v, 1);
        rdy_hold(m, R01_RDY_CARTEE_WRITE_HOLDS);
        break;
    case 0x7F30:
        io->world = (uint8_t)(v & 7u);
        if (r01e_video_softboot_enabled()) {
            (void)r01e_video_boot_world(m, (int)io->world);
        } else {
            (void)r01e_video_prepare_world(m, (int)io->world);
        }
        break;
    case 0x7F60:
    case 0x7F61:
        break; /* host-driven pads via r01e_machine_set_pad */
    case 0x7F70:
        io->meeprom_al = v;
        break;
    case 0x7F71:
        io->meeprom_ah = v;
        break;
    case 0x7F72:
        (void)meeprom_data_access(m, v, 1);
        rdy_hold(m, R01_RDY_MEEPROM_WRITE_HOLDS);
        break;
    case 0x7F90:
        io->map_addr = (io->map_addr & 0xFFFF00u) | v;
        break;
    case 0x7F91:
        io->map_addr = (io->map_addr & 0xFF00FFu) | ((uint32_t)v << 8);
        break;
    case 0x7F92:
        io->map_addr = (io->map_addr & 0x00FFFFu) | ((uint32_t)v << 16);
        break;
    case 0x7F93:
        break; /* read-only auto-inc port */
    default:
        if (addr >= 0x7F40 && addr <= 0x7F5F) {
            io->apu[addr - 0x7F40] = v;
        } else if (addr >= 0x7F31 && addr <= 0x7F37) {
            io->bank_helper[addr - 0x7F30] = v;
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
        /* Early VBlank: pending scroll/pal, pad latch, Host Play OAM/scroll. */
        flush_scroll_pal(m);
        latch_pads(io);
        r01e_play_tick(m);

        io->status |= R01E_PPUSTATUS_VBLANK;
        if (io->ctrl & R01E_PPUCTRL_NMI_EN) {
            m->nmi_pending = 1;
        }
        /* Host Play cart APU: one tracker tick per VBlank NMI. */
        if (m->apu_tracker_on) {
            (void)r01_apu_tracker_nmi(&m->apu_tracker, m->io.apu);
        }
    }
    if (io->dot_y == 0 && io->dot_x == 0) {
        io->status = (uint8_t)(io->status & (uint8_t)~R01E_PPUSTATUS_VBLANK);
    }
}

#include "avr128db28_m.h"

#include "r01_hw_regs.h"
#include "r01_soft_sel_demux.h"
#include "retr01_sim/bus.h"

#include <string.h>

static void soft_sync_map(R01sAvr128db28M *c) {
    c->soft_map_addr =
        ((uint32_t)c->soft_map_hi << 16) | ((uint32_t)c->soft_map_mid << 8) | c->soft_map_lo;
    c->soft_cart_a14_18 = (uint8_t)((c->soft_map_addr >> 14) & 0x1Fu);
}

static uint16_t meeprom_addr(const R01sAvr128db28M *c) {
    return (uint16_t)(((uint16_t)(c->meeprom_ah & 0x01u) << 8) | c->meeprom_al) &
           (uint16_t)(R01S_MCU_M_EEPROM_BYTES - 1u);
}

static uint16_t cartee_addr(const R01sAvr128db28M *c) {
    return (uint16_t)(((uint16_t)c->cartee_ah << 8) | c->cartee_al) &
           (uint16_t)(R01S_MCU_M_CARTEE_BYTES - 1u);
}

static void rdy_hold_n(R01sAvr128db28M *c, uint32_t holds) {
    if (c) {
        c->rdy_hold = holds;
    }
}

static void rdy_drive(R01sEntity *e, R01sAvr128db28M *c) {
    /* Open-drain: pull low to stall. Idle is Hi-Z so the board pull-up holds RDY high. */
    if (c->rdy_hold > 0) {
        r01s_entity_drive(e, "CPU_RDY", R01S_LVL_L);
    } else {
        r01s_entity_drive(e, "CPU_RDY", R01S_LVL_Z);
    }
}

static void ss_idle(R01sEntity *e) {
    r01s_entity_drive(e, "/SS_S1", R01S_LVL_H);
    r01s_entity_drive(e, "/SS_S2", R01S_LVL_H);
}

static void m_reset(R01sEntity *e) {
    R01sAvr128db28M *c = (R01sAvr128db28M *)e;
    memset(c->eeprom_mb, 0, sizeof(c->eeprom_mb));
    memset(c->eeprom, 0, sizeof(c->eeprom));
    memset(c->cartee, 0, sizeof(c->cartee));
    c->soft_ppuctrl = 0;
    c->soft_raster_ctrl = 0;
    c->soft_bg0_x = 0;
    c->soft_bg0_y = 0;
    c->soft_pal_addr = 0;
    c->soft_map_lo = 0;
    c->soft_map_mid = 0;
    c->soft_map_hi = 0;
    c->soft_map_addr = 0;
    c->soft_cart_a14_18 = 0;
    c->soft_last_strobe = 0xFF;
    c->soft_oam_addr = 0;
    memset(c->soft_oam, 0xFF, sizeof(c->soft_oam));
    memset(c->soft_apu, 0, sizeof(c->soft_apu));
    c->soft_oam_dirty = 0;
    c->soft_apu_dirty = 0;
    c->soft_vbl_mark_pending = 0;
    c->cartee_al = 0;
    c->cartee_ah = 0;
    c->cartee_cmd = 0;
    c->meeprom_al = 0;
    c->meeprom_ah = 0;
    c->cpu_a_lo = 0;
    c->rdy_hold = 0;
    c->sel_soft_prev = 0;
    c->a_sample_prev = 0;
    c->clk_ticks = 0;
    c->alive = 0;
    r01s_bus_hiz(e, "CPU_D", 8);
    ss_idle(e);
    rdy_drive(e, c);
}

static void m_eval(R01sEntity *e) {
    R01sAvr128db28M *c = (R01sAvr128db28M *)e;
    r01s_bus_hiz(e, "CPU_D", 8);
    ss_idle(e);
    rdy_drive(e, c);
}

static void m_tick(R01sEntity *e) {
    R01sAvr128db28M *c = (R01sAvr128db28M *)e;
    if (r01s_level_is_high(r01s_entity_sense(e, "CLK"))) {
        c->clk_ticks++;
        c->alive = 1;
        if (c->rdy_hold > 0) {
            c->rdy_hold--;
        }
    }
    r01s_entity_drive(e, "RUN", c->alive ? R01S_LVL_H : R01S_LVL_L);
    ss_idle(e);
    rdy_drive(e, c);
}

static void m_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable MCU_M_VT = {m_reset, m_eval, m_tick, m_destroy};

void r01s_avr128db28_m_init(R01sAvr128db28M *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &MCU_M_VT, "AVR128DB28", refdes ? refdes : "UM");
    chip->base.impl = chip;

    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "CPU_D0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 3, "CPU_D1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 4, "CPU_D2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 5, "CPU_D3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 6, "CPU_D4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 7, "CPU_D5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "CPU_D6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 9, "CPU_D7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 10, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 11, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 12, "SPI_MOSI", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 13, "SPI_MISO", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 14, "SPI_SCK", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 15, "/SS_S1", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 16, "/SS_S2", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 17, "SEL_SOFT0", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 18, "SEL_SOFT1", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 19, "SEL_SOFT2", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 20, "CPU_RDY", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 21, "VBL", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 22, "CPU_A_SAMPLE", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 23, "S1_RDY", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 24, "SDA", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 25, "SCL", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 26, "UPDI", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 27, "CLK", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 28, "RUN", R01S_PIN_OUT);
    r01s_entity_set_dip_mm(&chip->base, 28, 35, 8);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_avr128db28_m_entity(R01sAvr128db28M *chip) {
    return chip ? &chip->base : NULL;
}

uint8_t r01s_avr128db28_m_eeprom_peek(const R01sAvr128db28M *chip, unsigned i) {
    if (!chip || i >= R01S_MCU_M_EEPROM_MAILBOX) {
        return 0;
    }
    return chip->eeprom_mb[i];
}

void r01s_avr128db28_m_eeprom_poke(R01sAvr128db28M *chip, unsigned i, uint8_t data) {
    if (!chip || i >= R01S_MCU_M_EEPROM_MAILBOX) {
        return;
    }
    chip->eeprom_mb[i] = data;
}

int r01s_avr128db28_m_soft_write(R01sAvr128db28M *chip, uint8_t port, uint8_t data) {
    if (!chip) {
        return 0;
    }
    if (port == 0x02u || port == 0x03u || port == 0x04u) {
        return 0;
    }
    switch (port) {
    case 0x00:
        chip->soft_ppuctrl = data;
        break;
    case 0x05:
        chip->soft_raster_ctrl = data;
        break;
    case 0x06:
        chip->soft_bg0_x = (uint8_t)(data & 0x7Fu);
        break;
    case 0x07:
        chip->soft_bg0_y = (uint8_t)(data < 120u ? data : 119u);
        break;
    case 0x08:
        chip->soft_pal_addr = data;
        break;
    case 0x20:
        chip->soft_oam_addr = data;
        break;
    case 0x21:
        chip->soft_oam[chip->soft_oam_addr] = data;
        chip->soft_oam_addr = (uint8_t)(chip->soft_oam_addr + 1u);
        chip->soft_oam_dirty = 1;
        break;
    case 0x22:
        chip->cartee_cmd = data;
        if (data != R01_CARTEE_CMD_READ && data != R01_CARTEE_CMD_WRITE) {
            chip->cartee_ah = data;
        }
        break;
    case 0x23:
        chip->cartee_al = data;
        break;
    case 0x24:
        if (chip->cartee_cmd == R01_CARTEE_CMD_WRITE) {
            chip->cartee[cartee_addr(chip)] = data;
        }
        rdy_hold_n(chip, R01_RDY_CARTEE_WRITE_HOLDS);
        rdy_drive(&chip->base, chip);
        break;
    case 0x70:
        chip->meeprom_al = data;
        chip->eeprom_mb[0] = data;
        break;
    case 0x71:
        chip->meeprom_ah = (uint8_t)(data & 0x01u);
        chip->eeprom_mb[1] = chip->meeprom_ah;
        break;
    case 0x72:
        chip->eeprom[meeprom_addr(chip)] = data;
        chip->eeprom_mb[2] = data;
        rdy_hold_n(chip, R01_RDY_MEEPROM_WRITE_HOLDS);
        rdy_drive(&chip->base, chip);
        break;
    case 0x90:
        chip->soft_map_lo = data;
        soft_sync_map(chip);
        break;
    case 0x91:
        chip->soft_map_mid = data;
        soft_sync_map(chip);
        break;
    case 0x92:
        chip->soft_map_hi = data;
        soft_sync_map(chip);
        break;
    default:
        if (port >= 0x40u && port <= 0x5Fu) {
            chip->soft_apu[port - 0x40u] = data;
            chip->soft_apu_dirty = 1;
            break;
        }
        return 0;
    }
    chip->soft_last_strobe = port;
    return 1;
}

uint8_t r01s_avr128db28_m_soft_read(const R01sAvr128db28M *chip, uint8_t port) {
    if (!chip) {
        return 0;
    }
    switch (port) {
    case 0x00:
        return chip->soft_ppuctrl;
    case 0x05:
        return chip->soft_raster_ctrl;
    case 0x06:
        return chip->soft_bg0_x;
    case 0x07:
        return chip->soft_bg0_y;
    case 0x08:
        return chip->soft_pal_addr;
    case 0x20:
        return chip->soft_oam_addr;
    case 0x21:
        return chip->soft_oam[chip->soft_oam_addr];
    case 0x22:
        return chip->cartee_cmd;
    case 0x23:
        return chip->cartee_al;
    case 0x24:
        if (chip->cartee_cmd == R01_CARTEE_CMD_READ) {
            return chip->cartee[cartee_addr(chip)];
        }
        return 0;
    case 0x70:
        return chip->meeprom_al;
    case 0x71:
        return chip->meeprom_ah;
    case 0x72:
        return chip->eeprom[meeprom_addr(chip)];
    case 0x90:
        return chip->soft_map_lo;
    case 0x91:
        return chip->soft_map_mid;
    case 0x92:
        return chip->soft_map_hi;
    default:
        if (port >= 0x40u && port <= 0x5Fu) {
            return chip->soft_apu[port - 0x40u];
        }
        return 0;
    }
}

void r01s_avr128db28_m_soft_rdy_on_data(R01sAvr128db28M *chip, uint8_t port) {
    if (!chip) {
        return;
    }
    if (port == 0x24u) {
        rdy_hold_n(chip, R01_RDY_CARTEE_READ_HOLDS);
        rdy_drive(&chip->base, chip);
    } else if (port == 0x72u) {
        rdy_hold_n(chip, R01_RDY_MEEPROM_READ_HOLDS);
        rdy_drive(&chip->base, chip);
    }
}

uint32_t r01s_avr128db28_m_soft_map_addr(const R01sAvr128db28M *chip) {
    return chip ? chip->soft_map_addr : 0;
}

uint32_t r01s_avr128db28_m_clk_ticks(const R01sAvr128db28M *chip) {
    return chip ? chip->clk_ticks : 0;
}

int r01s_avr128db28_m_alive(const R01sAvr128db28M *chip) {
    return chip && chip->alive;
}

int r01s_avr128db28_m_soft_oam_dirty(const R01sAvr128db28M *chip) {
    return chip && chip->soft_oam_dirty;
}

int r01s_avr128db28_m_soft_apu_dirty(const R01sAvr128db28M *chip) {
    return chip && chip->soft_apu_dirty;
}

void r01s_avr128db28_m_soft_clear_oam_dirty(R01sAvr128db28M *chip) {
    if (chip) {
        chip->soft_oam_dirty = 0;
    }
}

void r01s_avr128db28_m_soft_clear_apu_dirty(R01sAvr128db28M *chip) {
    if (chip) {
        chip->soft_apu_dirty = 0;
    }
}

void r01s_avr128db28_m_soft_request_vbl_mark(R01sAvr128db28M *chip) {
    if (chip) {
        chip->soft_vbl_mark_pending = 1;
    }
}

const uint8_t *r01s_avr128db28_m_soft_oam(const R01sAvr128db28M *chip) {
    return chip ? chip->soft_oam : NULL;
}

const uint8_t *r01s_avr128db28_m_soft_apu(const R01sAvr128db28M *chip) {
    return chip ? chip->soft_apu : NULL;
}

int r01s_avr128db28_m_rdy_is_held(const R01sAvr128db28M *chip) {
    return chip && chip->rdy_hold > 0;
}

uint32_t r01s_avr128db28_m_rdy_hold(const R01sAvr128db28M *chip) {
    return chip ? chip->rdy_hold : 0;
}

void r01s_avr128db28_m_cpu_a_latch(R01sAvr128db28M *chip, uint8_t a_lo) {
    if (chip) {
        chip->cpu_a_lo = a_lo;
    }
}

uint8_t r01s_avr128db28_m_cpu_a_lo(const R01sAvr128db28M *chip) {
    return chip ? chip->cpu_a_lo : 0;
}

int r01s_avr128db28_m_soft_sel_accept(R01sAvr128db28M *chip, uint8_t sel_bit, uint8_t a_lo, uint8_t data) {
    uint8_t port;
    if (!chip) {
        return 0;
    }
    chip->cpu_a_lo = a_lo;
    port = r01_soft_sel_demux(sel_bit, a_lo);
    if (port == 0xFFu) {
        return 0;
    }
    return r01s_avr128db28_m_soft_write(chip, port, data);
}

static uint8_t sel_soft_mask(R01sEntity *e) {
    uint8_t m = 0;
    if (r01s_level_is_high(r01s_entity_sense(e, "SEL_SOFT0"))) {
        m |= 0x01u;
    }
    if (r01s_level_is_high(r01s_entity_sense(e, "SEL_SOFT1"))) {
        m |= 0x02u;
    }
    if (r01s_level_is_high(r01s_entity_sense(e, "SEL_SOFT2"))) {
        m |= 0x04u;
    }
    return m;
}

static uint8_t sample_cpu_d(R01sEntity *e) {
    return (uint8_t)r01s_bus_read(e, "CPU_D", 8);
}

static void soft_sel_poll_ex(R01sAvr128db28M *chip, int do_write) {
    R01sEntity *e;
    uint8_t cur;
    uint8_t rise;
    uint8_t a_sample;
    uint8_t data;
    uint8_t port;
    if (!chip) {
        return;
    }
    e = &chip->base;
    a_sample = r01s_level_is_high(r01s_entity_sense(e, "CPU_A_SAMPLE")) ? 1u : 0u;
    if (a_sample && !chip->a_sample_prev) {
        chip->cpu_a_lo = sample_cpu_d(e);
    }
    chip->a_sample_prev = a_sample;

    cur = sel_soft_mask(e);
    rise = (uint8_t)(cur & (uint8_t)~chip->sel_soft_prev);
    chip->sel_soft_prev = cur;
    data = sample_cpu_d(e);
    if (do_write) {
        if (rise & 0x01u) {
            port = r01_soft_sel_demux(0u, chip->cpu_a_lo);
            if (port != 0xFFu) {
                (void)r01s_avr128db28_m_soft_write(chip, port, data);
            }
        }
        if (rise & 0x02u) {
            port = r01_soft_sel_demux(1u, chip->cpu_a_lo);
            if (port != 0xFFu) {
                (void)r01s_avr128db28_m_soft_write(chip, port, data);
            }
        }
        if (rise & 0x04u) {
            port = r01_soft_sel_demux(2u, chip->cpu_a_lo);
            if (port != 0xFFu) {
                (void)r01s_avr128db28_m_soft_write(chip, port, data);
            }
        }
    }
    rdy_drive(e, chip);
}

void r01s_avr128db28_m_soft_sel_poll(R01sAvr128db28M *chip) {
    soft_sel_poll_ex(chip, 1);
}

void r01s_avr128db28_m_soft_sel_sync_pins(R01sAvr128db28M *chip) {
    soft_sel_poll_ex(chip, 0);
}

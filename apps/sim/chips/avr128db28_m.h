#ifndef retr01_SIM_AVR128DB28_M_H
#define retr01_SIM_AVR128DB28_M_H

#include "retr01_sim/entity.h"

#include "r01_hw_regs.h"
#include "r01_spi_mailbox.h"

#include <stdint.h>

#define R01S_MCU_M_EEPROM_MAILBOX 3u
#define R01S_MCU_M_EEPROM_BYTES R01_MEEPROM_BYTES
#define R01S_MCU_M_CARTEE_BYTES R01_CARTEE_BYTES

/*
 * MCU-M (AVR128DB28) -- soft $7Fxx, MAP seek, EE mailboxes + RDY, SPI master stub.
 * Soft OAM/APU latch here; SPI mailbox flush applies to S1/S2. Behavioral shell (not AVR ISA).
 */
typedef struct R01sAvr128db28M {
    R01sEntity base;
    uint8_t eeprom_mb[R01S_MCU_M_EEPROM_MAILBOX]; /* legacy 3-byte peek/poke */
    uint8_t eeprom[R01S_MCU_M_EEPROM_BYTES];
    uint8_t cartee[R01S_MCU_M_CARTEE_BYTES];
    uint8_t soft_ppuctrl;
    uint8_t soft_raster_ctrl;
    uint8_t soft_bg0_x;
    uint8_t soft_bg0_y;
    uint8_t soft_pal_addr;
    uint8_t soft_map_lo;
    uint8_t soft_map_mid;
    uint8_t soft_map_hi;
    uint32_t soft_map_addr;
    uint8_t soft_cart_a14_18;
    uint8_t soft_last_strobe;
    uint8_t soft_oam_addr; /* $7F20 */
    uint8_t soft_oam[R01_OAM_BYTES];
    uint8_t soft_apu[R01_APU_REGS]; /* $7F40-$7F5F shadow */
    uint8_t soft_oam_dirty;
    uint8_t soft_apu_dirty;
    uint8_t soft_vbl_mark_pending;
    uint8_t cartee_al;
    uint8_t cartee_ah;
    uint8_t cartee_cmd;
    uint8_t meeprom_al;
    uint8_t meeprom_ah;
    uint8_t cpu_a_lo; /* latched A[7:0] for Soft SEL demux */
    uint32_t rdy_hold; /* CPU_RDY low while > 0 */
    uint8_t sel_soft_prev;
    uint8_t a_sample_prev;
    uint32_t clk_ticks;
    uint8_t alive;
} R01sAvr128db28M;

void r01s_avr128db28_m_init(R01sAvr128db28M *chip, const char *refdes);
R01sEntity *r01s_avr128db28_m_entity(R01sAvr128db28M *chip);

uint8_t r01s_avr128db28_m_eeprom_peek(const R01sAvr128db28M *chip, unsigned i);
void r01s_avr128db28_m_eeprom_poke(R01sAvr128db28M *chip, unsigned i, uint8_t data);

int r01s_avr128db28_m_soft_write(R01sAvr128db28M *chip, uint8_t port, uint8_t data);
uint8_t r01s_avr128db28_m_soft_read(const R01sAvr128db28M *chip, uint8_t port);
/* Assert RDY hold after bus touch of $7F24 / $7F72 (read or write). */
void r01s_avr128db28_m_soft_rdy_on_data(R01sAvr128db28M *chip, uint8_t port);
uint32_t r01s_avr128db28_m_soft_map_addr(const R01sAvr128db28M *chip);

uint32_t r01s_avr128db28_m_clk_ticks(const R01sAvr128db28M *chip);
int r01s_avr128db28_m_alive(const R01sAvr128db28M *chip);

int r01s_avr128db28_m_soft_oam_dirty(const R01sAvr128db28M *chip);
int r01s_avr128db28_m_soft_apu_dirty(const R01sAvr128db28M *chip);
void r01s_avr128db28_m_soft_clear_oam_dirty(R01sAvr128db28M *chip);
void r01s_avr128db28_m_soft_clear_apu_dirty(R01sAvr128db28M *chip);
void r01s_avr128db28_m_soft_request_vbl_mark(R01sAvr128db28M *chip);
const uint8_t *r01s_avr128db28_m_soft_oam(const R01sAvr128db28M *chip);
const uint8_t *r01s_avr128db28_m_soft_apu(const R01sAvr128db28M *chip);

int r01s_avr128db28_m_rdy_is_held(const R01sAvr128db28M *chip);
uint32_t r01s_avr128db28_m_rdy_hold(const R01sAvr128db28M *chip);
void r01s_avr128db28_m_cpu_a_latch(R01sAvr128db28M *chip, uint8_t a_lo);
uint8_t r01s_avr128db28_m_cpu_a_lo(const R01sAvr128db28M *chip);
/* Direct demux accept (tests / Host Play). sel_bit 0/1/2. */
int r01s_avr128db28_m_soft_sel_accept(R01sAvr128db28M *chip, uint8_t sel_bit, uint8_t a_lo, uint8_t data);
/* Rising CPU_A_SAMPLE / SEL_SOFT* from pins -> soft write. */
void r01s_avr128db28_m_soft_sel_poll(R01sAvr128db28M *chip);
/* Same edge tracking as poll, but do not soft_write (board already poke_fe'd). */
void r01s_avr128db28_m_soft_sel_sync_pins(R01sAvr128db28M *chip);

#endif

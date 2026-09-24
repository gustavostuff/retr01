#include "avr128db28_m.h"
#include "r01_hw_regs.h"
#include "test_common.h"

#include "retr01_sim/bus.h"

int main(void) {
    R01sAvr128db28M chip;
    R01sEntity *e;
    int i;

    r01s_avr128db28_m_init(&chip, "UM");
    e = r01s_avr128db28_m_entity(&chip);
    expect_true(e != NULL, "entity");
    expect_true(e->part && e->part[0] == 'A', "part AVR128DB28");

    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x08u, 0x15), "soft FE08 write");
    expect_true(r01s_avr128db28_m_soft_read(&chip, 0x08u) == 0x15, "soft FE08 read");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x90u, 0x34), "soft FE90");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x91u, 0x12), "soft FE91");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x92u, 0x00), "soft FE92");
    expect_true(r01s_avr128db28_m_soft_map_addr(&chip) == 0x001234u, "soft map_addr");

    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x20u, 0x00), "soft FE20");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x21u, 0x10), "soft FE21 Y");
    expect_true(r01s_avr128db28_m_soft_oam_dirty(&chip), "oam dirty");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x40u, 0x8F), "soft FE40");
    expect_true(r01s_avr128db28_m_soft_apu_dirty(&chip), "apu dirty");
    expect_true(r01s_entity_pin_named(e, "CPU_D0") != NULL, "SoT CPU_D0");
    expect_true(r01s_entity_pin_named(e, "/SS_S1") != NULL, "SoT /SS_S1");
    expect_true(r01s_entity_pin_named(e, "CPU_A_SAMPLE") != NULL, "SoT CPU_A_SAMPLE");
    expect_true(r01s_entity_pin_named(e, "SEL_SOFT2") != NULL, "SoT SEL_SOFT2");

    /* Soft SEL A-sample then SEL_SOFT2 rise -> $7F40. */
    {
        uint8_t d;
        for (d = 0; d < 8u; d++) {
            char n[8];
            snprintf(n, sizeof(n), "CPU_D%d", d);
            r01s_entity_drive(e, n, (0x40u & (1u << d)) ? R01S_LVL_H : R01S_LVL_L);
        }
        r01s_entity_drive(e, "CPU_A_SAMPLE", R01S_LVL_H);
        r01s_avr128db28_m_soft_sel_poll(&chip);
        r01s_entity_drive(e, "CPU_A_SAMPLE", R01S_LVL_L);
        r01s_avr128db28_m_soft_sel_poll(&chip);
        expect_true(r01s_avr128db28_m_cpu_a_lo(&chip) == 0x40u, "A latch 0x40");
        for (d = 0; d < 8u; d++) {
            char n[8];
            snprintf(n, sizeof(n), "CPU_D%d", d);
            r01s_entity_drive(e, n, (0xA5u & (1u << d)) ? R01S_LVL_H : R01S_LVL_L);
        }
        r01s_entity_drive(e, "SEL_SOFT2", R01S_LVL_L);
        r01s_avr128db28_m_soft_sel_poll(&chip);
        r01s_entity_drive(e, "SEL_SOFT2", R01S_LVL_H);
        r01s_avr128db28_m_soft_sel_poll(&chip);
        expect_true(r01s_avr128db28_m_soft_read(&chip, 0x40u) == 0xA5u, "soft sel FE40");
        r01s_entity_drive(e, "SEL_SOFT2", R01S_LVL_L);
        r01s_avr128db28_m_soft_sel_poll(&chip);
    }

    /* Machine EE $7F72 asserts RDY for write holds. */
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x70u, 0x00), "FE70 AL");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x71u, 0x00), "FE71 AH");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x72u, 0x5A), "FE72 data");
    expect_true(r01s_avr128db28_m_rdy_is_held(&chip), "RDY held after FE72");
    expect_true(r01s_avr128db28_m_rdy_hold(&chip) == R01_RDY_MEEPROM_WRITE_HOLDS, "RDY hold count");
    r01s_entity_eval(e);
    expect_true(r01s_level_is_low(r01s_entity_sense(e, "CPU_RDY")), "CPU_RDY low");
    for (i = 0; i < (int)R01_RDY_MEEPROM_WRITE_HOLDS; i++) {
        r01s_entity_drive(e, "CLK", R01S_LVL_H);
        r01s_entity_tick(e);
    }
    expect_true(!r01s_avr128db28_m_rdy_is_held(&chip), "RDY clear after holds");
    expect_true(r01s_avr128db28_m_soft_read(&chip, 0x72u) == 0x5A, "EE readback");
    r01s_avr128db28_m_soft_rdy_on_data(&chip, 0x72u);
    expect_true(r01s_avr128db28_m_rdy_hold(&chip) == R01_RDY_MEEPROM_READ_HOLDS, "RDY on FE72 read");

    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x22u, R01_CARTEE_CMD_WRITE), "cartee cmd");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x23u, 0x10), "cartee AL");
    expect_true(r01s_avr128db28_m_soft_write(&chip, 0x24u, 0xAB), "cartee data");
    expect_true(r01s_avr128db28_m_rdy_hold(&chip) == R01_RDY_CARTEE_WRITE_HOLDS, "RDY after FE24");

    r01s_avr128db28_m_eeprom_poke(&chip, 0, 0xA5);
    expect_true(r01s_avr128db28_m_eeprom_peek(&chip, 0) == 0xA5, "eeprom mailbox");

    r01s_entity_drive(e, "CLK", R01S_LVL_H);
    r01s_entity_tick(e);
    expect_true(r01s_avr128db28_m_clk_ticks(&chip) >= 1, "24 MHz domain ticks");
    expect_true(r01s_avr128db28_m_alive(&chip), "alive");

    return test_done("test_avr128db28_m");
}

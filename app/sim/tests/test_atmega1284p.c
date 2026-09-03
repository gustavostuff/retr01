#include "atmega1284p.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

#include <stdio.h>

static void drive_a0(R01sEntity *e, int data_port) {
    r01s_entity_drive(e, "A0", data_port ? R01S_LVL_H : R01S_LVL_L);
}

static void write_port(R01sEntity *e, int data_port, uint8_t v) {
    drive_a0(e, data_port);
    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_drive(e, "WE#", R01S_LVL_L);
    r01s_bus_write(e, "DQ", 8, v);
    r01s_entity_eval(e);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_eval(e);
    r01s_entity_drive(e, "CE#", R01S_LVL_H);
}

static uint8_t read_port(R01sEntity *e, int data_port) {
    uint8_t v;
    drive_a0(e, data_port);
    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_eval(e);
    v = (uint8_t)r01s_bus_read(e, "DQ", 8);
    r01s_entity_drive(e, "CE#", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
    r01s_entity_eval(e);
    return v;
}

int main(void) {
    R01sAtmega1284p chip;
    R01sEntity *e;
    int i;

    r01s_atmega1284p_init(&chip, "U1284");
    e = r01s_atmega1284p_entity(&chip);
    expect_true(e->dip_pins == 40, "40-pin package");

    write_port(e, 0, 0x00); /* $FE20 addr */
    write_port(e, 1, 0x10); /* Y */
    write_port(e, 1, 0x01); /* tile */
    write_port(e, 1, 0x00); /* attr */
    write_port(e, 1, 0x20); /* X */
    expect_true(r01s_atmega1284p_oam_peek(&chip, 0) == 0x10, "OAM Y");
    expect_true(r01s_atmega1284p_oam_peek(&chip, 1) == 0x01, "OAM tile");
    expect_true(r01s_atmega1284p_oam_peek(&chip, 3) == 0x20, "OAM X");
    expect_true(r01s_atmega1284p_oam_addr(&chip) == 4, "auto-inc to 4");

    write_port(e, 0, 0x00);
    expect_true(read_port(e, 1) == 0x10, "readback Y");
    expect_true(read_port(e, 1) == 0x01, "readback tile");
    expect_true(r01s_atmega1284p_oam_addr(&chip) == 2, "addr after 2 reads");

    r01s_entity_drive(e, "CLK", R01S_LVL_H);
    for (i = 0; i < 8; i++) {
        r01s_entity_tick(e);
    }
    expect_true(r01s_atmega1284p_clk_ticks(&chip) >= 8, "20 MHz domain ticks");
    expect_true(r01s_atmega1284p_alive(&chip), "alive");
    expect_true(r01s_entity_sense(e, "RUN") == R01S_LVL_H, "RUN high");

    r01s_atmega1284p_eeprom_poke(&chip, 0, 0xA5);
    expect_true(r01s_atmega1284p_eeprom_peek(&chip, 0) == 0xA5, "eeprom mailbox");

    return test_done("test_atmega1284p");
}

#include "prg_rom.h"
#include "retr01_sim/bus.h"
#include "test_common.h"

int main(void) {
    R01sPrgRom chip;
    R01sEntity *e;
    uint8_t prog[] = {0xEA, 0xEA, 0x4C, 0x00, 0x80}; /* NOP NOP JMP $8000 */

    r01s_prg_rom_init(&chip, "U4");
    e = r01s_prg_rom_entity(&chip);

    r01s_prg_rom_load(&chip, 0x0000, prog, (uint16_t)sizeof(prog));
    r01s_prg_rom_set_reset_vec(&chip, 0x8000);
    expect_true(r01s_prg_rom_peek(&chip, 0x7FFC) == 0x00, "vec lo");
    expect_true(r01s_prg_rom_peek(&chip, 0x7FFD) == 0x80, "vec hi");

    r01s_entity_drive(e, "CE#", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_bus_write(e, "A", 15, 0);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "DQ0") == R01S_LVL_Z, "CE# high Hi-Z");

    r01s_entity_drive(e, "CE#", R01S_LVL_L);
    r01s_entity_drive(e, "OE#", R01S_LVL_L);
    r01s_bus_write(e, "A", 15, 0);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0xEA, "read NOP");

    r01s_bus_write(e, "A", 15, 0x7FFC);
    r01s_entity_eval(e);
    expect_true(r01s_bus_read(e, "DQ", 8) == 0x00, "read reset lo");

    return test_done("test_prg_rom");
}

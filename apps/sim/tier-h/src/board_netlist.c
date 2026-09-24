#include "retr01_sim/board_netlist.h"

#include "retr01_sim/board.h"
#include "retr01_sim/bom32.h"

#include <stdio.h>

void r01s_board_schematic_apply(R01sBoard *board, R01sPinNetlist *nl);

static void netlist_register(R01sPinNetlist *nl, R01sEntity *e) {
    r01s_pin_netlist_register_entity(nl, e);
}

static void netlist_link_beam_y_beam(R01sPinNetlist *nl, R01sEntity *beam_y, R01sEntity *beam) {
    int i;
    for (i = 0; i < 8; i++) {
        char pn[8];
        char yn[8];
        snprintf(pn, sizeof(pn), "P%d", i);
        snprintf(yn, sizeof(yn), "Y%d", i);
        r01s_pin_netlist_link(nl, beam_y, pn, beam, yn);
    }
}

static void netlist_register_silicon(R01sBoard *board, R01sPinNetlist *nl) {
    int i;
    netlist_register(nl, r01s_pwr5v_entity(&board->pwr));
    netlist_register(nl, r01s_osc8m_entity(&board->osc));
    netlist_register(nl, r01s_w65c02s_entity(&board->cpu));
    netlist_register(nl, r01s_as6c62256_entity(&board->ram));
    netlist_register(nl, r01s_as6c62256_entity(&board->vram));
    netlist_register(nl, r01s_as6c62256_entity(&board->linebuf));
    netlist_register(nl, r01s_prg_rom_entity(&board->prg));
    netlist_register(nl, r01s_sst39sf040_entity(&board->cart_module.flash));
    netlist_register(nl, r01s_i2c_eeprom_entity(&board->cart_module.save));
    netlist_register(nl, r01s_avr128db28_m_entity(&board->mcu_m));
    netlist_register(nl, r01s_avr128db28_s1_entity(&board->mcu_s1));
    netlist_register(nl, r01s_avr128db28_s2_entity(&board->mcu_s2));
    netlist_register(nl, r01s_pads_entity(&board->pads));
    netlist_register(nl, r01s_attiny85_entity(&board->pad_mcu[0]));
    netlist_register(nl, r01s_attiny85_entity(&board->pad_mcu[1]));
    netlist_register(nl, r01s_atf22v10_entity(&board->pld_decode));
    netlist_register(nl, r01s_atf22v10_entity(&board->pld_vram));
    netlist_register(nl, r01s_beam_xy_entity(&board->pld_beam_x));
    netlist_register(nl, r01s_atf22v10_entity(&board->pld_beam_y));
    netlist_register(nl, r01s_osc_dot_entity(&board->osc_dot));
    netlist_register(nl, r01s_compositor_entity(&board->compositor));
    netlist_register(nl, r01s_at27c256r_entity(&board->color_prom));
    netlist_register(nl, r01s_video_sink_entity(&board->video_sink));
    netlist_register(nl, r01s_sn74hc573_entity(&board->field_ale));
    netlist_register(nl, r01s_sn74hc574_entity(&board->scroll_x));
    netlist_register(nl, r01s_bg_fetch_entity(&board->bg_fetch));
    netlist_register(nl, r01s_sprite_fetch_entity(&board->sprite_fetch));
    netlist_register(nl, r01s_integration_entity(&board->integration));
    for (i = 0; i < R01S_BOM_HC157_N; i++) {
        netlist_register(nl, r01s_sn74hc157_entity(&board->mux157[i]));
    }
    for (i = 0; i < board->passives.count; i++) {
        netlist_register(nl, &board->passives.parts[i].base);
    }
}

static void netlist_link_motherboard(R01sBoard *board, R01sPinNetlist *nl) {
    R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
    R01sEntity *ram = r01s_as6c62256_entity(&board->ram);
    R01sEntity *prg = r01s_prg_rom_entity(&board->prg);
    R01sEntity *flash = r01s_sst39sf040_entity(&board->cart_module.flash);
    R01sEntity *vram = r01s_as6c62256_entity(&board->vram);
    R01sEntity *mcu = r01s_avr128db28_m_entity(&board->mcu_m);
    R01sEntity *s1 = r01s_avr128db28_s1_entity(&board->mcu_s1);
    R01sEntity *apu = r01s_avr128db28_s2_entity(&board->mcu_s2);
    R01sEntity *pads = r01s_pads_entity(&board->pads);
    R01sEntity *beam = r01s_beam_xy_entity(&board->pld_beam_x);
    R01sEntity *beam_y = r01s_atf22v10_entity(&board->pld_beam_y);
    R01sEntity *pld = r01s_atf22v10_entity(&board->pld_decode);
    R01sEntity *mux_vram = r01s_sn74hc157_entity(board->vram_impl.mux157[R01S_MUX157_VRAM0]);
    R01sEntity *field_ale = r01s_sn74hc573_entity(board->mcu_lb_impl.field_ale);
    R01sEntity *sram_lb = r01s_as6c62256_entity(&board->linebuf);
    int i;

    r01s_pin_netlist_link_bus(nl, cpu, "A", ram, "A", 16);
    r01s_pin_netlist_link_bus(nl, cpu, "D", ram, "DQ", 8);
    r01s_pin_netlist_link_bus(nl, cpu, "A", prg, "A", 16);
    r01s_pin_netlist_link_bus(nl, cpu, "D", prg, "DQ", 8);
    r01s_pin_netlist_link_bus(nl, cpu, "A", vram, "A", 16);
    r01s_pin_netlist_link_bus(nl, cpu, "D", vram, "DQ", 8);
    r01s_pin_netlist_link_bus(nl, cpu, "D", mcu, "CPU_D", 8);

    r01s_pin_netlist_link(nl, mcu, "SPI_MOSI", apu, "SPI_MOSI");
    r01s_pin_netlist_link(nl, mcu, "SPI_MISO", apu, "SPI_MISO");
    r01s_pin_netlist_link(nl, mcu, "SPI_SCK", apu, "SPI_SCK");
    r01s_pin_netlist_link(nl, mcu, "/SS_S2", apu, "/SS_S2");
    r01s_pin_netlist_link(nl, mcu, "SPI_MOSI", s1, "SPI_MOSI");
    r01s_pin_netlist_link(nl, mcu, "SPI_MISO", s1, "SPI_MISO");
    r01s_pin_netlist_link(nl, mcu, "SPI_SCK", s1, "SPI_SCK");
    r01s_pin_netlist_link(nl, mcu, "/SS_S1", s1, "/SS_S1");
    r01s_pin_netlist_link(nl, s1, "S1_RDY", mcu, "S1_RDY");
    r01s_pin_netlist_link_bus(nl, cpu, "D", pads, "DQ", 8);

    for (i = 0; i < 16; i++) {
        char an[8];
        snprintf(an, sizeof(an), "A%d", i);
        r01s_pin_netlist_link(nl, cpu, an, flash, an);
    }

    r01s_pin_netlist_link_bus(nl, cpu, "A", pld, "A", 8);
    r01s_pin_netlist_link(nl, cpu, "BE", pld, "BE");
    r01s_pin_netlist_link(nl, cpu, "RWB", pld, "RWB");

    netlist_link_beam_y_beam(nl, beam_y, beam);
    r01s_pin_netlist_link(nl, cpu, "IRQB", beam_y, "EQ#");

    r01s_pin_netlist_link(nl, vram, "A0", mux_vram, "1Y");
    r01s_pin_netlist_link(nl, vram, "A1", mux_vram, "2Y");
    r01s_pin_netlist_link(nl, vram, "A2", mux_vram, "3Y");
    r01s_pin_netlist_link(nl, vram, "A3", mux_vram, "4Y");

    r01s_pin_netlist_link(nl, sram_lb, "A0", field_ale, "Q0");
    r01s_pin_netlist_link(nl, sram_lb, "A1", field_ale, "Q1");
    r01s_pin_netlist_link(nl, sram_lb, "A2", field_ale, "Q2");
    r01s_pin_netlist_link(nl, sram_lb, "A3", field_ale, "Q3");
    r01s_pin_netlist_link(nl, sram_lb, "A4", field_ale, "Q4");
    r01s_pin_netlist_link(nl, sram_lb, "A5", field_ale, "Q5");
    r01s_pin_netlist_link(nl, sram_lb, "A6", field_ale, "Q6");
    r01s_pin_netlist_link(nl, sram_lb, "A7", field_ale, "Q7");

    r01s_pin_netlist_name_net(nl, r01s_pwr5v_entity(&board->pwr), "VDD", "+5V");
    r01s_pin_netlist_name_net(nl, r01s_pwr5v_entity(&board->pwr), "GND", "GND");
}

void r01s_board_netlist_rebuild(R01sBoard *board) {
    if (!board) {
        return;
    }
    if (board->passives.count <= 0) {
        (void)r01s_passive_bank_spawn_bom(&board->passives);
    }
    r01s_pin_netlist_clear(&board->pin_netlist);
    netlist_register_silicon(board, &board->pin_netlist);
    netlist_link_motherboard(board, &board->pin_netlist);
    r01s_board_schematic_apply(board, &board->pin_netlist);
}

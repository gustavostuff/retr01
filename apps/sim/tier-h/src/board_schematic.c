#include "retr01_sim/board.h"

#include <stdio.h>
#include <string.h>

/*
 * Passive ↔ silicon links from docs/passive_bom.md and tier-a DAC pattern.
 * AD724 analog encoding is stubbed. C17 bypasses the MCP130.
 */

static R01sEntity *passive_entity(R01sBoard *board, const char *refdes) {
  int i;
  if (!board || !refdes) {
    return NULL;
  }
  for (i = 0; i < board->passives.count; i++) {
    if (board->passives.parts[i].base.refdes &&
        strcmp(board->passives.parts[i].base.refdes, refdes) == 0) {
      return &board->passives.parts[i].base;
    }
  }
  return NULL;
}

static R01sEntity *ic_entity(R01sBoard *board, const char *refdes) {
  int i;
  if (!board || !refdes) {
    return NULL;
  }
  if (strcmp(refdes, "PS1") == 0) {
    return r01s_pwr5v_entity(&board->pwr);
  }
  if (strcmp(refdes, "U1") == 0) {
    return r01s_w65c02s_entity(&board->cpu);
  }
  if (strcmp(refdes, "U3") == 0) {
    return r01s_as6c62256_entity(&board->ram);
  }
  if (strcmp(refdes, "U4") == 0) {
    return r01s_prg_rom_entity(&board->prg);
  }
  if (strcmp(refdes, "U6") == 0) {
    return r01s_as6c62256_entity(&board->vram);
  }
  if (strcmp(refdes, "U41") == 0) {
    return r01s_as6c62256_entity(&board->linebuf);
  }
  if (strcmp(refdes, "UM") == 0) {
    return r01s_avr128db28_m_entity(&board->mcu_m);
  }
  if (strcmp(refdes, "US1") == 0) {
    return r01s_avr128db28_s1_entity(&board->mcu_s1);
  }
  if (strcmp(refdes, "US2") == 0) {
    return r01s_avr128db28_s2_entity(&board->mcu_s2);
  }
  if (strcmp(refdes, "UPLDX") == 0) {
    return r01s_beam_xy_entity(&board->pld_beam_x);
  }
  if (strcmp(refdes, "UPLDY") == 0) {
    return r01s_atf22v10_entity(&board->pld_beam_y);
  }
  if (strcmp(refdes, "UPLDV") == 0) {
    return r01s_compositor_entity(&board->compositor);
  }
  if (strcmp(refdes, "U573") == 0) {
    return r01s_sn74hc573_entity(&board->field_ale);
  }
  if (strcmp(refdes, "U574") == 0) {
    return r01s_sn74hc574_entity(&board->scroll_x);
  }
  if (strcmp(refdes, "U24") == 0) {
    return r01s_at27c256r_entity(&board->color_prom);
  }
  if (strcmp(refdes, "U40") == 0) {
    return r01s_sst39sf040_entity(&board->cart_module.flash);
  }
  if (strcmp(refdes, "U50") == 0) {
    return r01s_i2c_eeprom_entity(&board->cart_module.save);
  }
  if (strcmp(refdes, "UPAD1") == 0) {
    return r01s_attiny85_entity(&board->pad_mcu[0]);
  }
  if (strcmp(refdes, "SCR1") == 0) {
    return r01s_video_sink_entity(&board->video_sink);
  }
  if (strcmp(refdes, "U04") == 0) {
    return r01a_sn74hcu04_entity(&board->u04);
  }
  if (strcmp(refdes, "U74") == 0) {
    return r01a_sn74hc74_entity(&board->u74);
  }
  if (strcmp(refdes, "U725") == 0) {
    return r01s_ad724_entity(&board->ad724);
  }
  if (strcmp(refdes, "J8") == 0) {
    return r01s_rca_jack_entity(&board->j8);
  }
  if (strcmp(refdes, "J9") == 0) {
    return r01s_rca_jack_entity(&board->j9);
  }
  if (strcmp(refdes, "U130") == 0) {
    return &board->io.u130;
  }
  if (strcmp(refdes, "U7A") == 0 || strcmp(refdes, "U7B") == 0 ||
      strcmp(refdes, "U7C") == 0) {
    static const char *const mux_ref[R01S_BOM_HC157_N] = {"U7A", "U7B", "U7C"};
    for (i = 0; i < R01S_BOM_HC157_N; i++) {
      if (strcmp(refdes, mux_ref[i]) == 0) {
        return r01s_sn74hc157_entity(&board->mux157[i]);
      }
    }
  }
  return NULL;
}

static void link_series(R01sPinNetlist *nl, R01sEntity *a, const char *ap,
                        R01sEntity *r, R01sEntity *b, const char *bp) {
  if (!nl || !a || !r || !b) {
    return;
  }
  r01s_pin_netlist_link(nl, a, ap, r, "1");
  r01s_pin_netlist_link(nl, r, "2", b, bp);
}

static void link_bypass(R01sPinNetlist *nl, R01sEntity *pwr, R01sEntity *cap,
                        R01sEntity *ic, const char *vcc_pin) {
  if (!nl || !pwr || !cap) {
    return;
  }
  r01s_pin_netlist_link(nl, cap, "2", pwr, "GND");
  if (ic && vcc_pin) {
    r01s_pin_netlist_link(nl, cap, "1", ic, vcc_pin);
    r01s_pin_netlist_link(nl, pwr, "VDD", ic, vcc_pin);
  } else {
    r01s_pin_netlist_link(nl, cap, "1", pwr, "VDD");
  }
}

static void tie_gnd(R01sPinNetlist *nl, R01sEntity *pwr, R01sEntity *ic,
                    const char *gnd_pin) {
  if (nl && pwr && ic && gnd_pin) {
    r01s_pin_netlist_link(nl, pwr, "GND", ic, gnd_pin);
  }
}

/* Bypass cap pin 1 to IC VCC, pin 2 to GND. UI stars non-IC rail pins to the 5V/GND symbols. */
static void apply_bypass(R01sBoard *board, R01sPinNetlist *nl) {
  static const struct {
    const char *cap;
    const char *ic;
    const char *vcc;
  } rows[] = {
      {"C1", "U1", "VDD"},     {"C2", "U3", "VCC"},    {"C3", "U6", "VCC"},
      {"C4", "U41", "VCC"},    {"C5", "UM", "VDD"},    {"C6", "US1", "VDD"},
      {"C7", "US2", "VDD"},    {"C8", "UPLDX", "VCC"}, {"C9", "UPLDY", "VCC"},
      {"C10", "UPLDV", "VCC"}, {"C11", "U7A", "VCC"},  {"C12", "U7B", "VCC"},
      {"C13", "U7C", "VCC"},   {"C14", "U573", "VCC"}, {"C15", "U574", "VCC"},
      {"C16", "U24", "VCC"},   {"C17", "U130", "VDD"}, {"C18", "U725", "APOS"},
      {"C19", "U04", "VCC"},   {"C20", "U74", "VCC"},
  };
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  size_t i;
  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    R01sEntity *cap = passive_entity(board, rows[i].cap);
    R01sEntity *ic = rows[i].ic ? ic_entity(board, rows[i].ic) : NULL;
    link_bypass(nl, pwr, cap, ic, rows[i].vcc);
  }
}

static void apply_bulk(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  R01sEntity *e1 = passive_entity(board, "E1");
  R01sEntity *v5 = passive_entity(board, "5V");
  R01sEntity *gnd = passive_entity(board, "GND");
  if (e1) {
    r01s_pin_netlist_link(nl, e1, "+", pwr, "VDD");
    r01s_pin_netlist_link(nl, e1, "-", pwr, "GND");
  }
  if (v5) {
    r01s_pin_netlist_link(nl, v5, "5V", pwr, "VDD");
  }
  if (gnd) {
    r01s_pin_netlist_link(nl, gnd, "GND", pwr, "GND");
  }
}

static void apply_clocks(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  R01sEntity *u04 = r01a_sn74hcu04_entity(&board->u04);
  R01sEntity *u74 = r01a_sn74hc74_entity(&board->u74);
  R01sEntity *u725 = r01s_ad724_entity(&board->ad724);
  R01sEntity *y1 = passive_entity(board, "Y1");
  R01sEntity *y2 = passive_entity(board, "Y2");
  R01sEntity *y3 = passive_entity(board, "Y3");
  R01sEntity *r23 = passive_entity(board, "R23");
  R01sEntity *r24 = passive_entity(board, "R24");

  /* Y2 21.47727 MHz Pierce (U04 gates 1-2) + U74 /4 -> DOT. */
  if (u04 && y2) {
    r01s_pin_netlist_link(nl, y2, "1", u04, "1A");
    r01s_pin_netlist_link(nl, y2, "2", u04, "1Y");
    r01s_pin_netlist_link(nl, u04, "2A", u04, "1Y");
    r01s_pin_netlist_name_net(nl, u04, "1A", "XTAL_21M_IN");
    r01s_pin_netlist_name_net(nl, u04, "1Y", "XTAL_21M_OUT");
  }
  if (r24 && u04) {
    r01s_pin_netlist_link(nl, r24, "1", u04, "1A");
    r01s_pin_netlist_link(nl, r24, "2", u04, "1Y");
  }
  r01s_pin_netlist_link(nl, passive_entity(board, "C21"), "1", y2, "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "C21"), "2", pwr, "GND");
  r01s_pin_netlist_link(nl, passive_entity(board, "C22"), "1", y2, "2");
  r01s_pin_netlist_link(nl, passive_entity(board, "C22"), "2", pwr, "GND");
  if (u04 && u74) {
    r01s_pin_netlist_link(nl, u04, "2Y", u74, "1CLK");
    r01s_pin_netlist_name_net(nl, u04, "2Y", "CLK_21M");
    r01s_pin_netlist_link(nl, u74, "1CLR#", pwr, "VDD");
    r01s_pin_netlist_link(nl, u74, "1PRE#", pwr, "VDD");
    r01s_pin_netlist_link(nl, u74, "1D", u74, "1/Q");
    r01s_pin_netlist_link(nl, u74, "2PRE#", pwr, "VDD");
    r01s_pin_netlist_link(nl, u74, "2CLR#", pwr, "VDD");
    r01s_pin_netlist_link(nl, u74, "2CLK", u74, "1Q");
    r01s_pin_netlist_link(nl, u74, "2D", u74, "2/Q");
    r01s_pin_netlist_name_net(nl, u74, "2Q", "DOT");
  }

  /* Y1 8.000 MHz Pierce (U04 gates 3-4) -> PHI2. */
  if (u04 && y1) {
    r01s_pin_netlist_link(nl, y1, "1", u04, "3A");
    r01s_pin_netlist_link(nl, y1, "2", u04, "3Y");
    r01s_pin_netlist_link(nl, u04, "4A", u04, "3Y");
    r01s_pin_netlist_name_net(nl, u04, "3A", "XTAL_CPU_IN");
    r01s_pin_netlist_name_net(nl, u04, "3Y", "XTAL_CPU_OUT");
    r01s_pin_netlist_name_net(nl, u04, "4Y", "PHI2");
  }
  if (r23 && u04) {
    r01s_pin_netlist_link(nl, r23, "1", u04, "3A");
    r01s_pin_netlist_link(nl, r23, "2", u04, "3Y");
  }
  r01s_pin_netlist_link(nl, passive_entity(board, "C25"), "1", y1, "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "C25"), "2", pwr, "GND");
  r01s_pin_netlist_link(nl, passive_entity(board, "C26"), "1", y1, "2");
  r01s_pin_netlist_link(nl, passive_entity(board, "C26"), "2", pwr, "GND");
  if (u04) {
    r01s_pin_netlist_link(nl, u04, "5A", pwr, "GND");
    r01s_pin_netlist_link(nl, u04, "6A", pwr, "GND");
  }

  /* Y3 3.579545 MHz FSC into AD724 FIN. */
  if (y3) {
    r01s_pin_netlist_link(nl, passive_entity(board, "C23"), "1", y3, "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "C23"), "2", pwr, "GND");
    r01s_pin_netlist_link(nl, passive_entity(board, "C24"), "1", y3, "2");
    r01s_pin_netlist_link(nl, passive_entity(board, "C24"), "2", pwr, "GND");
    r01s_pin_netlist_name_net(nl, y3, "1", "FSC_FIN");
    r01s_pin_netlist_name_net(nl, y3, "2", "FSC_XTAL");
    if (u725) {
      r01s_pin_netlist_link(nl, y3, "1", u725, "FIN");
    }
  }
}

static void apply_dac(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *prom = r01s_at27c256r_entity(&board->color_prom);
  R01sEntity *sink = r01s_video_sink_entity(&board->video_sink);
  R01sEntity *enc = r01s_ad724_entity(&board->ad724);
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  static const struct {
    const char *prom_o;
    const char *r;
    const char *gun;
  } ladder[] = {
      {"O7", "R1", "RIN"}, {"O6", "R2", "RIN"}, {"O5", "R3", "RIN"},
      {"O4", "R4", "GIN"}, {"O3", "R5", "GIN"}, {"O2", "R6", "GIN"},
      {"O1", "R7", "BIN"}, {"O0", "R8", "BIN"},
  };
  size_t i;
  for (i = 0; i < sizeof(ladder) / sizeof(ladder[0]); i++) {
    R01sEntity *r = passive_entity(board, ladder[i].r);
    if (r) {
      r01s_pin_netlist_link(nl, prom, ladder[i].prom_o, r, "1");
      r01s_pin_netlist_link(nl, r, "2", sink, ladder[i].gun);
    }
  }
  r01s_pin_netlist_link(nl, sink, "RIN", passive_entity(board, "R9"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R9"), "2", pwr, "GND");
  r01s_pin_netlist_link(nl, sink, "GIN", passive_entity(board, "R10"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R10"), "2", pwr, "GND");
  r01s_pin_netlist_link(nl, sink, "BIN", passive_entity(board, "R11"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R11"), "2", pwr, "GND");
  r01s_pin_netlist_link(nl, sink, "AGND", pwr, "GND");
  if (enc) {
    r01s_pin_netlist_link(nl, enc, "RIN", sink, "RIN");
    r01s_pin_netlist_link(nl, enc, "GIN", sink, "GIN");
    r01s_pin_netlist_link(nl, enc, "BIN", sink, "BIN");
  }
}

static void apply_series_33(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
  R01sEntity *u04 = r01a_sn74hcu04_entity(&board->u04);
  R01sEntity *u74 = r01a_sn74hc74_entity(&board->u74);
  R01sEntity *beam = r01s_beam_xy_entity(&board->pld_beam_x);
  R01sEntity *flash = r01s_sst39sf040_entity(&board->cart_module.flash);
  R01sEntity *mcu = r01s_avr128db28_m_entity(&board->mcu_m);
  R01sEntity *ee = r01s_i2c_eeprom_entity(&board->cart_module.save);
  int i;

  link_series(nl, u04, "4Y", passive_entity(board, "R12"), cpu, "PHI2");
  link_series(nl, u74, "2Q", passive_entity(board, "R13"), beam, "DOT");

  for (i = 0; i < 8; i++) {
    char dn[8];
    char fq[8];
    snprintf(dn, sizeof(dn), "D%d", i);
    snprintf(fq, sizeof(fq), "DQ%d", i);
    r01s_pin_netlist_link(nl, cpu, dn, flash, fq);
  }
  r01s_pin_netlist_link(nl, passive_entity(board, "R14"), "2", flash, "OE#");
  r01s_pin_netlist_name_net(nl, passive_entity(board, "R14"), "1", "CART_OE#");
  r01s_pin_netlist_link(nl, passive_entity(board, "R15"), "2", flash, "WE#");
  r01s_pin_netlist_name_net(nl, passive_entity(board, "R15"), "1", "CART_WE#");
  link_series(nl, mcu, "SDA", passive_entity(board, "R16"), ee, "SDA");
  link_series(nl, mcu, "SCL", passive_entity(board, "R17"), ee, "SCL");
  /* Motherboard side of the cart nets also lands on J36. Flash stays on the same nets. */
  {
    R01sEntity *j36 = &board->io.j36;
    for (i = 0; i < 8; i++) {
      char dn[8];
      snprintf(dn, sizeof(dn), "D%d", i);
      r01s_pin_netlist_link(nl, cpu, dn, j36, dn);
    }
    for (i = 0; i < 14; i++) {
      char an[8];
      snprintf(an, sizeof(an), "A%d", i);
      r01s_pin_netlist_link(nl, cpu, an, j36, an);
    }
    r01s_pin_netlist_link(nl, passive_entity(board, "R14"), "2", j36, "OE#");
    r01s_pin_netlist_link(nl, passive_entity(board, "R15"), "2", j36, "WE#");
    r01s_pin_netlist_link(nl, passive_entity(board, "R16"), "2", j36, "SDA");
    r01s_pin_netlist_link(nl, passive_entity(board, "R17"), "2", j36, "SCL");
    r01s_pin_netlist_link(nl, r01s_pwr5v_entity(&board->pwr), "GND", j36, "GND_A1");
    r01s_pin_netlist_link(nl, r01s_pwr5v_entity(&board->pwr), "GND", j36, "GND_B1");
    r01s_pin_netlist_link(nl, r01s_pwr5v_entity(&board->pwr), "GND", j36, "GND_A18");
    r01s_pin_netlist_link(nl, r01s_pwr5v_entity(&board->pwr), "VDD", j36, "VCC_A");
    r01s_pin_netlist_link(nl, r01s_pwr5v_entity(&board->pwr), "VDD", j36, "VCC_B");
  }
}

static void apply_pullups(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
  R01sEntity *mcu = r01s_avr128db28_m_entity(&board->mcu_m);
  R01sEntity *apu = r01s_avr128db28_s2_entity(&board->mcu_s2);
  R01sEntity *pad = r01s_attiny85_entity(&board->pad_mcu[0]);

  r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R21"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R21"), "2", cpu, "RDY");
  r01s_pin_netlist_link(nl, mcu, "CPU_RDY", cpu, "RDY");

  r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R22"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R22"), "2", cpu, "RESB");

  r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R18"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R18"), "2", apu, "PAD_DATA");
  r01s_pin_netlist_link(nl, apu, "PAD_DATA", pad, "DATA");

  r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R19"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R19"), "2", mcu, "SDA");
  r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R20"), "1");
  r01s_pin_netlist_link(nl, passive_entity(board, "R20"), "2", mcu, "SCL");
}

static void apply_gnd_ties(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  tie_gnd(nl, pwr, r01s_w65c02s_entity(&board->cpu), "VSS");
  tie_gnd(nl, pwr, r01s_as6c62256_entity(&board->ram), "VSS");
  tie_gnd(nl, pwr, r01s_as6c62256_entity(&board->vram), "VSS");
  tie_gnd(nl, pwr, r01s_as6c62256_entity(&board->linebuf), "VSS");
  tie_gnd(nl, pwr, r01s_at27c256r_entity(&board->color_prom), "GND");
  tie_gnd(nl, pwr, r01s_sst39sf040_entity(&board->cart_module.flash), "VSS");
  tie_gnd(nl, pwr, r01s_i2c_eeprom_entity(&board->cart_module.save), "GND");
  tie_gnd(nl, pwr, r01s_i2c_eeprom_entity(&board->cart_module.save), "A0");
  tie_gnd(nl, pwr, r01s_i2c_eeprom_entity(&board->cart_module.save), "A1");
  tie_gnd(nl, pwr, r01s_i2c_eeprom_entity(&board->cart_module.save), "A2");
  tie_gnd(nl, pwr, r01s_i2c_eeprom_entity(&board->cart_module.save), "WP#");
  tie_gnd(nl, pwr, r01s_atf22v10_entity(&board->pld_beam_y), "GND");
  tie_gnd(nl, pwr, r01s_compositor_entity(&board->compositor), "GND");
  tie_gnd(nl, pwr, r01s_ad724_entity(&board->ad724), "AGND");
  tie_gnd(nl, pwr, r01s_ad724_entity(&board->ad724), "DGND");
  tie_gnd(nl, pwr, r01a_sn74hcu04_entity(&board->u04), "GND");
  tie_gnd(nl, pwr, r01a_sn74hc74_entity(&board->u74), "GND");
  tie_gnd(nl, pwr, r01s_sn74hc157_entity(&board->mux157[0]), "GND");
  tie_gnd(nl, pwr, r01s_sn74hc157_entity(&board->mux157[1]), "GND");
  tie_gnd(nl, pwr, r01s_sn74hc157_entity(&board->mux157[2]), "GND");
  tie_gnd(nl, pwr, r01s_sn74hc573_entity(board->mcu_lb_impl.field_ale), "GND");
  tie_gnd(nl, pwr, r01s_sn74hc574_entity(&board->scroll_x), "GND");
  tie_gnd(nl, pwr, r01s_beam_xy_entity(&board->pld_beam_x), "GND");
  tie_gnd(nl, pwr, r01s_avr128db28_m_entity(&board->mcu_m), "GND");
  tie_gnd(nl, pwr, r01s_avr128db28_m_entity(&board->mcu_m), "GND2");
  tie_gnd(nl, pwr, r01s_avr128db28_s1_entity(&board->mcu_s1), "GND");
  tie_gnd(nl, pwr, r01s_avr128db28_s1_entity(&board->mcu_s1), "GND2");
  tie_gnd(nl, pwr, r01s_avr128db28_s2_entity(&board->mcu_s2), "GND");
  tie_gnd(nl, pwr, r01s_avr128db28_s2_entity(&board->mcu_s2), "GND2");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_m_entity(&board->mcu_m), "VDDIO2");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_m_entity(&board->mcu_m), "AVDD");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_s1_entity(&board->mcu_s1), "VDDIO2");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_s1_entity(&board->mcu_s1), "AVDD");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_s2_entity(&board->mcu_s2), "VDDIO2");
  r01s_pin_netlist_link(nl, pwr, "VDD", r01s_avr128db28_s2_entity(&board->mcu_s2), "AVDD");
}

static void apply_rca_av(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  R01sEntity *enc = r01s_ad724_entity(&board->ad724);
  R01sEntity *j8 = r01s_rca_jack_entity(&board->j8);
  R01sEntity *j9 = r01s_rca_jack_entity(&board->j9);
  R01sEntity *apu = r01s_avr128db28_s2_entity(&board->mcu_s2);
  const char *shell[3] = {"1A", "1B", "1C"};
  int i;

  if (enc) {
    r01s_pin_netlist_link(nl, pwr, "VDD", enc, "DPOS");
    r01s_pin_netlist_link(nl, pwr, "VDD", enc, "STND");
    r01s_pin_netlist_link(nl, pwr, "VDD", enc, "ENCD");
    r01s_pin_netlist_link(nl, pwr, "VDD", enc, "VSYNC");
    r01s_pin_netlist_link(nl, pwr, "GND", enc, "SELECT");
    r01s_pin_netlist_name_net(nl, enc, "HSYNC", "CSYNC");
    r01s_pin_netlist_name_net(nl, enc, "COMP", "COMPOSITE_OUT");
  }
  if (j8) {
    for (i = 0; i < 3; i++) {
      r01s_pin_netlist_link(nl, j8, shell[i], pwr, "GND");
    }
    if (apu) {
      r01s_pin_netlist_link(nl, apu, "AUDIO_PWM", j8, "2");
    }
    r01s_pin_netlist_name_net(nl, j8, "2", "AUDIO_OUT");
  }
  if (j9 && enc) {
    for (i = 0; i < 3; i++) {
      r01s_pin_netlist_link(nl, j9, shell[i], pwr, "GND");
    }
    r01s_pin_netlist_link(nl, enc, "COMP", j9, "2");
  }
}

static void apply_vram_mux(R01sBoard *board, R01sPinNetlist *nl) {
  static const char *const a_pins[4] = {"1A", "2A", "3A", "4A"};
  static const char *const y_pins[4] = {"1Y", "2Y", "3Y", "4Y"};
  R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
  R01sEntity *vram = r01s_as6c62256_entity(&board->vram);
  int bit;
  /* A inputs are the CPU side. Select and beam-side inputs wait on the PLD phase decode. */
  for (bit = 0; bit < 12; bit++) {
    char an[8];
    R01sEntity *mux = r01s_sn74hc157_entity(&board->mux157[bit / 4]);
    snprintf(an, sizeof(an), "A%d", bit);
    r01s_pin_netlist_link(nl, cpu, an, mux, a_pins[bit % 4]);
    r01s_pin_netlist_link(nl, mux, y_pins[bit % 4], vram, an);
  }
}

static void apply_field_bus(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *s1 = r01s_avr128db28_s1_entity(&board->mcu_s1);
  R01sEntity *ale = r01s_sn74hc573_entity(&board->field_ale);
  R01sEntity *sram = r01s_as6c62256_entity(&board->linebuf);
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  int i;
  for (i = 0; i < 8; i++) {
    char ad[8];
    char dn[8];
    char dq[8];
    snprintf(ad, sizeof(ad), "AD%d", i);
    snprintf(dn, sizeof(dn), "D%d", i);
    snprintf(dq, sizeof(dq), "DQ%d", i);
    r01s_pin_netlist_link(nl, s1, ad, ale, dn);
    r01s_pin_netlist_link(nl, s1, ad, sram, dq);
  }
  for (i = 8; i <= 14; i++) {
    char an[8];
    snprintf(an, sizeof(an), "A%d", i);
    r01s_pin_netlist_link(nl, s1, an, sram, an);
  }
  r01s_pin_netlist_link(nl, s1, "ALE", ale, "LE");
  r01s_pin_netlist_link(nl, s1, "/WE", sram, "WE#");
  r01s_pin_netlist_link(nl, ale, "OE#", pwr, "GND");
}

static void apply_color_index(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *comp = r01s_compositor_entity(&board->compositor);
  R01sEntity *prom = r01s_at27c256r_entity(&board->color_prom);
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  int i;
  for (i = 0; i < 6; i++) {
    char an[8];
    snprintf(an, sizeof(an), "A%d", i);
    r01s_pin_netlist_link(nl, comp, an, prom, an);
  }
  for (i = 6; i <= 13; i++) {
    char an[8];
    snprintf(an, sizeof(an), "A%d", i);
    r01s_pin_netlist_link(nl, prom, an, pwr, "GND");
  }
  r01s_pin_netlist_link(nl, prom, "CE#", pwr, "GND");
  r01s_pin_netlist_link(nl, prom, "OE#", pwr, "GND");
}

static void apply_scroll_latch(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
  R01sEntity *lat = r01s_sn74hc574_entity(&board->scroll_x);
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  int i;
  for (i = 0; i < 8; i++) {
    char dn[8];
    snprintf(dn, sizeof(dn), "D%d", i);
    r01s_pin_netlist_link(nl, cpu, dn, lat, dn);
  }
  r01s_pin_netlist_link(nl, lat, "OE#", pwr, "GND");
}

static void apply_mobo_io_nets(R01sBoard *board, R01sPinNetlist *nl) {
  R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
  R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
  R01sEntity *mcu_m = r01s_avr128db28_m_entity(&board->mcu_m);
  R01sEntity *s2 = r01s_avr128db28_s2_entity(&board->mcu_s2);
  R01sEntity *sink = r01s_video_sink_entity(&board->video_sink);
  R01sMoboIo *io = &board->io;
  static const char *const p2[8] = {"RIGHT", "LEFT", "DOWN", "UP", "X", "Y", "COIN", "START"};
  static const char *const p2_pin[8] = {"P2_RIGHT", "P2_LEFT", "P2_DOWN", "P2_UP", "P2_X", "P2_Y", "P2_COIN", "P2_START"};
  int i;
  /* Barrel J1 feeds +5V directly. Cab power / reset are on J7 (no onboard SW1 / SW_RST). */
  r01s_pin_netlist_link(nl, &io->j1, "1", pwr, "VDD");
  r01s_pin_netlist_link(nl, &io->j1, "2", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->u130, "VDD", pwr, "VDD");
  r01s_pin_netlist_link(nl, &io->u130, "VSS", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->u130, "RESET#", cpu, "RESB");
  r01s_pin_netlist_link(nl, &io->j2, "R", sink, "RIN");
  r01s_pin_netlist_link(nl, &io->j2, "G", sink, "GIN");
  r01s_pin_netlist_link(nl, &io->j2, "B", sink, "BIN");
  r01s_pin_netlist_link(nl, &io->j2, "CSYNC", r01s_ad724_entity(&board->ad724), "HSYNC");
  r01s_pin_netlist_name_net(nl, &io->j2, "HSYNC", "HSYNC");
  r01s_pin_netlist_name_net(nl, &io->j2, "VSYNC", "VSYNC");
  r01s_pin_netlist_link(nl, &io->j2, "GND1", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j2, "GND2", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j3, "TIP", pwr, "VDD");
  r01s_pin_netlist_link(nl, &io->j3, "RING", s2, "PAD_DATA");
  r01s_pin_netlist_link(nl, &io->j3, "SLEEVE", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j4, "TIP", pwr, "VDD");
  r01s_pin_netlist_link(nl, &io->j4, "RING", s2, "PAD_DATA");
  r01s_pin_netlist_link(nl, &io->j4, "SLEEVE", pwr, "GND");
  for (i = 0; i < 8; i++) {
    char jn[4];
    char pn[8];
    snprintf(jn, sizeof(jn), "P%d", i);
    snprintf(pn, sizeof(pn), "PAD%d", i);
    r01s_pin_netlist_link(nl, &io->j5, jn, s2, pn);
  }
  for (i = 0; i < 8; i++) {
    r01s_pin_netlist_link(nl, &io->j5, p2[i], s2, p2_pin[i]);
  }
  r01s_pin_netlist_link(nl, &io->j5, "GND17", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j5, "GND18", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j5, "GND19", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j5, "GND20", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j7, "VCC", pwr, "VDD");
  r01s_pin_netlist_link(nl, &io->j7, "GND1", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j7, "GND2", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j7, "RESB", cpu, "RESB");
  r01s_pin_netlist_name_net(nl, &io->j10, "PWR", "J10_PWR_NC");
  r01s_pin_netlist_link(nl, &io->j10, "GND1", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j10, "GND2", pwr, "GND");
  r01s_pin_netlist_link(nl, &io->j10, "DATA", mcu_m, "SPI_MISO");
}

void r01s_board_schematic_apply(R01sBoard *board, R01sPinNetlist *nl) {
  if (!board || !nl) {
    return;
  }
  apply_bypass(board, nl);
  apply_bulk(board, nl);
  apply_gnd_ties(board, nl);
  apply_clocks(board, nl);
  apply_dac(board, nl);
  apply_series_33(board, nl);
  apply_pullups(board, nl);
  apply_rca_av(board, nl);
  apply_vram_mux(board, nl);
  apply_field_bus(board, nl);
  apply_color_index(board, nl);
  apply_scroll_latch(board, nl);
  apply_mobo_io_nets(board, nl);
}

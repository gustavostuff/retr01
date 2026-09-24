#include "retr01_sim/board.h"

#include <string.h>

/*
 * Passive ↔ silicon links from docs/passive_bom.md and tier-a DAC pattern.
 * AD724 / 74HC14 are not seated in sim; C17/C18 tie to +5V/GND rails only.
 */

static R01sEntity *passive_entity(R01sBoard *board, const char *refdes) {
    int i;
    if (!board || !refdes) {
        return NULL;
    }
    for (i = 0; i < board->passives.count; i++) {
        if (board->passives.parts[i].base.refdes && strcmp(board->passives.parts[i].base.refdes, refdes) == 0) {
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
    if (strcmp(refdes, "Y1") == 0) {
        return r01s_osc8m_entity(&board->osc);
    }
    if (strcmp(refdes, "Y2") == 0) {
        return r01s_osc_dot_entity(&board->osc_dot);
    }
    if (strcmp(refdes, "U7A") == 0 || strcmp(refdes, "U7B") == 0 || strcmp(refdes, "U7C") == 0) {
        static const char *const mux_ref[R01S_BOM_HC157_N] = {"U7A", "U7B", "U7C"};
        for (i = 0; i < R01S_BOM_HC157_N; i++) {
            if (strcmp(refdes, mux_ref[i]) == 0) {
                return r01s_sn74hc157_entity(&board->mux157[i]);
            }
        }
    }
    return NULL;
}

static void link_series(R01sPinNetlist *nl, R01sEntity *a, const char *ap, R01sEntity *r, R01sEntity *b,
                        const char *bp) {
    if (!nl || !a || !r || !b) {
        return;
    }
    r01s_pin_netlist_link(nl, a, ap, r, "1");
    r01s_pin_netlist_link(nl, r, "2", b, bp);
}

static void link_bypass(R01sPinNetlist *nl, R01sEntity *pwr, R01sEntity *cap, R01sEntity *ic,
                        const char *vcc_pin) {
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

static void tie_gnd(R01sPinNetlist *nl, R01sEntity *pwr, R01sEntity *ic, const char *gnd_pin) {
    if (nl && pwr && ic && gnd_pin) {
        r01s_pin_netlist_link(nl, pwr, "GND", ic, gnd_pin);
    }
}

static void apply_bypass(R01sBoard *board, R01sPinNetlist *nl) {
    static const struct {
        const char *cap;
        const char *ic;
        const char *vcc;
    } rows[] = {
        {"C1", "U1", "VDD"},
        {"C2", "U3", "VCC"},
        {"C3", "U6", "VCC"},
        {"C4", "U41", "VCC"},
        {"C5", "UM", "VCC"},
        {"C6", "US1", "VCC"},
        {"C7", "US2", "VCC"},
        {"C8", "UPLDX", "VCC"},
        {"C9", "UPLDY", "VCC"},
        {"C10", "UPLDV", "VCC"},
        {"C11", "U7A", "VCC"},
        {"C12", "U7B", "VCC"},
        {"C13", "U7C", "VCC"},
        {"C14", "U573", "VCC"},
        {"C15", "U574", "VCC"},
        {"C16", "U24", "VCC"},
        {"C17", NULL, NULL},
        {"C18", NULL, NULL},
        {"C19", "U40", "VDD"},
        {"C20", "U50", "VCC"},
        {"C21", "UPAD1", "VCC"},
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
    if (e1) {
        r01s_pin_netlist_link(nl, e1, "+", pwr, "VDD");
        r01s_pin_netlist_link(nl, e1, "-", pwr, "GND");
    }
}

static void apply_crystal_loads(R01sBoard *board, R01sPinNetlist *nl) {
    R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
    /* Passive Y1/Y2/Y3 are BOM crystals; Y1/Y2 functional osc chips share refdes (see docs). */
    R01sEntity *xt_y1 = passive_entity(board, "Y1");
    R01sEntity *xt_y2 = passive_entity(board, "Y2");
    R01sEntity *xt_y3 = passive_entity(board, "Y3");
    R01sEntity *osc_cpu = r01s_osc8m_entity(&board->osc);
    R01sEntity *osc_dot = r01s_osc_dot_entity(&board->osc_dot);

    if (xt_y1 && osc_cpu) {
        r01s_pin_netlist_link(nl, xt_y1, "14", osc_cpu, "VDD");
        r01s_pin_netlist_link(nl, xt_y1, "7", pwr, "GND");
        r01s_pin_netlist_link(nl, osc_cpu, "GND", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C22"), "1", xt_y1, "1");
        r01s_pin_netlist_link(nl, passive_entity(board, "C22"), "2", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C23"), "1", xt_y1, "8");
        r01s_pin_netlist_link(nl, passive_entity(board, "C23"), "2", pwr, "GND");
    }
    if (xt_y2 && osc_dot) {
        r01s_pin_netlist_link(nl, xt_y2, "14", osc_dot, "VDD");
        r01s_pin_netlist_link(nl, xt_y2, "7", pwr, "GND");
        r01s_pin_netlist_link(nl, osc_dot, "GND", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C24"), "1", xt_y2, "1");
        r01s_pin_netlist_link(nl, passive_entity(board, "C24"), "2", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C25"), "1", xt_y2, "8");
        r01s_pin_netlist_link(nl, passive_entity(board, "C25"), "2", pwr, "GND");
    }
    if (xt_y3) {
        r01s_pin_netlist_link(nl, xt_y3, "7", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C26"), "1", xt_y3, "1");
        r01s_pin_netlist_link(nl, passive_entity(board, "C26"), "2", pwr, "GND");
        r01s_pin_netlist_link(nl, passive_entity(board, "C27"), "1", xt_y3, "8");
        r01s_pin_netlist_link(nl, passive_entity(board, "C27"), "2", pwr, "GND");
        r01s_pin_netlist_name_net(nl, xt_y3, "8", "FSC_XTAL");
    }
}

static void apply_dac(R01sBoard *board, R01sPinNetlist *nl) {
    R01sEntity *prom = r01s_at27c256r_entity(&board->color_prom);
    R01sEntity *sink = r01s_video_sink_entity(&board->video_sink);
    R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
    static const struct {
        const char *prom_o;
        const char *r;
        const char *gun;
    } ladder[] = {
        {"O7", "R1", "RIN"},
        {"O6", "R2", "RIN"},
        {"O5", "R3", "RIN"},
        {"O4", "R4", "GIN"},
        {"O3", "R5", "GIN"},
        {"O2", "R6", "GIN"},
        {"O1", "R7", "BIN"},
        {"O0", "R8", "BIN"},
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
}

static void apply_series_33(R01sBoard *board, R01sPinNetlist *nl) {
    R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
    R01sEntity *osc = r01s_osc8m_entity(&board->osc);
    R01sEntity *dot = r01s_osc_dot_entity(&board->osc_dot);
    R01sEntity *beam = r01s_beam_xy_entity(&board->pld_beam_x);
    R01sEntity *flash = r01s_sst39sf040_entity(&board->cart_module.flash);
    R01sEntity *mcu = r01s_avr128db28_m_entity(&board->mcu_m);
    R01sEntity *ee = r01s_i2c_eeprom_entity(&board->cart_module.save);
    int i;

    link_series(nl, osc, "PHI2", passive_entity(board, "R12"), cpu, "PHI2");
    link_series(nl, dot, "DOT", passive_entity(board, "R13"), beam, "DOT");

    for (i = 0; i < 8; i++) {
        char rn[8];
        char dn[8];
        char fq[8];
        snprintf(rn, sizeof(rn), "R%d", 14 + i);
        snprintf(dn, sizeof(dn), "D%d", i);
        snprintf(fq, sizeof(fq), "DQ%d", i);
        link_series(nl, cpu, dn, passive_entity(board, rn), flash, fq);
    }
    r01s_pin_netlist_link(nl, passive_entity(board, "R22"), "2", flash, "OE#");
    r01s_pin_netlist_name_net(nl, passive_entity(board, "R22"), "1", "CART_OE#");
    r01s_pin_netlist_link(nl, passive_entity(board, "R23"), "2", flash, "WE#");
    r01s_pin_netlist_name_net(nl, passive_entity(board, "R23"), "1", "CART_WE#");
    link_series(nl, mcu, "SDA", passive_entity(board, "R24"), ee, "SDA");
    link_series(nl, mcu, "SCL", passive_entity(board, "R25"), ee, "SCL");
}

static void apply_pullups(R01sBoard *board, R01sPinNetlist *nl) {
    R01sEntity *pwr = r01s_pwr5v_entity(&board->pwr);
    R01sEntity *cpu = r01s_w65c02s_entity(&board->cpu);
    R01sEntity *mcu = r01s_avr128db28_m_entity(&board->mcu_m);
    R01sEntity *apu = r01s_avr128db28_s2_entity(&board->mcu_s2);
    R01sEntity *pad = r01s_attiny85_entity(&board->pad_mcu[0]);

    r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R29"), "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "R29"), "2", cpu, "RDY");
    r01s_pin_netlist_link(nl, mcu, "CPU_RDY", cpu, "RDY");

    r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R30"), "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "R30"), "2", cpu, "RESB");

    r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R26"), "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "R26"), "2", apu, "PAD_DATA");
    r01s_pin_netlist_link(nl, apu, "PAD_DATA", pad, "DATA");

    r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R27"), "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "R27"), "2", mcu, "SDA");
    r01s_pin_netlist_link(nl, pwr, "VDD", passive_entity(board, "R28"), "1");
    r01s_pin_netlist_link(nl, passive_entity(board, "R28"), "2", mcu, "SCL");
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
}

void r01s_board_schematic_apply(R01sBoard *board, R01sPinNetlist *nl) {
    if (!board || !nl) {
        return;
    }
    apply_bypass(board, nl);
    apply_bulk(board, nl);
    apply_gnd_ties(board, nl);
    apply_crystal_loads(board, nl);
    apply_dac(board, nl);
    apply_series_33(board, nl);
    apply_pullups(board, nl);
}

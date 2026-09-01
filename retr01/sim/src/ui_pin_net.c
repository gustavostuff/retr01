#include "ui.h"
#include "ui_internal.h"

#include "retr01_sim/board.h"
#include "retr01_sim/bom32.h"
#include "retr01_sim/bus.h"

#include <stdio.h>
#include <string.h>

#define R01S_UI_PIN_NET_SLOTS 1024

typedef struct R01sUiPinNetSlot {
    R01sEntity *entity;
    int pin_index;
} R01sUiPinNetSlot;

typedef struct R01sUiPinNet {
    R01sUiPinNetSlot slots[R01S_UI_PIN_NET_SLOTS];
    int parent[R01S_UI_PIN_NET_SLOTS];
    int slot_count;
} R01sUiPinNet;

static R01sUiPinNet g_pin_net;

static int pin_net_find_slot(R01sUiPinNet *g, R01sEntity *e, int pin_index) {
    int i;
    if (!g || !e || pin_index < 0) {
        return -1;
    }
    for (i = 0; i < g->slot_count; i++) {
        if (g->slots[i].entity == e && g->slots[i].pin_index == pin_index) {
            return i;
        }
    }
    return -1;
}

static int pin_net_add_slot(R01sUiPinNet *g, R01sEntity *e, int pin_index) {
    int i;
    if (!g || !e || pin_index < 0) {
        return -1;
    }
    i = pin_net_find_slot(g, e, pin_index);
    if (i >= 0) {
        return i;
    }
    if (g->slot_count >= R01S_UI_PIN_NET_SLOTS) {
        return -1;
    }
    i = g->slot_count++;
    g->slots[i].entity = e;
    g->slots[i].pin_index = pin_index;
    g->parent[i] = i;
    return i;
}

static int pin_net_slot_for_name(R01sUiPinNet *g, R01sEntity *e, const char *name) {
    const R01sPin *pin;
    int pin_index;
    if (!g || !e || !name) {
        return -1;
    }
    pin = r01s_entity_pin_named_const(e, name);
    if (!pin) {
        return -1;
    }
    pin_index = (int)(pin - e->pins);
    if (pin_index < 0 || pin_index >= e->pin_count) {
        return -1;
    }
    return pin_net_add_slot(g, e, pin_index);
}

static int pin_net_root(R01sUiPinNet *g, int slot) {
    int p;
    if (!g || slot < 0 || slot >= g->slot_count) {
        return -1;
    }
    p = g->parent[slot];
    while (p != g->parent[p]) {
        p = g->parent[p];
    }
    while (g->parent[slot] != p) {
        int next = g->parent[slot];
        g->parent[slot] = p;
        slot = next;
    }
    return p;
}

static void pin_net_union(R01sUiPinNet *g, int a, int b) {
    int ra;
    int rb;
    if (!g || a < 0 || b < 0) {
        return;
    }
    ra = pin_net_root(g, a);
    rb = pin_net_root(g, b);
    if (ra < 0 || rb < 0 || ra == rb) {
        return;
    }
    g->parent[rb] = ra;
}

static void pin_net_link(R01sUiPinNet *g, R01sEntity *ea, const char *an, R01sEntity *eb, const char *bn) {
    int sa;
    int sb;
    if (!g || !ea || !eb) {
        return;
    }
    sa = pin_net_slot_for_name(g, ea, an);
    sb = pin_net_slot_for_name(g, eb, bn);
    if (sa >= 0 && sb >= 0) {
        pin_net_union(g, sa, sb);
    }
}

static void pin_net_link_bus(R01sUiPinNet *g, R01sEntity *ea, const char *ap, R01sEntity *eb, const char *bp,
                             int width) {
    int i;
    char an[16];
    char bn[16];
    for (i = 0; i < width; i++) {
        snprintf(an, sizeof(an), "%s%d", ap, i);
        snprintf(bn, sizeof(bn), "%s%d", bp, i);
        pin_net_link(g, ea, an, eb, bn);
    }
}

static void pin_net_link_cpu_d_latch(R01sUiPinNet *g, R01sEntity *cpu, R01sEntity *latch) {
    int i;
    char ln[8];
    char cn[8];
    for (i = 0; i < 8; i++) {
        snprintf(ln, sizeof(ln), "%dD", i + 1);
        snprintf(cn, sizeof(cn), "D%d", i);
        pin_net_link(g, cpu, cn, latch, ln);
        snprintf(ln, sizeof(ln), "%dQ", i + 1);
        pin_net_link(g, cpu, cn, latch, ln);
    }
}

static void pin_net_link_beam_y_raster(R01sUiPinNet *g, R01sEntity *beam_y, R01sEntity *raster) {
    int i;
    char qn[8];
    char ln[16];
    for (i = 0; i < 8; i++) {
        snprintf(qn, sizeof(qn), "Q%d", i);
        snprintf(ln, sizeof(ln), "%dQ", i + 1);
        pin_net_link(g, beam_y, qn, raster, ln);
    }
}

static void pin_net_link_beam_y_beam(R01sUiPinNet *g, R01sEntity *beam_y, R01sEntity *beam) {
    int i;
    char pn[8];
    char yn[8];
    for (i = 0; i < 8; i++) {
        snprintf(pn, sizeof(pn), "P%d", i);
        snprintf(yn, sizeof(yn), "Y%d", i);
        pin_net_link(g, beam_y, pn, beam, yn);
    }
}

static void pin_net_link_245_internal(R01sUiPinNet *g, R01sEntity *buf) {
    int i;
    char an[8];
    char bn[8];
    for (i = 1; i <= 8; i++) {
        snprintf(an, sizeof(an), "A%d", i);
        snprintf(bn, sizeof(bn), "B%d", i);
        pin_net_link(g, buf, an, buf, bn);
    }
}

static void pin_net_link_245_cpu_side(R01sUiPinNet *g, R01sEntity *cpu, R01sEntity *buf) {
    int i;
    char an[8];
    char bn[8];
    char cn[8];
    for (i = 0; i < 8; i++) {
        snprintf(cn, sizeof(cn), "D%d", i);
        snprintf(an, sizeof(an), "A%d", i + 1);
        snprintf(bn, sizeof(bn), "B%d", i + 1);
        pin_net_link(g, cpu, cn, buf, an);
        pin_net_link(g, cpu, cn, buf, bn);
    }
}

static void pin_net_link_latch_q_vram(R01sUiPinNet *g, R01sEntity *latch, R01sEntity *vram, int addr_base) {
    int i;
    char qn[8];
    char an[8];
    for (i = 0; i < 8; i++) {
        snprintf(qn, sizeof(qn), "%dQ", i + 1);
        snprintf(an, sizeof(an), "A%d", addr_base + i);
        pin_net_link(g, latch, qn, vram, an);
    }
}

static void pin_net_link_latch_le_pld(R01sUiPinNet *g, R01sEntity *pld, R01sEntity *latch, const char *sel) {
    pin_net_link(g, pld, sel, latch, "LE");
}

static int pin_skip_wire(const R01sPin *p) {
    return !p || p->dir == R01S_PIN_NC || p->dir == R01S_PIN_PWR;
}

static int pin_name_eq_dq(const char *d, const char *dq) {
    if (d[0] != 'D' || dq[0] != 'D' || dq[1] != 'Q') {
        return 0;
    }
    return strcmp(d + 1, dq + 2) == 0;
}

static int pin_name_eq_latch_d(const char *ld, const char *d) {
    if (ld[0] < '1' || ld[0] > '8' || ld[1] != 'D' || ld[2] != '\0') {
        return 0;
    }
    if (d[0] != 'D' || d[1] < '0' || d[1] > '7' || d[2] != '\0') {
        return 0;
    }
    return (ld[0] - '1') == (d[1] - '0');
}

static int pin_name_eq_latch_q(const char *lq, const char *d) {
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    if (d[0] != 'D' || d[1] < '0' || d[1] > '7' || d[2] != '\0') {
        return 0;
    }
    return (lq[0] - '1') == (d[1] - '0');
}

static int pin_name_eq_beam_q_latch_q(const char *bq, const char *lq) {
    if (bq[0] != 'Q' || bq[1] < '0' || bq[1] > '7' || bq[2] != '\0') {
        return 0;
    }
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    return (bq[1] - '0') == (lq[0] - '1');
}

static int pin_name_eq_mux_y_a(const char *my, const char *a) {
    if (my[0] < '1' || my[0] > '4' || my[1] != 'Y' || my[2] != '\0') {
        return 0;
    }
    if (a[0] != 'A' || a[2] != '\0' || a[1] < '0' || a[1] > '9') {
        return 0;
    }
    return (my[0] - '1') == (a[1] - '0');
}

static int pin_name_eq_latch_q_a(const char *lq, const char *a) {
    if (lq[0] < '1' || lq[0] > '8' || lq[1] != 'Q' || lq[2] != '\0') {
        return 0;
    }
    if (a[0] != 'A' || a[2] != '\0' || a[1] < '0' || a[1] > '9') {
        return 0;
    }
    return (lq[0] - '1') == (a[1] - '0');
}

static int pin_name_eq_245_ab(const char *a, const char *b) {
    char side_a;
    char side_b;
    int ia;
    int ib;
    if (!a || !b || a[0] != b[0]) {
        return 0;
    }
    side_a = a[0];
    side_b = b[0];
    if ((side_a != 'A' && side_a != 'B') || side_b != side_a) {
        return 0;
    }
    ia = (int)strtol(a + 1, NULL, 10);
    ib = (int)strtol(b + 1, NULL, 10);
    return ia >= 1 && ia <= 8 && ia == ib;
}

static int pin_signals_match(const R01sPin *pa, const R01sPin *pb) {
    const char *na;
    const char *nb;

    if (pin_skip_wire(pa) || pin_skip_wire(pb)) {
        return 0;
    }
    na = pa->name;
    nb = pb->name;
    if (!na || !nb) {
        return 0;
    }
    if (strcmp(na, nb) == 0) {
        return 1;
    }
    if (pin_name_eq_dq(na, nb) || pin_name_eq_dq(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_d(na, nb) || pin_name_eq_latch_d(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_q(na, nb) || pin_name_eq_latch_q(nb, na)) {
        return 1;
    }
    if (pin_name_eq_beam_q_latch_q(na, nb) || pin_name_eq_beam_q_latch_q(nb, na)) {
        return 1;
    }
    if ((na[0] == 'P' && nb[0] == 'Y' && strcmp(na + 1, nb + 1) == 0) ||
        (na[0] == 'Y' && nb[0] == 'P' && strcmp(na + 1, nb + 1) == 0)) {
        return 1;
    }
    if (pin_name_eq_mux_y_a(na, nb) || pin_name_eq_mux_y_a(nb, na)) {
        return 1;
    }
    if (pin_name_eq_latch_q_a(na, nb) || pin_name_eq_latch_q_a(nb, na)) {
        return 1;
    }
    if (pin_name_eq_245_ab(na, nb)) {
        return 1;
    }
    if ((strcmp(na, "IRQB") == 0 && strcmp(nb, "EQ#") == 0) ||
        (strcmp(na, "EQ#") == 0 && strcmp(nb, "IRQB") == 0)) {
        return 1;
    }
    return 0;
}

void r01s_ui_pin_net_build(R01sBoard *board) {
    R01sUiPinNet *g = &g_pin_net;
    R01sEntity *cpu;
    R01sEntity *ram;
    R01sEntity *prg;
    R01sEntity *flash;
    R01sEntity *vram;
    R01sEntity *mcu;
    R01sEntity *apu;
    R01sEntity *pads;
    R01sEntity *beam;
    R01sEntity *beam_y;
    R01sEntity *raster;
    R01sEntity *dot_osc;
    R01sEntity *osc;
    R01sEntity *hc;
    R01sEntity *pld;
    R01sEntity *fe10;
    R01sEntity *fe11;
    R01sEntity *mux_vram;
    R01sEntity *mux_lb;
    R01sEntity *sram_lb;
    R01sEntity *buf_cpu;
    R01sEntity *buf_cart;
    int i;

    memset(g, 0, sizeof(*g));
    if (!board) {
        return;
    }

    cpu = r01s_w65c02s_entity(&board->cpu);
    ram = r01s_as6c62256_entity(&board->ram);
    prg = r01s_prg_rom_entity(&board->prg);
    flash = r01s_sst39sf040_entity(&board->cart_flash);
    vram = r01s_as6c62256_entity(&board->vram);
    mcu = r01s_atmega1284p_entity(&board->mcu1284);
    apu = r01s_atmega328p_entity(&board->apu);
    pads = r01s_pads_entity(&board->pads);
    beam = r01s_beam_xy_entity(&board->pld_beam_x);
    beam_y = r01s_atf22v10_entity(&board->pld_beam_y);
    raster = r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE04]);
    dot_osc = r01s_osc_dot_entity(&board->osc_dot);
    osc = r01s_osc8m_entity(&board->osc);
    hc = r01s_sn74hc14_entity(&board->hc14);
    pld = r01s_atf22v10_entity(&board->pld_decode);
    fe10 = r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE10]);
    fe11 = r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE11]);
    mux_vram = r01s_sn74hc157_entity(board->vram_impl.mux157[R01S_MUX157_VRAM0]);
    mux_lb = r01s_sn74hc157_entity(board->mcu_lb_impl.mux157[R01S_MUX157_LINEBUF0]);
    sram_lb = r01s_as6c62256_entity(&board->linebuf);
    buf_cpu = r01s_sn74hc245_entity(&board->bus245[R01S_BUS245_CPU]);
    buf_cart = r01s_sn74hc245_entity(&board->bus245[R01S_BUS245_CART_OAM]);

    pin_net_link_bus(g, cpu, "A", ram, "A", 16);
    pin_net_link_bus(g, cpu, "D", ram, "DQ", 8);
    pin_net_link_bus(g, cpu, "A", prg, "A", 16);
    pin_net_link_bus(g, cpu, "D", prg, "DQ", 8);
    pin_net_link_bus(g, cpu, "A", vram, "A", 16);
    pin_net_link_bus(g, cpu, "D", flash, "DQ", 8);
    pin_net_link_bus(g, cpu, "D", vram, "DQ", 8);
    pin_net_link_bus(g, cpu, "D", mcu, "DQ", 8);
    pin_net_link_bus(g, cpu, "D", apu, "DQ", 8);
    pin_net_link_bus(g, cpu, "D", pads, "DQ", 8);

    for (i = 0; i < 16; i++) {
        char an[8];
        snprintf(an, sizeof(an), "A%d", i);
        pin_net_link(g, cpu, an, flash, an);
    }

    pin_net_link_bus(g, cpu, "A", pld, "A", 8);
    pin_net_link(g, cpu, "BE", pld, "BE");
    pin_net_link(g, cpu, "RWB", pld, "RWB");

    for (i = 0; i < R01S_BOM_HC573_N; i++) {
        R01sEntity *latch = r01s_sn74hc573_entity(&board->latch573[i]);
        pin_net_link_cpu_d_latch(g, cpu, latch);
    }

    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE02]),
                              "SEL_FE02");
    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE03]),
                              "SEL_FE03");
    pin_net_link_latch_le_pld(g, pld, raster, "SEL_FE04");
    pin_net_link_latch_le_pld(g, pld, fe10, "SEL_FE10");
    pin_net_link_latch_le_pld(g, pld, fe11, "SEL_FE11");
    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE08]),
                              "SEL_FE08");
    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE90]),
                              "SEL_FE90");
    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE91]),
                              "SEL_FE91");
    pin_net_link_latch_le_pld(g, pld, r01s_sn74hc573_entity(board->io_latch_impl.latch573[R01S_LATCH_FE92]),
                              "SEL_FE92");

    pin_net_link_latch_q_vram(g, fe10, vram, 0);
    pin_net_link_latch_q_vram(g, fe11, vram, 8);

    for (i = 0; i < 4; i++) {
        char mn[8];
        char qn[8];
        snprintf(mn, sizeof(mn), "%dA", i + 1);
        snprintf(qn, sizeof(qn), "%dQ", i + 1);
        pin_net_link(g, fe10, qn, mux_vram, mn);
    }

    pin_net_link_245_internal(g, buf_cpu);
    pin_net_link_245_cpu_side(g, cpu, buf_cpu);
    pin_net_link_245_internal(g, buf_cart);
    pin_net_link_245_cpu_side(g, cpu, buf_cart);
    for (i = 0; i < 8; i++) {
        char dq[8];
        char an[8];
        snprintf(dq, sizeof(dq), "DQ%d", i);
        snprintf(an, sizeof(an), "A%d", i + 1);
        pin_net_link(g, flash, dq, buf_cart, an);
    }

    pin_net_link(g, beam, "DOT", dot_osc, "DOT");
    pin_net_link_beam_y_beam(g, beam_y, beam);
    pin_net_link_beam_y_raster(g, beam_y, raster);
    pin_net_link(g, cpu, "IRQB", beam_y, "EQ#");

    pin_net_link(g, vram, "A0", mux_vram, "1Y");
    pin_net_link(g, vram, "A1", mux_vram, "2Y");
    pin_net_link(g, vram, "A2", mux_vram, "3Y");
    pin_net_link(g, vram, "A3", mux_vram, "4Y");

    pin_net_link(g, sram_lb, "A0", mux_lb, "1Y");
    pin_net_link(g, sram_lb, "A1", mux_lb, "2Y");
    pin_net_link(g, sram_lb, "A2", mux_lb, "3Y");
    pin_net_link(g, sram_lb, "A3", mux_lb, "4Y");

    pin_net_link(g, cpu, "PHI2", osc, "PHI2");
    pin_net_link(g, hc, "1A", osc, "PHI2");
    pin_net_link(g, cpu, "PHI2", hc, "1Y");
    pin_net_link(g, hc, "2A", cpu, "RESB");
}

static int ui_chip_index(const R01sUi *ui, const R01sEntity *e) {
    int i;
    if (!ui || !e) {
        return -1;
    }
    for (i = 0; i < ui->chip_count; i++) {
        if (ui->chips[i] == e) {
            return i;
        }
    }
    return -1;
}

static int ui_pin_dip_package(const R01sEntity *e, int pin_index) {
    int num;
    int dip;
    if (!e || pin_index < 0 || pin_index >= e->pin_count) {
        return 0;
    }
    num = e->pins[pin_index].number;
    dip = e->dip_pins > 0 ? e->dip_pins : e->pin_count;
    return num >= 1 && num <= dip;
}

int ui_hit_chip_pin(const R01sUi *ui, int lx, int ly, int *chip_out, int *pin_out) {
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int ci;

    if (chip_out) {
        *chip_out = -1;
    }
    if (pin_out) {
        *pin_out = -1;
    }
    if (!ui || !ui_logic_in_view(lx, ly)) {
        return 0;
    }

    for (ci = ui->chip_count - 1; ci >= 0; ci--) {
        const R01sEntity *e = ui->chips[ci];
        int pi;
        if (!e || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
            continue;
        }
        for (pi = 0; pi < e->pin_count; pi++) {
            int sx;
            int sy;
            int dx;
            int dy;
            int dist;
            if (!ui_pin_dip_package(e, pi)) {
                continue;
            }
            if (!ui_chip_pin_screen_center(ui, e, pi, &sx, &sy)) {
                continue;
            }
            dx = lx - sx;
            dy = ly - sy;
            if (dx < -4 || dx > 4 || dy < -4 || dy > 4) {
                continue;
            }
            dist = dx * dx + dy * dy;
            if (dist < best_dist) {
                best_dist = dist;
                best_chip = ci;
                best_pin = pi;
            }
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (chip_out) {
        *chip_out = best_chip;
    }
    if (pin_out) {
        *pin_out = best_pin;
    }
    return 1;
}

static int ui_pin_net_pick_peer(const R01sUi *ui, const R01sEntity *src, int pin_i, int sx, int sy,
                                int peer_chip, int peer_pin, int *best_chip, int *best_pin, int *best_dist,
                                int prefer_off_chip) {
    const R01sEntity *peer_e;
    int px;
    int py;
    int dx;
    int dy;
    int dist;
    int off_chip;

    if (peer_chip < 0 || peer_pin < 0 || !best_chip || !best_pin || !best_dist) {
        return 0;
    }
    peer_e = ui->chips[peer_chip];
    if (!peer_e || !ui_pin_dip_package(peer_e, peer_pin)) {
        return 0;
    }
    if (peer_e == src && peer_pin == pin_i) {
        return 0;
    }
    if (!ui_chip_pin_screen_center(ui, peer_e, peer_pin, &px, &py)) {
        return 0;
    }
    off_chip = (peer_e != src);
    if (prefer_off_chip && !off_chip) {
        return 0;
    }
    dx = px - sx;
    dy = py - sy;
    dist = dx * dx + dy * dy;
    if (dist < *best_dist) {
        *best_dist = dist;
        *best_chip = peer_chip;
        *best_pin = peer_pin;
        return 1;
    }
    return 0;
}

static int ui_pin_net_peer_union(R01sUiPinNet *g, const R01sUi *ui, const R01sEntity *src, int pin_i,
                                 int src_slot, int sx, int sy, int *peer_chip_out, int *peer_pin_out) {
    int root;
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int pass;
    int i;

    root = pin_net_root(g, src_slot);
    if (root < 0) {
        return 0;
    }

    /* Prefer a peer on a different IC; same-package only if nothing else is wired. */
    for (pass = 0; pass < 2; pass++) {
        int prefer_off_chip = (pass == 0);
        best_chip = -1;
        best_pin = -1;
        best_dist = 999999;
        for (i = 0; i < g->slot_count; i++) {
            const R01sEntity *peer_e;
            int peer_chip;
            int peer_pin;
            if (pin_net_root(g, i) != root) {
                continue;
            }
            if (i == src_slot) {
                continue;
            }
            peer_e = g->slots[i].entity;
            peer_pin = g->slots[i].pin_index;
            if (!peer_e) {
                continue;
            }
            peer_chip = ui_chip_index(ui, peer_e);
            if (peer_chip < 0) {
                continue;
            }
            (void)ui_pin_net_pick_peer(ui, src, pin_i, sx, sy, peer_chip, peer_pin, &best_chip, &best_pin,
                                       &best_dist, prefer_off_chip);
        }
        if (best_chip >= 0) {
            break;
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (peer_chip_out) {
        *peer_chip_out = best_chip;
    }
    if (peer_pin_out) {
        *peer_pin_out = best_pin;
    }
    return 1;
}

static int ui_pin_net_peer_by_name(const R01sUi *ui, const R01sEntity *src, int pin_i, int chip_i, int sx,
                                   int sy, int *peer_chip_out, int *peer_pin_out) {
    const R01sPin *src_pin;
    int best_chip = -1;
    int best_pin = -1;
    int best_dist = 999999;
    int pass;
    int ci;

    src_pin = &src->pins[pin_i];
    if (pin_skip_wire(src_pin)) {
        return 0;
    }

    for (pass = 0; pass < 2; pass++) {
        int prefer_off_chip = (pass == 0);
        best_chip = -1;
        best_pin = -1;
        best_dist = 999999;
        for (ci = 0; ci < ui->chip_count; ci++) {
            const R01sEntity *e = ui->chips[ci];
            int pi;
            if (!e || e->visual != R01S_ENTITY_VIS_IC || ui_chip_hidden(ui, e)) {
                continue;
            }
            for (pi = 0; pi < e->pin_count; pi++) {
                if (ci == chip_i && pi == pin_i) {
                    continue;
                }
                if (!ui_pin_dip_package(e, pi)) {
                    continue;
                }
                if (!pin_signals_match(src_pin, &e->pins[pi])) {
                    continue;
                }
                (void)ui_pin_net_pick_peer(ui, src, pin_i, sx, sy, ci, pi, &best_chip, &best_pin, &best_dist,
                                           prefer_off_chip);
            }
        }
        if (best_chip >= 0) {
            break;
        }
    }

    if (best_chip < 0) {
        return 0;
    }
    if (peer_chip_out) {
        *peer_chip_out = best_chip;
    }
    if (peer_pin_out) {
        *peer_pin_out = best_pin;
    }
    return 1;
}

int ui_pin_net_peer(const R01sUi *ui, int chip_i, int pin_i, int *peer_chip_out, int *peer_pin_out) {
    R01sUiPinNet *g = &g_pin_net;
    const R01sEntity *src;
    const R01sPin *src_pin;
    int src_slot;
    int sx;
    int sy;

    if (peer_chip_out) {
        *peer_chip_out = -1;
    }
    if (peer_pin_out) {
        *peer_pin_out = -1;
    }
    if (!ui || chip_i < 0 || chip_i >= ui->chip_count || pin_i < 0) {
        return 0;
    }
    src = ui->chips[chip_i];
    if (!src || pin_i >= src->pin_count) {
        return 0;
    }
    src_pin = &src->pins[pin_i];
    if (pin_skip_wire(src_pin)) {
        return 0;
    }
    if (!ui_chip_pin_screen_center(ui, src, pin_i, &sx, &sy)) {
        return 0;
    }

    src_slot = pin_net_find_slot(g, (R01sEntity *)(void *)src, pin_i);
    if (src_slot >= 0 &&
        ui_pin_net_peer_union(g, ui, src, pin_i, src_slot, sx, sy, peer_chip_out, peer_pin_out)) {
        return 1;
    }
    return ui_pin_net_peer_by_name(ui, src, pin_i, chip_i, sx, sy, peer_chip_out, peer_pin_out);
}

static void draw_wire_hseg(SDL_Renderer *r, int x0, int x1, int y, Uint8 cr, Uint8 cg, Uint8 cb) {
    int xa;
    int xb;
    if (!r || x0 == x1) {
        return;
    }
    xa = x0 < x1 ? x0 : x1;
    xb = x0 < x1 ? x1 : x0;
    fill_rect(r, xa, y, xb - xa + 1, 1, cr, cg, cb);
}

static void draw_wire_vseg(SDL_Renderer *r, int x, int y0, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    int ya;
    int yb;
    if (!r || y0 == y1) {
        return;
    }
    ya = y0 < y1 ? y0 : y1;
    yb = y0 < y1 ? y1 : y0;
    fill_rect(r, x, ya, 1, yb - ya + 1, cr, cg, cb);
}

static void ui_draw_pin_wire(SDL_Renderer *r, const R01sEntity *e0, int x0, int y0, const R01sEntity *e1,
                             int x1, int y1, Uint8 cr, Uint8 cg, Uint8 cb) {
    int h0;
    int h1;
    int mid_x;
    int mid_y;

    if (!r || !e0 || !e1) {
        return;
    }
    mid_x = (x0 + x1) / 2;
    mid_y = (y0 + y1) / 2;
    h0 = (e0->orient != R01S_ORIENT_V);
    h1 = (e1->orient != R01S_ORIENT_V);

    if (h0 && h1) {
        draw_wire_vseg(r, x0, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, x0, x1, mid_y, cr, cg, cb);
        draw_wire_vseg(r, x1, mid_y, y1, cr, cg, cb);
    } else if (!h0 && !h1) {
        draw_wire_hseg(r, x0, mid_x, y0, cr, cg, cb);
        draw_wire_vseg(r, mid_x, y0, y1, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, y1, cr, cg, cb);
    } else if (h0 && !h1) {
        draw_wire_vseg(r, x0, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, x0, mid_x, mid_y, cr, cg, cb);
        draw_wire_vseg(r, mid_x, mid_y, y1, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, y1, cr, cg, cb);
    } else {
        draw_wire_hseg(r, x0, mid_x, y0, cr, cg, cb);
        draw_wire_vseg(r, mid_x, y0, mid_y, cr, cg, cb);
        draw_wire_hseg(r, mid_x, x1, mid_y, cr, cg, cb);
        draw_wire_vseg(r, x1, mid_y, y1, cr, cg, cb);
    }
}

void ui_draw_pin_wire_overlay(SDL_Renderer *r, R01sUi *ui) {
    int chip_i;
    int pin_i;
    int peer_chip;
    int peer_pin;
    int x0;
    int y0;
    int x1;
    int y1;
    const R01sEntity *src;
    const R01sEntity *peer;
    Uint8 cr;
    Uint8 cg;
    Uint8 cb;

    if (!ui || !r) {
        return;
    }
    if (!ui_hit_chip_pin(ui, ui->mouse_lx, ui->mouse_ly, &chip_i, &pin_i)) {
        return;
    }
    if (!ui_pin_net_peer(ui, chip_i, pin_i, &peer_chip, &peer_pin)) {
        return;
    }
    src = ui->chips[chip_i];
    peer = ui->chips[peer_chip];
    if (!src || !peer) {
        return;
    }
    if (!ui_chip_pin_screen_center(ui, src, pin_i, &x0, &y0)) {
        return;
    }
    if (!ui_chip_pin_screen_center(ui, peer, peer_pin, &x1, &y1)) {
        return;
    }
    ui_chip_pin_rgb(ui, src->pins[pin_i].level, src->pins[pin_i].dir, &cr, &cg, &cb);
    ui_draw_pin_wire(r, src, x0, y0, peer, x1, y1, cr, cg, cb);
}

#include "mobo_io.h"

#include "discrete_ic/pin_header.h"

#include <string.h>

static void io_reset(R01sEntity *e) {
    (void)e;
}

static void io_eval(R01sEntity *e) {
    (void)e;
}

static void io_tick(R01sEntity *e) {
    (void)e;
}

static void io_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable IO_VT = {io_reset, io_eval, io_tick, io_destroy};

static void io_begin(R01sEntity *e, const char *part, const char *refdes) {
    r01s_entity_init(e, &IO_VT, part, refdes);
    e->impl = e;
}

static void io_pin(R01sEntity *e, int number, const char *name) {
    R01sPinDir dir = R01S_PIN_IO;
    if (name && (strcmp(name, "VCC") == 0 || strcmp(name, "VDD") == 0 || strstr(name, "GND") != NULL)) {
        dir = R01S_PIN_PWR;
    }
    r01s_entity_add_pin(e, number, name, dir);
}

static void io_header(R01sEntity *e, int cols, int rows) {
    ns_entity_set_pin_header(e, cols, rows);
    r01s_entity_reset(e);
}

static void io_panel(R01sEntity *e, int w_px, int h_px) {
    r01s_entity_set_glyph(e, R01S_ENTITY_VIS_PANEL, w_px, h_px);
    r01s_entity_reset(e);
}

/* EDAC 2x18. Column 0 is side A, column 1 is side B. Pin number is the contact pair. */
static void init_j36(R01sEntity *e) {
    static const char *const side_a[18] = {
        "GND_A1", "VCC_A", "SDA", "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10", "A11", "A12",
        "A13", "GND_A18",
    };
    static const char *const side_b[18] = {
        "GND_B1", "VCC_B", "SCL", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "OE#", "A14", "A15", "A16", "A17",
        "A18", "WE#",
    };
    int row;
    int n = 1;
    io_begin(e, "EDAC-395", "J36");
    for (row = 0; row < 18; row++) {
        io_pin(e, n++, side_a[row]);
        io_pin(e, n++, side_b[row]);
    }
    io_header(e, 2, 18);
}

static void init_j2(R01sEntity *e) {
    io_begin(e, "HDR-1x6", "J2");
    io_pin(e, 1, "R");
    io_pin(e, 2, "G");
    io_pin(e, 3, "B");
    io_pin(e, 4, "SYNC");
    io_pin(e, 5, "GND1");
    io_pin(e, 6, "GND2");
    io_header(e, 1, 6);
}

/* 2x10 IDC. Odd pins are Player 1, even pins are Player 2. Last two rows are GND. */
static void init_j5(R01sEntity *e) {
    static const char *const p1[8] = {"P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};
    static const char *const p2[8] = {"RIGHT", "LEFT", "DOWN", "UP", "X", "Y", "COIN", "START"};
    int i;
    io_begin(e, "HDR-2x10", "J5");
    for (i = 0; i < 8; i++) {
        io_pin(e, i * 2 + 1, p1[i]);
        io_pin(e, i * 2 + 2, p2[i]);
    }
    io_pin(e, 17, "GND17");
    io_pin(e, 18, "GND18");
    io_pin(e, 19, "GND19");
    io_pin(e, 20, "GND20");
    io_header(e, 2, 10);
}

static void init_j7(R01sEntity *e) {
    io_begin(e, "HDR-1x4", "J7");
    io_pin(e, 1, "VCC");
    io_pin(e, 2, "GND1");
    io_pin(e, 3, "RESB");
    io_pin(e, 4, "GND2");
    io_header(e, 1, 4);
}

static void init_u130(R01sEntity *e) {
    io_begin(e, "MCP130", "U130");
    io_pin(e, 1, "RESET#");
    io_pin(e, 2, "VDD");
    io_pin(e, 3, "VSS");
    io_header(e, 1, 3);
}

static void init_trs(R01sEntity *e, const char *refdes) {
    io_begin(e, "TRS", refdes);
    io_pin(e, 1, "TIP");
    io_pin(e, 2, "RING");
    io_pin(e, 3, "SLEEVE");
    io_panel(e, 48, 32);
}

static void init_named_panel(R01sEntity *e, const char *part, const char *refdes, int w, int h) {
    io_begin(e, part, refdes);
    io_pin(e, 1, "1");
    io_pin(e, 2, "2");
    io_panel(e, w, h);
}

void r01s_mobo_io_init(R01sMoboIo *io) {
    if (!io) {
        return;
    }
    memset(io, 0, sizeof(*io));
    init_named_panel(&io->j1, "DCJ200", "J1", 48, 32);
    init_j2(&io->j2);
    init_trs(&io->j3, "J3");
    init_trs(&io->j4, "J4");
    init_j5(&io->j5);
    init_j7(&io->j7);
    init_j36(&io->j36);
    init_named_panel(&io->sw1, "SW-SLIDE", "SW1", 48, 32);
    init_named_panel(&io->sw_rst, "SW-TACT", "SW_RST", 72, 32);
    init_u130(&io->u130);
}

void r01s_mobo_io_register(R01sMoboIo *io, R01sPinNetlist *nl) {
    R01sEntity *list[10];
    int i;
    if (!io || !nl) {
        return;
    }
    list[0] = &io->j1;
    list[1] = &io->j2;
    list[2] = &io->j3;
    list[3] = &io->j4;
    list[4] = &io->j5;
    list[5] = &io->j7;
    list[6] = &io->j36;
    list[7] = &io->sw1;
    list[8] = &io->sw_rst;
    list[9] = &io->u130;
    for (i = 0; i < 10; i++) {
        r01s_pin_netlist_register_entity(nl, list[i]);
    }
}

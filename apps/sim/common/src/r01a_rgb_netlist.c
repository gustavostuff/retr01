#include "r01a_rgb_netlist.h"

static void link_gun(R01aNetLinkFn link, NsEntity *u24, const char *o_hi, const char *o_mid,
                     const char *o_lo, NsEntity *rh, NsEntity *rm, NsEntity *rl, NsEntity *load,
                     NsEntity *j2, const char *j_pin, NsEntity *y2) {
    link(u24, o_hi, rh, "1");
    link(u24, o_mid, rm, "1");
    link(u24, o_lo, rl, "1");
    link(rh, "2", rm, "2");
    link(rm, "2", rl, "2");
    link(rh, "2", load, "1");
    link(load, "2", y2, "GND");
    link(rh, "2", j2, j_pin);
}

void r01a_netlist_link_dac_rgbs(R01aNetLinkFn link, NsEntity *y2, NsEntity *u24, NsEntity *j2,
                                NsEntity *r1, NsEntity *r2, NsEntity *r3, NsEntity *r4, NsEntity *r5,
                                NsEntity *r6, NsEntity *r7, NsEntity *r8, NsEntity *r9, NsEntity *r10,
                                NsEntity *r11) {
    if (!link || !y2 || !u24 || !j2) {
        return;
    }
    link_gun(link, u24, "O7", "O6", "O5", r1, r2, r3, r9, j2, "R", y2);
    link_gun(link, u24, "O4", "O3", "O2", r4, r5, r6, r10, j2, "G", y2);
    link(u24, "O1", r7, "1");
    link(u24, "O0", r8, "1");
    link(r7, "2", r8, "2");
    link(r7, "2", r11, "1");
    link(r11, "2", y2, "GND");
    link(r7, "2", j2, "B");
}

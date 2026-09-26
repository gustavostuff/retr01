#include "ad724.h"

#include "discrete_ic/bus.h"
#include "test_common.h"

static void drive_encode_pins(NsEntity *e) {
    ns_entity_drive(e, "APOS", NS_LVL_H);
    ns_entity_drive(e, "DPOS", NS_LVL_H);
    ns_entity_drive(e, "ENCD", NS_LVL_H);
    ns_entity_drive(e, "STND", NS_LVL_H);
    ns_entity_drive(e, "SELECT", NS_LVL_L);
}

int main(void) {
    R01aAd724 enc;
    NsEntity *e;

    r01a_ad724_init(&enc, "UENC");
    e = r01a_ad724_entity(&enc);
    drive_encode_pins(e);
    ns_entity_drive(e, "FIN", NS_LVL_L);
    ns_entity_eval(e);
    expect_true(!r01a_ad724_encode_ok(&enc), "no FSC edge yet");

    ns_entity_drive(e, "FIN", NS_LVL_H);
    ns_entity_eval(e);
    expect_true(r01a_ad724_encode_ok(&enc), "encode after FSC edge");
    expect_true(ns_entity_sense(e, "COMP") == NS_LVL_H, "COMP high when encoding");

    ns_entity_drive(e, "ENCD", NS_LVL_L);
    ns_entity_eval(e);
    expect_true(!r01a_ad724_encode_ok(&enc), "ENCD low gates encode");
    expect_true(ns_entity_sense(e, "COMP") == NS_LVL_L, "COMP low when gated");

    return test_done("test_ad724");
}

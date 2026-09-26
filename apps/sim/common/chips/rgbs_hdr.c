#include "rgbs_hdr.h"

#include "discrete_ic/pin_header.h"

#include <string.h>

static void rgbs_hdr_reset(NsEntity *e) {
    (void)e;
}

static void rgbs_hdr_eval(NsEntity *e) {
    (void)e;
}

static void rgbs_hdr_tick(NsEntity *e) {
    (void)e;
}

static void rgbs_hdr_destroy(NsEntity *e) {
    (void)e;
}

static const NsEntityVTable RGBS_HDR_VT = {rgbs_hdr_reset, rgbs_hdr_eval, rgbs_hdr_tick, rgbs_hdr_destroy};

void r01a_rgbs_hdr_init(R01aRgbsHdr *hdr, const char *refdes) {
    if (!hdr) {
        return;
    }
    memset(hdr, 0, sizeof(*hdr));
    ns_entity_init(&hdr->base, &RGBS_HDR_VT, "RGBS_HDR", refdes ? refdes : "J2");
    hdr->base.impl = hdr;
    ns_entity_add_pin(&hdr->base, 1, "R", NS_PIN_IN);
    ns_entity_add_pin(&hdr->base, 2, "G", NS_PIN_IN);
    ns_entity_add_pin(&hdr->base, 3, "B", NS_PIN_IN);
    ns_entity_add_pin(&hdr->base, 4, "CSYNC", NS_PIN_IN);
    ns_entity_add_pin(&hdr->base, 5, "GND", NS_PIN_PWR);
    ns_entity_add_pin(&hdr->base, 6, "GND2", NS_PIN_PWR);
    ns_entity_set_pin_header(&hdr->base, 6, 1);
    ns_entity_reset(&hdr->base);
}

NsEntity *r01a_rgbs_hdr_entity(R01aRgbsHdr *hdr) {
    return hdr ? &hdr->base : NULL;
}

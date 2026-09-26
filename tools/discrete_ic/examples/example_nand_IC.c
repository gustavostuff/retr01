#include "discrete_ic/bus.h"
#include "discrete_ic/chip.h"

#include <stdio.h>

static void nand_eval(NsEntity *e) {
    NsLevel a = ns_entity_sense(e, "1A");
    NsLevel b = ns_entity_sense(e, "1B");
    int ah = (a == NS_LVL_H || a == NS_LVL_Z);
    int bh = (b == NS_LVL_H || b == NS_LVL_Z);
    ns_entity_drive(e, "1Y", (ah && bh) ? NS_LVL_L : NS_LVL_H);
}

static const NsChipPinDef k_pins[] = {
    {1, "1A", NS_PIN_IN},
    {2, "1B", NS_PIN_IN},
    {3, "1Y", NS_PIN_OUT},
    {4, "2A", NS_PIN_IN},
    {5, "2B", NS_PIN_IN},
    {6, "2Y", NS_PIN_OUT},
    {7, "GND", NS_PIN_PWR},
    {14, "VCC", NS_PIN_PWR},
};

void example_nand_register(void) {
    NsChipClass cls = {0};
    cls.part = "SN74HC00";
    cls.dip_pins = 14;
    cls.pins = k_pins;
    cls.pin_count = (int)(sizeof(k_pins) / sizeof(k_pins[0]));
    cls.eval = nand_eval;
    if (ns_chip_register(&cls) != 0) {
        fprintf(stderr, "example_nand_register failed\n");
    }
}

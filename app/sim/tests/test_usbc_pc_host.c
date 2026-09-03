#include "atmega32u4.h"
#include "pc_host.h"
#include "r01_flash_proto.h"
#include "sst39sf040.h"
#include "usbc_receptacle.h"
#include "test_common.h"

#include <string.h>

int main(void) {
    R01sPcHost pc;
    R01sUsbcReceptacle usb;
    R01sAtmega32u4 mcu;
    R01sSn74hc595 lo;
    R01sSn74hc595 hi;
    R01sSst39sf040 flash;
    static uint8_t rom[64];
    int i;

    for (i = 0; i < (int)sizeof(rom); i++) {
        rom[i] = (uint8_t)(0xA0u + (unsigned)i);
    }

    r01s_sst39sf040_init(&flash, "U40");
    r01s_sn74hc595_init(&lo, "U595A");
    r01s_sn74hc595_init(&hi, "U595B");
    r01s_usbc_receptacle_init(&usb, "J1");
    r01s_atmega32u4_init(&mcu, "U32");
    r01s_pc_host_init(&pc, "PC1");
    r01s_pc_host_bind_usb(&pc, &usb);
    r01s_atmega32u4_bind_usb(&mcu, &usb);
    r01s_atmega32u4_bind_shifts(&mcu, &lo, &hi);
    r01s_atmega32u4_bind_flash(&mcu, &flash, 1);
    r01s_usbc_set_vbus(&usb, 1);

    pc.rom = rom;
    pc.rom_len = sizeof(rom);
    pc.rom_pos = 0;

    expect_true(r01s_pc_host_start_stream(&pc), "arm stream");
    expect_true(r01s_usbc_cc_sink_ok(&usb), "vbus on");

    for (i = 0; i < 500000; i++) {
        r01s_atmega32u4_service(&mcu, 65536);
        r01s_pc_host_stream_tick(&pc, 4096);
        if (r01s_pc_host_stream_done(&pc) && !r01s_usbc_host_pending(&usb) && !r01s_atmega32u4_busy(&mcu) &&
            r01s_atmega32u4_bytes_programmed(&mcu) >= pc.rom_len) {
            break;
        }
    }

    expect_true(r01s_atmega32u4_bytes_programmed(&mcu) == pc.rom_len, "all bytes programmed");
    expect_true(r01s_sst39sf040_peek(&flash, 0) == rom[0], "first rom byte in flash");
    expect_true(r01s_sst39sf040_peek(&flash, (uint32_t)(sizeof(rom) - 1u)) == rom[sizeof(rom) - 1u], "last byte");

    pc.rom = NULL;
    pc.rom_len = 0;
    return test_done("test_usbc_pc_host");
}

#include "atmega32u4.h"
#include "r01_flash_proto.h"
#include "sst39sf040.h"
#include "usbc_receptacle.h"
#include "test_common.h"

static void send_frame(R01sUsbcReceptacle *usb, uint8_t cmd, const uint8_t *payload, uint16_t len) {
    int i;
    r01s_usbc_host_send_byte(usb, R01F_MAGIC0);
    r01s_usbc_host_send_byte(usb, R01F_MAGIC1);
    r01s_usbc_host_send_byte(usb, cmd);
    r01s_usbc_host_send_byte(usb, (uint8_t)(len & 0xFFu));
    r01s_usbc_host_send_byte(usb, (uint8_t)(len >> 8));
    r01s_usbc_host_send_byte(usb, 0);
    for (i = 0; i < len; i++) {
        r01s_usbc_host_send_byte(usb, payload[i]);
    }
}

int main(void) {
    R01sAtmega32u4 mcu;
    R01sUsbcReceptacle usb;
    R01sSst39sf040 flash;
    R01sSn74hc595 lo;
    R01sSn74hc595 hi;
    uint8_t byte = 0xAB;

    r01s_sst39sf040_init(&flash, "U40");
    r01s_sn74hc595_init(&lo, "U595A");
    r01s_sn74hc595_init(&hi, "U595B");
    r01s_usbc_receptacle_init(&usb, "J1");
    r01s_atmega32u4_init(&mcu, "U32");
    expect_true(mcu.base.dip_pins == 32, "DIP-32 package");
    r01s_atmega32u4_bind_usb(&mcu, &usb);
    r01s_atmega32u4_bind_shifts(&mcu, &lo, &hi);
    r01s_atmega32u4_bind_flash(&mcu, &flash, 1);
    r01s_usbc_set_vbus(&usb, 1);

    send_frame(&usb, R01F_CMD_ERASE, NULL, 0);
    expect_true(r01s_atmega32u4_service(&mcu, 64) > 0, "erase frame");
    send_frame(&usb, R01F_CMD_DATA, &byte, 1);
    expect_true(r01s_atmega32u4_service(&mcu, 4096) > 0, "program one byte");
    expect_true(r01s_sst39sf040_peek(&flash, 0) == 0xAB, "flash byte 0 via JEDEC");

    r01s_atmega32u4_bind_flash(&mcu, &flash, 0);
    send_frame(&usb, R01F_CMD_DATA, &byte, 1);
    r01s_atmega32u4_service(&mcu, 64);
    expect_true(r01s_atmega32u4_last_error(&mcu) == R01S_32U4_ERR_NO_CART, "no cart error");

    return test_done("test_atmega32u4");
}

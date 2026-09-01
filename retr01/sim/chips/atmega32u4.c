#include "atmega32u4.h"

#include "r01_flash_proto.h"
#include "retr01_sim/bus.h"
#include "sst39sf040.h"

#include <string.h>

static void mcu_reply(R01sAtmega32u4 *chip, uint8_t status) {
    if (chip && chip->usb) {
        r01s_usbc_device_send_byte(chip->usb, status);
    }
}

static void mcu_set_addr(R01sAtmega32u4 *chip, uint32_t addr) {
    r01s_flasher_bus_spi_shift(&chip->bus, (uint8_t)(addr >> 8));
    r01s_flasher_bus_spi_shift(&chip->bus, (uint8_t)(addr & 0xFFu));
    r01s_flasher_bus_latch_addr(&chip->bus);
    r01s_flasher_bus_set_addr_ext(&chip->bus, addr);
    chip->prog_addr = addr;
}

static int mcu_program_byte(R01sAtmega32u4 *chip, uint8_t data) {
    if (!chip->target_flash) {
        return -1;
    }
    mcu_set_addr(chip, chip->prog_addr);
    r01s_flasher_bus_jedec_program(&chip->bus, chip->prog_addr, data);
    chip->prog_addr++;
    chip->bytes_programmed++;
    return 0;
}

static int mcu_payload_tick(R01sAtmega32u4 *chip, int budget) {
    int n = 0;
    while (n < budget && chip->payload_pos < chip->frame_len) {
        if (mcu_program_byte(chip, chip->payload[chip->payload_pos]) != 0) {
            chip->last_error = R01S_32U4_ERR_NO_CART;
            mcu_reply(chip, R01F_ST_NO_CART);
            chip->fw_state = R01S_32U4_FW_IDLE;
            return n;
        }
        chip->payload_pos++;
        n++;
    }
    if (chip->payload_pos >= chip->frame_len) {
        mcu_reply(chip, R01F_ST_OK);
        chip->fw_state = R01S_32U4_FW_IDLE;
    }
    return n;
}

static int mcu_finish_frame(R01sAtmega32u4 *chip) {
    if (!chip->cart_present || !chip->target_flash) {
        chip->last_error = R01S_32U4_ERR_NO_CART;
        mcu_reply(chip, R01F_ST_NO_CART);
        return 1;
    }

    switch (chip->frame_cmd) {
    case R01F_CMD_ERASE:
        r01s_flasher_bus_jedec_chip_erase(&chip->bus);
        chip->prog_addr = 0;
        chip->bytes_programmed = 0;
        mcu_reply(chip, R01F_ST_OK);
        chip->fw_state = R01S_32U4_FW_IDLE;
        return 1;
    case R01F_CMD_DATA:
        chip->payload_pos = 0;
        chip->fw_state = R01S_32U4_FW_PAYLOAD;
        return 0;
    case R01F_CMD_DONE:
        mcu_reply(chip, R01F_ST_OK);
        chip->fw_state = R01S_32U4_FW_IDLE;
        return 1;
    default:
        chip->last_error = R01S_32U4_ERR_PROTO;
        mcu_reply(chip, R01F_ST_ERR);
        chip->fw_state = R01S_32U4_FW_IDLE;
        return 1;
    }
}

void r01s_atmega32u4_reset_program(R01sAtmega32u4 *chip) {
    if (!chip) {
        return;
    }
    chip->prog_addr = 0;
    chip->bytes_programmed = 0;
    chip->last_error = R01S_32U4_ERR_NONE;
    chip->fw_state = R01S_32U4_FW_IDLE;
    chip->hdr_pos = 0;
    chip->payload_pos = 0;
    chip->erase_done = 0;
}

static void mcu_reset(R01sEntity *e) {
    R01sAtmega32u4 *c = (R01sAtmega32u4 *)e;
    r01s_atmega32u4_reset_program(c);
    r01s_entity_drive(e, "WE#", R01S_LVL_H);
    r01s_entity_drive(e, "OE#", R01S_LVL_H);
}

static void mcu_eval(R01sEntity *e) {
    (void)e;
}

static void mcu_tick(R01sEntity *e) {
    (void)r01s_atmega32u4_service((R01sAtmega32u4 *)e, 64);
}

static void mcu_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable MCU32U4_VT = {mcu_reset, mcu_eval, mcu_tick, mcu_destroy};

void r01s_atmega32u4_init(R01sAtmega32u4 *chip, const char *refdes) {
    if (!chip) {
        return;
    }
    memset(chip, 0, sizeof(*chip));
    r01s_entity_init(&chip->base, &MCU32U4_VT, "ATMEGA32U4", refdes ? refdes : "U1");
    chip->base.impl = chip;
    chip->cart_present = 0;

    r01s_entity_add_pin(&chip->base, 1, "RESET#", R01S_PIN_IN);
    r01s_entity_add_pin(&chip->base, 2, "MOSI", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 3, "SCK", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 4, "SS", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 5, "WE#", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 6, "OE#", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 7, "UD+", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 8, "UD-", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 9, "A16", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 10, "A17", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 11, "A18", R01S_PIN_OUT);
    r01s_entity_add_pin(&chip->base, 12, "D0", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 13, "D1", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 14, "D2", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 15, "D3", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 16, "D4", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 17, "D5", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 18, "D6", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 19, "D7", R01S_PIN_IO);
    r01s_entity_add_pin(&chip->base, 20, "VCC", R01S_PIN_PWR);
    r01s_entity_add_pin(&chip->base, 21, "GND", R01S_PIN_PWR);
    r01s_entity_set_dip_mm(&chip->base, 32, 42, 14);
    r01s_entity_reset(&chip->base);
}

R01sEntity *r01s_atmega32u4_entity(R01sAtmega32u4 *chip) {
    return chip ? &chip->base : NULL;
}

void r01s_atmega32u4_bind_usb(R01sAtmega32u4 *chip, R01sUsbcReceptacle *usb) {
    if (chip) {
        chip->usb = usb;
    }
}

void r01s_atmega32u4_bind_shifts(R01sAtmega32u4 *chip, R01sSn74hc595 *lo, R01sSn74hc595 *hi) {
    if (chip) {
        r01s_flasher_bus_init(&chip->bus, chip, lo, hi, chip->target_flash);
    }
}

void r01s_atmega32u4_bind_flash(R01sAtmega32u4 *chip, R01sSst39sf040 *flash, int cart_present) {
    if (chip) {
        chip->target_flash = flash;
        chip->cart_present = cart_present ? 1 : 0;
        chip->bus.flash = flash;
    }
}

int r01s_atmega32u4_service(R01sAtmega32u4 *chip, int budget) {
    uint8_t b;
    int n = 0;

    if (!chip || !chip->usb || budget <= 0) {
        return 0;
    }
    if (chip->fw_state == R01S_32U4_FW_PAYLOAD) {
        return mcu_payload_tick(chip, budget);
    }
    if (!r01s_usbc_cc_sink_ok(chip->usb)) {
        chip->last_error = R01S_32U4_ERR_NO_VBUS;
        return 0;
    }

    while (n < budget && r01s_usbc_device_recv_byte(chip->usb, &b)) {
        if (chip->fw_state == R01S_32U4_FW_IDLE) {
            if (chip->hdr_pos == 0 && b != R01F_MAGIC0) {
                chip->last_error = R01S_32U4_ERR_PROTO;
                continue;
            }
            chip->hdr[chip->hdr_pos++] = b;
            if (chip->hdr_pos < R01F_HDR_LEN) {
                n++;
                continue;
            }
            if (chip->hdr[1] != R01F_MAGIC1) {
                chip->last_error = R01S_32U4_ERR_PROTO;
                chip->hdr_pos = 0;
                n++;
                continue;
            }
            chip->frame_cmd = chip->hdr[2];
            chip->frame_len = (uint16_t)chip->hdr[3] | ((uint16_t)chip->hdr[4] << 8);
            chip->frame_seq = chip->hdr[5];
            chip->hdr_pos = 0;
            if (chip->frame_len > R01F_MAX_DATA) {
                chip->last_error = R01S_32U4_ERR_PROTO;
                mcu_reply(chip, R01F_ST_ERR);
                n++;
                continue;
            }
            if (chip->frame_len > 0) {
                chip->fw_state = R01S_32U4_FW_HDR;
                chip->payload_pos = 0;
            } else {
                (void)mcu_finish_frame(chip);
            }
            n++;
            continue;
        }
        if (chip->fw_state == R01S_32U4_FW_HDR) {
            chip->payload[chip->payload_pos++] = b;
            if (chip->payload_pos >= chip->frame_len) {
                (void)mcu_finish_frame(chip);
            }
            n++;
        }
    }
    if (chip->fw_state == R01S_32U4_FW_PAYLOAD && n < budget) {
        n += mcu_payload_tick(chip, budget - n);
    }
    return n;
}

uint8_t r01s_atmega32u4_last_error(const R01sAtmega32u4 *chip) {
    return chip ? chip->last_error : R01S_32U4_ERR_NONE;
}

uint32_t r01s_atmega32u4_bytes_programmed(const R01sAtmega32u4 *chip) {
    return chip ? chip->bytes_programmed : 0;
}

int r01s_atmega32u4_busy(const R01sAtmega32u4 *chip) {
    return chip && chip->fw_state != R01S_32U4_FW_IDLE;
}

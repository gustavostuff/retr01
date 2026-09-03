#ifndef retr01_SIM_ATMEGA32U4_H
#define retr01_SIM_ATMEGA32U4_H

#include "flasher_bus.h"
#include "r01_flash_proto.h"
#include "retr01_sim/entity.h"
#include "usbc_receptacle.h"

#include <stdint.h>

struct R01sSst39sf040;

typedef enum R01s32u4FwState {
    R01S_32U4_FW_IDLE = 0,
    R01S_32U4_FW_HDR,
    R01S_32U4_FW_PAYLOAD,
} R01s32u4FwState;

/*
 * Cart flasher MCU (docs/cart.md): USB CDC bulk -> SPI/595 address + JEDEC program.
 */
typedef struct R01sAtmega32u4 {
    R01sEntity base;
    R01sUsbcReceptacle *usb;
    R01sFlasherBus bus;
    struct R01sSst39sf040 *target_flash;
    int cart_present;
    uint32_t prog_addr;
    uint32_t bytes_programmed;
    uint8_t last_error;
    R01s32u4FwState fw_state;
    uint8_t hdr[R01F_HDR_LEN];
    uint8_t hdr_pos;
    uint8_t frame_cmd;
    uint16_t frame_len;
    uint8_t frame_seq;
    uint16_t payload_pos;
    uint8_t payload[R01F_MAX_DATA];
    int erase_done;
} R01sAtmega32u4;

#define R01S_32U4_ERR_NONE 0u
#define R01S_32U4_ERR_NO_CART 1u
#define R01S_32U4_ERR_NO_VBUS 2u
#define R01S_32U4_ERR_PROTO 3u

void r01s_atmega32u4_init(R01sAtmega32u4 *chip, const char *refdes);
R01sEntity *r01s_atmega32u4_entity(R01sAtmega32u4 *chip);

void r01s_atmega32u4_bind_usb(R01sAtmega32u4 *chip, R01sUsbcReceptacle *usb);
void r01s_atmega32u4_bind_shifts(R01sAtmega32u4 *chip, R01sSn74hc595 *lo, R01sSn74hc595 *hi);
void r01s_atmega32u4_bind_flash(R01sAtmega32u4 *chip, struct R01sSst39sf040 *flash, int cart_present);
void r01s_atmega32u4_reset_program(R01sAtmega32u4 *chip);

/* Pump USB RX; returns work units handled (bytes/frames). Budget limits per call. */
int r01s_atmega32u4_service(R01sAtmega32u4 *chip, int budget);

uint8_t r01s_atmega32u4_last_error(const R01sAtmega32u4 *chip);
uint32_t r01s_atmega32u4_bytes_programmed(const R01sAtmega32u4 *chip);
int r01s_atmega32u4_busy(const R01sAtmega32u4 *chip);

#endif

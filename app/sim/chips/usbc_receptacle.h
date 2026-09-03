#ifndef retr01_SIM_USBC_RECEPTACLE_H
#define retr01_SIM_USBC_RECEPTACLE_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * USB-C receptacle (docs/cart.md flasher). CC1/CC2 sink pull-downs modeled internally.
 * DP/DM connect host <-> device (flasher MCU).
 */
typedef struct R01sUsbcReceptacle {
    R01sEntity base;
    int vbus_on;
    uint8_t host_to_dev[4096];
    int h2d_head;
    int h2d_tail;
    uint8_t dev_to_host[256];
    int d2h_head;
    int d2h_tail;
} R01sUsbcReceptacle;

void r01s_usbc_receptacle_init(R01sUsbcReceptacle *port, const char *refdes);
R01sEntity *r01s_usbc_receptacle_entity(R01sUsbcReceptacle *port);

void r01s_usbc_host_send_byte(R01sUsbcReceptacle *port, uint8_t b);
int r01s_usbc_device_recv_byte(R01sUsbcReceptacle *port, uint8_t *out);
void r01s_usbc_device_send_byte(R01sUsbcReceptacle *port, uint8_t b);
int r01s_usbc_host_recv_byte(R01sUsbcReceptacle *port, uint8_t *out);
int r01s_usbc_host_pending(const R01sUsbcReceptacle *port);

void r01s_usbc_set_vbus(R01sUsbcReceptacle *port, int on);
int r01s_usbc_cc_sink_ok(const R01sUsbcReceptacle *port);

#endif

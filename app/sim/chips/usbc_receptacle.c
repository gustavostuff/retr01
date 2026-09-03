#include "usbc_receptacle.h"

#include "retr01_sim/bus.h"

#include <string.h>

#define H2D_CAP ((int)sizeof(((R01sUsbcReceptacle *)0)->host_to_dev))
#define D2H_CAP ((int)sizeof(((R01sUsbcReceptacle *)0)->dev_to_host))

static void usbc_reset(R01sEntity *e) {
    R01sUsbcReceptacle *p = (R01sUsbcReceptacle *)e;
    p->vbus_on = 0;
    p->h2d_head = 0;
    p->h2d_tail = 0;
    p->d2h_head = 0;
    p->d2h_tail = 0;
    r01s_entity_drive(e, "VBUS", R01S_LVL_L);
    r01s_entity_drive(e, "CC1", R01S_LVL_L);
    r01s_entity_drive(e, "CC2", R01S_LVL_L);
    r01s_entity_drive(e, "DP", R01S_LVL_L);
    r01s_entity_drive(e, "DM", R01S_LVL_L);
}

static void usbc_eval(R01sEntity *e) {
    R01sUsbcReceptacle *p = (R01sUsbcReceptacle *)e;
    if (p->vbus_on) {
        r01s_entity_drive(e, "CC1", R01S_LVL_L);
        r01s_entity_drive(e, "CC2", R01S_LVL_L);
        r01s_entity_drive(e, "VBUS", R01S_LVL_H);
    } else {
        r01s_entity_drive(e, "VBUS", R01S_LVL_L);
    }
    r01s_entity_drive(e, "DP", R01S_LVL_L);
    r01s_entity_drive(e, "DM", R01S_LVL_L);
}

static void usbc_tick(R01sEntity *e) {
    (void)e;
}

static void usbc_destroy(R01sEntity *e) {
    (void)e;
}

static const R01sEntityVTable USBC_VT = {usbc_reset, usbc_eval, usbc_tick, usbc_destroy};

void r01s_usbc_receptacle_init(R01sUsbcReceptacle *port, const char *refdes) {
    if (!port) {
        return;
    }
    memset(port, 0, sizeof(*port));
    r01s_entity_init(&port->base, &USBC_VT, "USB-C", refdes ? refdes : "J1");
    port->base.impl = port;
    port->base.visual = R01S_ENTITY_VIS_PANEL;
    r01s_entity_set_glyph(&port->base, R01S_ENTITY_VIS_PANEL, 48, 24);

    r01s_entity_add_pin(&port->base, 1, "VBUS", R01S_PIN_OUT);
    r01s_entity_add_pin(&port->base, 2, "GND", R01S_PIN_PWR);
    r01s_entity_add_pin(&port->base, 3, "CC1", R01S_PIN_OUT);
    r01s_entity_add_pin(&port->base, 4, "CC2", R01S_PIN_OUT);
    r01s_entity_add_pin(&port->base, 5, "DP", R01S_PIN_IO);
    r01s_entity_add_pin(&port->base, 6, "DM", R01S_PIN_IO);
    r01s_entity_reset(&port->base);
}

R01sEntity *r01s_usbc_receptacle_entity(R01sUsbcReceptacle *port) {
    return port ? &port->base : NULL;
}

void r01s_usbc_set_vbus(R01sUsbcReceptacle *port, int on) {
    if (port) {
        port->vbus_on = on ? 1 : 0;
        r01s_entity_eval(&port->base);
    }
}

int r01s_usbc_cc_sink_ok(const R01sUsbcReceptacle *port) {
    return port && port->vbus_on;
}

void r01s_usbc_host_send_byte(R01sUsbcReceptacle *port, uint8_t b) {
    int next;
    if (!port) {
        return;
    }
    next = (port->h2d_tail + 1) % H2D_CAP;
    if (next == port->h2d_head) {
        return;
    }
    port->host_to_dev[port->h2d_tail] = b;
    port->h2d_tail = next;
}

int r01s_usbc_device_recv_byte(R01sUsbcReceptacle *port, uint8_t *out) {
    if (!port || !out || port->h2d_head == port->h2d_tail) {
        return 0;
    }
    *out = port->host_to_dev[port->h2d_head];
    port->h2d_head = (port->h2d_head + 1) % H2D_CAP;
    return 1;
}

void r01s_usbc_device_send_byte(R01sUsbcReceptacle *port, uint8_t b) {
    int next;
    if (!port) {
        return;
    }
    next = (port->d2h_tail + 1) % D2H_CAP;
    if (next == port->d2h_head) {
        return;
    }
    port->dev_to_host[port->d2h_tail] = b;
    port->d2h_tail = next;
}

int r01s_usbc_host_recv_byte(R01sUsbcReceptacle *port, uint8_t *out) {
    if (!port || !out || port->d2h_head == port->d2h_tail) {
        return 0;
    }
    *out = port->dev_to_host[port->d2h_head];
    port->d2h_head = (port->d2h_head + 1) % D2H_CAP;
    return 1;
}

int r01s_usbc_host_pending(const R01sUsbcReceptacle *port) {
    return port && port->h2d_head != port->h2d_tail;
}

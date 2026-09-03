#include "pc_host.h"

#include "r01_flash_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    PC_PHASE_IDLE = 0,
    PC_PHASE_ERASE,
    PC_PHASE_DATA,
    PC_PHASE_DONE,
};

static void pc_queue_frame(R01sPcHost *host, uint8_t cmd, const uint8_t *payload, uint16_t len) {
    uint16_t i;
    if (!host) {
        return;
    }
    host->tx[0] = R01F_MAGIC0;
    host->tx[1] = R01F_MAGIC1;
    host->tx[2] = cmd;
    host->tx[3] = (uint8_t)(len & 0xFFu);
    host->tx[4] = (uint8_t)(len >> 8);
    host->tx[5] = host->frame_seq++;
    for (i = 0; i < len; i++) {
        host->tx[R01F_HDR_LEN + i] = payload ? payload[i] : 0;
    }
    host->tx_len = (int)(R01F_HDR_LEN + len);
    host->tx_pos = 0;
}

static void pc_advance_phase(R01sPcHost *host) {
    if (!host) {
        return;
    }
    if (host->phase == PC_PHASE_ERASE) {
        host->phase = PC_PHASE_DATA;
        if (host->rom_pos >= host->rom_len) {
            host->phase = PC_PHASE_DONE;
            pc_queue_frame(host, R01F_CMD_DONE, NULL, 0);
            return;
        }
        {
            uint16_t chunk = (uint16_t)(host->rom_len - host->rom_pos);
            if (chunk > R01F_MAX_DATA) {
                chunk = R01F_MAX_DATA;
            }
            pc_queue_frame(host, R01F_CMD_DATA, host->rom + host->rom_pos, chunk);
            host->rom_pos += chunk;
        }
        return;
    }
    if (host->phase == PC_PHASE_DATA) {
        if (host->rom_pos < host->rom_len) {
            uint16_t chunk = (uint16_t)(host->rom_len - host->rom_pos);
            if (chunk > R01F_MAX_DATA) {
                chunk = R01F_MAX_DATA;
            }
            pc_queue_frame(host, R01F_CMD_DATA, host->rom + host->rom_pos, chunk);
            host->rom_pos += chunk;
        } else {
            host->phase = PC_PHASE_DONE;
            pc_queue_frame(host, R01F_CMD_DONE, NULL, 0);
        }
    }
}

static void pc_reset(R01sEntity *e) {
    R01sPcHost *h = (R01sPcHost *)e;
    h->rom_pos = 0;
    h->streaming = 0;
    h->stream_done = 0;
    h->phase = PC_PHASE_IDLE;
    h->tx_pos = 0;
    h->tx_len = 0;
    h->wait_ack = 0;
}

static void pc_eval(R01sEntity *e) {
    (void)e;
}

static void pc_tick(R01sEntity *e) {
    (void)r01s_pc_host_stream_tick((R01sPcHost *)e, 64);
}

static void pc_destroy(R01sEntity *e) {
    R01sPcHost *h = (R01sPcHost *)e;
    r01s_pc_host_free_rom(h);
}

static const R01sEntityVTable PC_HOST_VT = {pc_reset, pc_eval, pc_tick, pc_destroy};

void r01s_pc_host_init(R01sPcHost *host, const char *refdes) {
    if (!host) {
        return;
    }
    memset(host, 0, sizeof(*host));
    r01s_entity_init(&host->base, &PC_HOST_VT, "PC-HOST", refdes ? refdes : "PC1");
    host->base.impl = host;
    host->base.visual = R01S_ENTITY_VIS_PANEL;
    r01s_entity_set_glyph(&host->base, R01S_ENTITY_VIS_PANEL, 64, 40);
    r01s_entity_reset(&host->base);
}

R01sEntity *r01s_pc_host_entity(R01sPcHost *host) {
    return host ? &host->base : NULL;
}

void r01s_pc_host_bind_usb(R01sPcHost *host, R01sUsbcReceptacle *usb) {
    if (host) {
        host->usb = usb;
    }
}

void r01s_pc_host_free_rom(R01sPcHost *host) {
    if (!host) {
        return;
    }
    free(host->rom);
    host->rom = NULL;
    host->rom_len = 0;
    host->rom_pos = 0;
}

int r01s_pc_host_load_file(R01sPcHost *host, const char *path) {
    FILE *f;
    long sz;

    if (!host || !path) {
        return -1;
    }
    r01s_pc_host_free_rom(host);
    f = fopen(path, "rb");
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    host->rom = (uint8_t *)malloc((size_t)sz);
    if (!host->rom) {
        fclose(f);
        return -1;
    }
    if (fread(host->rom, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        r01s_pc_host_free_rom(host);
        return -1;
    }
    fclose(f);
    host->rom_len = (size_t)sz;
    host->rom_pos = 0;
    host->stream_done = 0;
    return 0;
}

int r01s_pc_host_start_stream(R01sPcHost *host) {
    if (!host || !host->rom || host->rom_len == 0) {
        return 0;
    }
    host->rom_pos = 0;
    host->streaming = 1;
    host->stream_done = 0;
    host->wait_ack = 0;
    host->phase = PC_PHASE_ERASE;
    host->frame_seq = 0;
    pc_queue_frame(host, R01F_CMD_ERASE, NULL, 0);
    if (host->usb) {
        r01s_usbc_set_vbus(host->usb, 1);
    }
    return 1;
}

int r01s_pc_host_stream_tick(R01sPcHost *host, int max_bytes) {
    int n = 0;
    if (!host || !host->streaming || !host->usb) {
        return 0;
    }

    if (host->wait_ack) {
        uint8_t st;
        if (!r01s_usbc_host_recv_byte(host->usb, &st)) {
            return 0;
        }
        (void)st;
        host->wait_ack = 0;
        if (host->phase == PC_PHASE_DONE) {
            host->streaming = 0;
            host->stream_done = 1;
            host->phase = PC_PHASE_IDLE;
            return 0;
        }
        pc_advance_phase(host);
    }

    while (n < max_bytes && host->tx_pos < host->tx_len) {
        r01s_usbc_host_send_byte(host->usb, host->tx[host->tx_pos++]);
        n++;
    }

    if (host->tx_pos < host->tx_len) {
        return n;
    }

    if (host->tx_len > 0) {
        host->wait_ack = 1;
        host->tx_len = 0;
        host->tx_pos = 0;
    }

    return n;
}

int r01s_pc_host_stream_done(const R01sPcHost *host) {
    return host && host->stream_done;
}

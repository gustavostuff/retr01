#ifndef retr01_SIM_PC_HOST_H
#define retr01_SIM_PC_HOST_H

#include "r01_flash_proto.h"
#include "retr01_sim/entity.h"
#include "usbc_receptacle.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Virtual PC + ROM file source for the flasher bench (not silicon).
 * "Flash ROM" button arms a stream of cart image bytes into USB-C.
 */
typedef struct R01sPcHost {
    R01sEntity base;
    R01sUsbcReceptacle *usb;
    uint8_t *rom;
    size_t rom_len;
    size_t rom_pos;
    int streaming;
    int stream_done;
    int phase;
    uint8_t frame_seq;
    int wait_ack;
    uint8_t tx[R01F_HDR_LEN + R01F_MAX_DATA];
    int tx_len;
    int tx_pos;
} R01sPcHost;

void r01s_pc_host_init(R01sPcHost *host, const char *refdes);
R01sEntity *r01s_pc_host_entity(R01sPcHost *host);

void r01s_pc_host_bind_usb(R01sPcHost *host, R01sUsbcReceptacle *usb);
int r01s_pc_host_load_file(R01sPcHost *host, const char *path);
void r01s_pc_host_free_rom(R01sPcHost *host);

/* UI / test "Flash ROM" action. */
int r01s_pc_host_start_stream(R01sPcHost *host);
int r01s_pc_host_stream_tick(R01sPcHost *host, int max_bytes);
int r01s_pc_host_stream_done(const R01sPcHost *host);

#endif

#ifndef retr01_SIM_R01_FLASH_PROTO_H
#define retr01_SIM_R01_FLASH_PROTO_H

#include <stdint.h>

/*
 * R01 cart flasher USB bulk protocol (CDC ACM payload).
 * Host frames: [R][1][cmd][len_lo][len_hi][seq] + payload[len].
 * Device replies with single status byte per processed frame.
 */
#define R01F_MAGIC0 'R'
#define R01F_MAGIC1 '1'
#define R01F_HDR_LEN 6u

#define R01F_CMD_ERASE 0x01u
#define R01F_CMD_DATA 0x02u
#define R01F_CMD_DONE 0x03u

#define R01F_ST_OK 0x00u
#define R01F_ST_ERR 0x01u
#define R01F_ST_BUSY 0x02u
#define R01F_ST_NO_CART 0x03u

#define R01F_MAX_DATA 256u

#endif

#ifndef retr01_SIM_SST39SF040_H
#define retr01_SIM_SST39SF040_H

#include "retr01_sim/entity.h"

#include <stdint.h>

#define R01S_FLASH_BYTES (512u * 1024u)

typedef enum R01sFlashCmd {
    R01S_FLASH_CMD_IDLE = 0,
    R01S_FLASH_CMD_UNLOCK1,
    R01S_FLASH_CMD_UNLOCK2,
    R01S_FLASH_CMD_PROG_ARMED,
    R01S_FLASH_CMD_ERASE_ARMED,
    R01S_FLASH_CMD_ERASE_UNLOCK1,
    R01S_FLASH_CMD_ERASE_READY,
} R01sFlashCmd;

/*
 * 512 KB cart flash with JEDEC byte program / sector+chip erase (hw/md/SST39SF040.md).
 */
typedef struct R01sSst39sf040 {
    R01sEntity base;
    uint8_t mem[R01S_FLASH_BYTES];
    R01sFlashCmd cmd;
    int busy;
    uint8_t prog_shadow;
    R01sLevel we_prev;
} R01sSst39sf040;

void r01s_sst39sf040_init(R01sSst39sf040 *chip, const char *refdes);
R01sEntity *r01s_sst39sf040_entity(R01sSst39sf040 *chip);

void r01s_sst39sf040_load(R01sSst39sf040 *chip, uint32_t addr, const uint8_t *data, uint32_t len);
uint8_t r01s_sst39sf040_peek(const R01sSst39sf040 *chip, uint32_t addr);
void r01s_sst39sf040_poke(R01sSst39sf040 *chip, uint32_t addr, uint8_t data);

/* Host/test direct write cycle (same JEDEC state machine as pin WE#). */
int r01s_sst39sf040_write_cycle(R01sSst39sf040 *chip, uint32_t addr, uint8_t data);
int r01s_sst39sf040_is_busy(const R01sSst39sf040 *chip);

#endif

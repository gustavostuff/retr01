#ifndef R01_SPI_MAILBOX_H
#define R01_SPI_MAILBOX_H

#include <stdint.h>

/*
 * Inter-MCU SPI mailbox (MCU-M master -> S1 / S2 slaves).
 * Shared MOSI/MISO/SCK. Per-slave /SS. See docs/general/hardware.md.
 *
 * Frame: [op][len][payload...]  len = payload bytes (0..R01_MB_PAYLOAD_MAX).
 * MISO status byte while /SS low: bit0 = busy (slave not ready for next frame).
 */

#define R01_MB_PAYLOAD_MAX 64u

#define R01_MB_OP_NOP 0x00u
#define R01_MB_OP_OAM_BULK 0x01u /* payload: start_index(1) + bytes (upto 4*n OAM) */
#define R01_MB_OP_APU_REG 0x02u  /* payload: offset(1) + data bytes in $7F40 window */
#define R01_MB_OP_VBL_MARK 0x03u /* payload empty or flags(1). M saw PLD VBL */
#define R01_MB_OP_PAD_REQ 0x04u  /* optional later: M polls S2 pads over SPI */
#define R01_MB_OP_CHR_TILE 0x05u /* payload: bank(1) tile(1) data[16] = 18 B */

#define R01_MB_STATUS_BUSY 0x01u

#ifndef R01_OAM_ENTRIES
#define R01_OAM_ENTRIES 64u
#endif
#ifndef R01_OAM_BYTES
#define R01_OAM_BYTES (R01_OAM_ENTRIES * 4u)
#endif
#ifndef R01_TILE_BYTES
#define R01_CHR_TILE_BYTES 16u
#else
#define R01_CHR_TILE_BYTES R01_TILE_BYTES
#endif
#define R01_APU_REGS 0x20u /* $7F40-$7F5F */
#ifndef R01_CHR_BANK_BYTES
#define R01_CHR_BANK_BYTES 0x1000u
#endif
#define R01_CHR_BANK_TILES 256u
#define R01_SPR_FIELD_W 128u
#define R01_SPR_FIELD_H 120u

#endif

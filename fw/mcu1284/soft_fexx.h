#ifndef RETR01_FW_SOFT_FEXX_H
#define RETR01_FW_SOFT_FEXX_H

#include <stdint.h>

/* Soft $FExx bank. Host CPU still writes the same addresses. */

typedef struct R01SoftFexx {
    uint8_t ppuctrl;      /* $FE00 */
    uint8_t raster_ctrl;  /* $FE05 */
    uint8_t bg0_x;        /* $FE06 */
    uint8_t bg0_y;        /* $FE07 */
    uint8_t pal_addr;     /* $FE08 low 5 bits used */
    uint8_t map_lo;       /* $FE90 */
    uint8_t map_mid;      /* $FE91 */
    uint8_t map_hi;       /* $FE92 */
    uint32_t map_addr;    /* assembled 24-bit seek */
    uint8_t cart_a14_18;  /* mirror of UPLDV export bits */
    uint8_t last_strobe;  /* debug: last port offset accepted */
} R01SoftFexx;

void r01_soft_fexx_init(R01SoftFexx *s);

/* port = low byte of $FExx (0x00, 0x05, ...). Returns 1 if accepted. */
int r01_soft_fexx_write(R01SoftFexx *s, uint8_t port, uint8_t data);
uint8_t r01_soft_fexx_read(const R01SoftFexx *s, uint8_t port);

/* Rebuild map_addr and cart_a14_18 from lo/mid/hi. */
void r01_soft_fexx_sync_map(R01SoftFexx *s);

#endif

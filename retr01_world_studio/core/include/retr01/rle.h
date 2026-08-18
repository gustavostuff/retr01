#ifndef RETR01_RLE_H
#define RETR01_RLE_H

#include <stddef.h>
#include <stdint.h>

#include "retr01/types.h"

/* Two back-to-back RLE sections: 960 tile bytes + 240 attr bytes. */
int retr01_screen_rle_encode(const uint8_t tiles[RETR01_SCREEN_TILE_BYTES],
                             const uint8_t attrs[RETR01_SCREEN_ATTR_BYTES],
                             uint8_t *out, size_t out_cap, size_t *out_len);

int retr01_screen_rle_decode(const uint8_t *in, size_t in_len,
                             uint8_t tiles[RETR01_SCREEN_TILE_BYTES],
                             uint8_t attrs[RETR01_SCREEN_ATTR_BYTES]);

#endif

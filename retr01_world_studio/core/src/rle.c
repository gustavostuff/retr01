#include "retr01/rle.h"

#include <string.h>

static int rle_encode_section(const uint8_t *in, size_t in_len,
                              uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t pos = 0;
    size_t i = 0;

    *out_len = 0;

    while (i < in_len) {
        size_t run = 1;
        while (i + run < in_len && in[i + run] == in[i] && run < 255) {
            run++;
        }

        if (run >= 3) {
            if (pos + 3 > out_cap) {
                return -1;
            }
            out[pos++] = 0x00;
            out[pos++] = (uint8_t)run;
            out[pos++] = in[i];
            i += run;
            continue;
        }

        size_t lit_start = i;
        size_t lit_count = 0;
        while (i < in_len && lit_count < 127) {
            size_t ahead = 1;
            while (i + ahead < in_len && in[i + ahead] == in[i] && ahead < 255) {
                ahead++;
            }
            if (ahead >= 3) {
                break;
            }
            lit_count++;
            i++;
        }

        if (lit_count == 0) {
            return -1;
        }
        if (pos + 1 + lit_count > out_cap) {
            return -1;
        }
        out[pos++] = (uint8_t)lit_count;
        memcpy(out + pos, in + lit_start, lit_count);
        pos += lit_count;
    }

    *out_len = pos;
    return 0;
}

static int rle_decode_section(const uint8_t *in, size_t in_len, size_t *in_pos,
                              uint8_t *out, size_t target)
{
    size_t produced = 0;

    while (produced < target) {
        if (*in_pos >= in_len) {
            return -1;
        }

        uint8_t lead = in[(*in_pos)++];

        if (lead == 0x00) {
            if (*in_pos + 2 > in_len) {
                return -1;
            }
            uint8_t len = in[(*in_pos)++];
            uint8_t val = in[(*in_pos)++];
            if (len == 0 || produced + len > target) {
                return -1;
            }
            memset(out + produced, val, len);
            produced += len;
        } else if (lead <= 0x7F) {
            size_t lit = lead;
            if (*in_pos + lit > in_len || produced + lit > target) {
                return -1;
            }
            memcpy(out + produced, in + *in_pos, lit);
            *in_pos += lit;
            produced += lit;
        } else {
            return -1;
        }
    }

    return 0;
}

int retr01_screen_rle_encode(const uint8_t tiles[RETR01_SCREEN_TILE_BYTES],
                             const uint8_t attrs[RETR01_SCREEN_ATTR_BYTES],
                             uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t tile_len = 0;
    size_t attr_len = 0;
    size_t total = 0;

    if (rle_encode_section(tiles, RETR01_SCREEN_TILE_BYTES, out, out_cap, &tile_len) != 0) {
        return -1;
    }
    if (rle_encode_section(attrs, RETR01_SCREEN_ATTR_BYTES,
                           out + tile_len, out_cap - tile_len, &attr_len) != 0) {
        return -1;
    }

    total = tile_len + attr_len;
    *out_len = total;
    return 0;
}

int retr01_screen_rle_decode(const uint8_t *in, size_t in_len,
                             uint8_t tiles[RETR01_SCREEN_TILE_BYTES],
                             uint8_t attrs[RETR01_SCREEN_ATTR_BYTES])
{
    size_t pos = 0;

    if (rle_decode_section(in, in_len, &pos, tiles, RETR01_SCREEN_TILE_BYTES) != 0) {
        return -1;
    }
    if (rle_decode_section(in, in_len, &pos, attrs, RETR01_SCREEN_ATTR_BYTES) != 0) {
        return -1;
    }
    if (pos != in_len) {
        return -1;
    }

    return 0;
}

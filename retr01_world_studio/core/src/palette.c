#include "retr01/palette.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_hex_color(const char *s, retr01_rgb_t *rgb)
{
    char *end = NULL;
    unsigned long v = strtoul(s, &end, 16);
    if (end == s || v > 0xFFFFFFul) {
        return -1;
    }
    rgb->r = (uint8_t)((v >> 16) & 0xFF);
    rgb->g = (uint8_t)((v >> 8) & 0xFF);
    rgb->b = (uint8_t)(v & 0xFF);
    return 0;
}

void retr01_palette_set_defaults(retr01_master_palette_t *out)
{
    static const uint8_t bg0[4][4] = {
        {0, 1, 2, 3},
        {4, 5, 6, 7},
        {8, 9, 10, 11},
        {12, 13, 14, 15},
    };
    static const uint8_t sp0[4][4] = {
        {0, 32, 33, 34},
        {0, 35, 36, 37},
        {0, 38, 39, 40},
        {0, 41, 42, 43},
    };

    memset(out, 0, sizeof(*out));
    out->backdrop_index = 0;
    memcpy(out->bg_palettes, bg0, sizeof(bg0));
    memcpy(out->sprite_palettes, sp0, sizeof(sp0));
}

int retr01_palette_load_v01(const char *path, retr01_master_palette_t *out)
{
    FILE *f = fopen(path, "rb");
    char line[4096];
    int row = 0;

    if (!f || !out) {
        if (f) {
            fclose(f);
        }
        return -1;
    }

    retr01_palette_set_defaults(out);

    while (row < 4 && fgets(line, sizeof(line), f)) {
        char *tok = line;
        int col = 0;

        while (col < 16) {
            while (*tok && isspace((unsigned char)*tok)) {
                tok++;
            }
            if (*tok == '\0' || *tok == '\n' || *tok == '\r') {
                break;
            }
            if (*tok == '#') {
                tok++;
            }
            {
                char hex[7];
                int i = 0;
                while (i < 6 && tok[i] && !isspace((unsigned char)tok[i]) &&
                       tok[i] != '\t') {
                    hex[i] = tok[i];
                    i++;
                }
                hex[i] = '\0';
                if (i != 6) {
                    fclose(f);
                    return -1;
                }
                if (parse_hex_color(hex, &out->entries[row * 16 + col]) != 0) {
                    fclose(f);
                    return -1;
                }
                tok += i;
            }
            col++;
            while (*tok && *tok != '\t' && *tok != '\n' && *tok != '\r') {
                tok++;
            }
            if (*tok == '\t') {
                tok++;
            }
        }

        if (col != 16) {
            fclose(f);
            return -1;
        }
        row++;
    }

    fclose(f);
    return (row == 4) ? 0 : -1;
}

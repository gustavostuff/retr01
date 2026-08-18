#ifndef RETR01_PALETTE_H
#define RETR01_PALETTE_H

#include "retr01/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Load retr01_palette_v_01.txt (4 rows x 16 #RRGGBB). */
int retr01_palette_load_v01(const char *path, retr01_master_palette_t *out);

void retr01_palette_set_defaults(retr01_master_palette_t *out);

#ifdef __cplusplus
}
#endif

#endif

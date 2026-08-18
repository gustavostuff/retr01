#ifndef RETR01_SCREEN_H
#define RETR01_SCREEN_H

#include "retr01/types.h"

void retr01_screen_clear(retr01_screen_t *s);
void retr01_attr_set(uint8_t *attrs, int tx, int ty, uint8_t pal);
uint8_t retr01_attr_get(const uint8_t *attrs, int tx, int ty);

#endif

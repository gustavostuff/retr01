#include "retr01/screen.h"

#include <string.h>

void retr01_screen_clear(retr01_screen_t *s)
{
    memset(s->tiles, 0, sizeof(s->tiles));
    memset(s->attrs, 0, sizeof(s->attrs));
    s->col = 0;
    s->row = 0;
    s->flags = 0;
    s->authored_bank = 0;
}

void retr01_attr_set(uint8_t *attrs, int tx, int ty, uint8_t pal)
{
    int idx = (ty / 2) * 16 + (tx / 2);
    int shift = ((ty & 1) * 2 + (tx & 1)) * 2;
    attrs[idx] = (uint8_t)((attrs[idx] & ~(3u << shift)) | ((pal & 3u) << shift));
}

uint8_t retr01_attr_get(const uint8_t *attrs, int tx, int ty)
{
    int idx = (ty / 2) * 16 + (tx / 2);
    int shift = ((ty & 1) * 2 + (tx & 1)) * 2;
    return (uint8_t)((attrs[idx] >> shift) & 3u);
}

#ifndef RETR01_UI_CHROME_H
#define RETR01_UI_CHROME_H

#include <stdint.h>

/* Host-injected PNG chrome. The library does not load or own these buffers. */

typedef struct R01UiChromeImg {
    const uint8_t *rgba; /* RGBA8, row-major; NULL if unavailable */
    int w;
    int h;
} R01UiChromeImg;

typedef struct R01UiChrome {
    R01UiChromeImg radio;    /* 8x16: off (y0-7) / on (y8-15) */
    R01UiChromeImg checkbox; /* 8x16: off / on */
    R01UiChromeImg dot;      /* usually 8x8 */
} R01UiChrome;

void r01_ui_chrome_set(const R01UiChrome *chrome);
const R01UiChrome *r01_ui_chrome(void);

#endif

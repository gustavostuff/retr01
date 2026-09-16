#include "retr01_ui/chrome.h"

#include <string.h>

static R01UiChrome g_chrome;

void r01_ui_chrome_set(const R01UiChrome *chrome) {
    if (!chrome) {
        memset(&g_chrome, 0, sizeof(g_chrome));
        return;
    }
    g_chrome = *chrome;
}

const R01UiChrome *r01_ui_chrome(void) {
    return &g_chrome;
}

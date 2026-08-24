#include "retr01_sim/board_fast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t g_fast_glue;

uint32_t r01s_fast_glue_mask(void) {
    return g_fast_glue;
}

void r01s_fast_glue_set(uint32_t mask) {
    g_fast_glue = mask;
}

int r01s_fast_glue_enabled(R01sFastGlue bit) {
    return (g_fast_glue & (uint32_t)bit) != 0;
}

void r01s_fast_glue_toggle(R01sFastGlue bit) {
    g_fast_glue ^= (uint32_t)bit;
}

static int token_eq(const char *a, const char *b) {
    return a && b && strcasecmp(a, b) == 0;
}

uint32_t r01s_fast_glue_parse(const char *spec) {
    char buf[128];
    char *save = NULL;
    char *tok;
    uint32_t mask = 0;

    if (!spec || spec[0] == '\0') {
        return 0;
    }
    if (token_eq(spec, "0") || token_eq(spec, "none") || token_eq(spec, "off")) {
        return R01S_FAST_GLUE_NONE;
    }
    if (token_eq(spec, "1") || token_eq(spec, "all") || token_eq(spec, "on")) {
        return R01S_FAST_GLUE_BOOT_DEFAULT;
    }
    if (strlen(spec) >= sizeof(buf)) {
        return 0;
    }
    snprintf(buf, sizeof(buf), "%s", spec);
    for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ' || *tok == '\t') {
            tok++;
        }
        if (*tok == '\0') {
            continue;
        }
        if (token_eq(tok, "settle")) {
            mask |= R01S_FAST_GLUE_SETTLE;
        } else if (token_eq(tok, "video")) {
            mask |= R01S_FAST_GLUE_VIDEO;
        } else if (token_eq(tok, "bus")) {
            mask |= R01S_FAST_GLUE_BUS;
        } else if (token_eq(tok, "memory")) {
            mask |= R01S_FAST_GLUE_MEMORY;
        } else if (token_eq(tok, "pins")) {
            mask |= R01S_FAST_GLUE_PINS;
        } else if (token_eq(tok, "boot")) {
            mask |= R01S_FAST_GLUE_BOOT_DEFAULT;
        } else {
            return 0;
        }
    }
    return mask;
}

uint32_t r01s_fast_glue_from_env(void) {
    const char *env = getenv("R01S_FAST");
    uint32_t mask;
    if (!env || env[0] == '\0') {
        return 0;
    }
    mask = r01s_fast_glue_parse(env);
    if (mask || token_eq(env, "0") || token_eq(env, "none") || token_eq(env, "off")) {
        r01s_fast_glue_set(mask);
    }
    return mask;
}

const char *r01s_fast_glue_label(uint32_t mask) {
    if (mask == 0) {
        return "pin-level";
    }
    if ((mask & R01S_FAST_GLUE_BOOT_DEFAULT) == R01S_FAST_GLUE_BOOT_DEFAULT &&
        (mask & ~R01S_FAST_GLUE_BOOT_DEFAULT) == 0) {
        return "fast:boot";
    }
    return "fast:custom";
}

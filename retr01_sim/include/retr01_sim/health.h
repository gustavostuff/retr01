#ifndef RETR01_SIM_HEALTH_H
#define RETR01_SIM_HEALTH_H

#include <stddef.h>

typedef enum R01sHealth {
    R01S_HEALTH_BOOT = 0, /* reset / starting */
    R01S_HEALTH_OK,
    R01S_HEALTH_WARN, /* idle, paused, or waiting on bring-up */
    R01S_HEALTH_FAIL
} R01sHealth;

#define R01S_HEALTH_MAX_ISLANDS 8
#define R01S_HEALTH_ACTIVITY_LEN 44

typedef struct R01sIslandHealth {
    char letter;
    R01sHealth health;
    char activity[R01S_HEALTH_ACTIVITY_LEN];
} R01sIslandHealth;

typedef struct R01sSystemHealth {
    R01sIslandHealth islands[R01S_HEALTH_MAX_ISLANDS];
    int island_count;
    R01sHealth system;
    char system_label[24];
    char system_detail[64];
} R01sSystemHealth;

static inline const char *r01s_health_tag(R01sHealth h) {
    switch (h) {
    case R01S_HEALTH_OK:
        return "OK";
    case R01S_HEALTH_WARN:
        return "WARN";
    case R01S_HEALTH_FAIL:
        return "FAIL";
    default:
        return "BOOT";
    }
}

static inline R01sHealth r01s_health_worst(R01sHealth a, R01sHealth b) {
    if (a == R01S_HEALTH_FAIL || b == R01S_HEALTH_FAIL) {
        return R01S_HEALTH_FAIL;
    }
    if (a == R01S_HEALTH_WARN || b == R01S_HEALTH_WARN) {
        return R01S_HEALTH_WARN;
    }
    if (a == R01S_HEALTH_BOOT || b == R01S_HEALTH_BOOT) {
        return R01S_HEALTH_BOOT;
    }
    return R01S_HEALTH_OK;
}

#endif

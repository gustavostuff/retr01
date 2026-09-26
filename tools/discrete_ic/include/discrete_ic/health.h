#ifndef DISCRETE_IC_HEALTH_H
#define DISCRETE_IC_HEALTH_H

#include <stddef.h>

typedef enum NsHealth {
    NS_HEALTH_BOOT = 0, /* reset / starting */
    NS_HEALTH_OK,
    NS_HEALTH_WARN, /* idle, paused, or waiting on bring-up */
    NS_HEALTH_FAIL
} NsHealth;

#define NS_HEALTH_MAX_ISLANDS 11
#define NS_HEALTH_ACTIVITY_LEN 44
#define NS_HEALTH_DEBUG_LEN 192
#define NS_HEALTH_SYSTEM_DEBUG_LEN 1024

typedef struct NsIslandHealth {
    char letter;
    NsHealth health;
    char activity[NS_HEALTH_ACTIVITY_LEN];
    char debug[NS_HEALTH_DEBUG_LEN]; /* paste-friendly dump for WARN/FAIL */
} NsIslandHealth;

typedef struct NsSystemHealth {
    NsIslandHealth islands[NS_HEALTH_MAX_ISLANDS];
    int island_count;
    NsHealth system;
    char system_label[24];
    char system_detail[64];
    char system_debug[NS_HEALTH_SYSTEM_DEBUG_LEN];
} NsSystemHealth;

static inline const char *ns_health_tag(NsHealth h) {
    switch (h) {
    case NS_HEALTH_OK:
        return "OK";
    case NS_HEALTH_WARN:
        return "WARN";
    case NS_HEALTH_FAIL:
        return "FAIL";
    default:
        return "BOOT";
    }
}

static inline NsHealth ns_health_worst(NsHealth a, NsHealth b) {
    if (a == NS_HEALTH_FAIL || b == NS_HEALTH_FAIL) {
        return NS_HEALTH_FAIL;
    }
    if (a == NS_HEALTH_WARN || b == NS_HEALTH_WARN) {
        return NS_HEALTH_WARN;
    }
    if (a == NS_HEALTH_BOOT || b == NS_HEALTH_BOOT) {
        return NS_HEALTH_BOOT;
    }
    return NS_HEALTH_OK;
}

#endif

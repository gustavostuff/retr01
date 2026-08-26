#include "agent_debug_log.h"

#include <stdio.h>
#include <time.h>

#define R01S_AGENT_DEBUG_LOG "/home/g/Repos/retr01/.cursor/debug-af3c46.log"

void r01s_agent_debug_log(const char *hypothesis_id, const char *location, const char *message,
                          int i0, int i1, int i2, int i3) {
    FILE *f;
    struct timespec ts;

    /* #region agent log */
    if (!hypothesis_id || !location || !message) {
        return;
    }
    f = fopen(R01S_AGENT_DEBUG_LOG, "a");
    if (!f) {
        return;
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(f,
            "{\"sessionId\":\"af3c46\",\"hypothesisId\":\"%s\",\"location\":\"%s\","
            "\"message\":\"%s\",\"data\":{\"i0\":%d,\"i1\":%d,\"i2\":%d,\"i3\":%d},"
            "\"timestamp\":%lld}\n",
            hypothesis_id, location, message, i0, i1, i2, i3,
            (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
    fclose(f);
    /* #endregion */
}

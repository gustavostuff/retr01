#ifndef RETR01_SIM_AGENT_DEBUG_LOG_H
#define RETR01_SIM_AGENT_DEBUG_LOG_H

#include <stdint.h>

void r01s_agent_debug_log(const char *hypothesis_id, const char *location, const char *message,
                          int i0, int i1, int i2, int i3);

#endif

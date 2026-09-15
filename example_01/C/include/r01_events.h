#ifndef R01_EVENTS_H
#define R01_EVENTS_H

#include <stdint.h>
typedef struct R01GameCtx R01GameCtx;
typedef void (*R01EventFn)(R01GameCtx *ctx);
int r01_event_on_button(uint8_t btn, R01EventFn fn);
void r01_runtime_dispatch_buttons(R01GameCtx *ctx);

#endif

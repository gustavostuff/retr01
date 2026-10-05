#ifndef R01A_LAB_SIM_H
#define R01A_LAB_SIM_H

#include <stdint.h>

struct R01aBoard;
struct NsIslandGroup;
struct NsPinNetlist;
struct NsIslandBuilder;

/* Board calls kept out of the Tier H UI translation unit (chip headers clash). */
void r01a_lab_boot(struct R01aBoard *board);
struct NsIslandGroup *r01a_lab_group(struct R01aBoard *board);
struct NsPinNetlist *r01a_lab_pins(struct R01aBoard *board);
struct NsIslandBuilder *r01a_lab_builder(struct R01aBoard *board);
int r01a_lab_running(const struct R01aBoard *board);
void r01a_lab_set_running(struct R01aBoard *board, int on);
void r01a_lab_step(struct R01aBoard *board);
void r01a_lab_step_dots(struct R01aBoard *board, uint32_t dots);
void r01a_lab_reset(struct R01aBoard *board);

#endif

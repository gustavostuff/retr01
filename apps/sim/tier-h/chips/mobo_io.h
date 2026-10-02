#ifndef RETR01_SIM_MOBO_IO_H
#define RETR01_SIM_MOBO_IO_H

#include "retr01_sim/ns_compat.h"

/* Motherboard connectors and the reset supervisor. Not cart or pad silicon. */
typedef struct R01sMoboIo {
    R01sEntity j1;
    R01sEntity j2;
    R01sEntity j3;
    R01sEntity j4;
    R01sEntity j5;
    R01sEntity j7;
    R01sEntity j36;
    R01sEntity u130;
} R01sMoboIo;

void r01s_mobo_io_init(R01sMoboIo *io);
void r01s_mobo_io_register(R01sMoboIo *io, R01sPinNetlist *nl);

#endif

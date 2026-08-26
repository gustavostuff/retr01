#ifndef retr01_EMU_CPU_H
#define retr01_EMU_CPU_H

#include <stdint.h>

struct R01eMachine;

typedef struct R01eCpu {
    uint8_t a, x, y, s, p;
    uint16_t pc;
    uint64_t cycles;
    int stalled; /* unused; reserved for RDY */
} R01eCpu;

void r01e_cpu_reset(R01eCpu *cpu, struct R01eMachine *m);
/* Execute one instruction; returns cycles consumed (approx W65C02). */
int r01e_cpu_step(R01eCpu *cpu, struct R01eMachine *m);

#endif

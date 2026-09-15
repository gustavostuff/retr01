#ifndef retr01_EMU_CPU_H
#define retr01_EMU_CPU_H

#include <stdint.h>

struct R01eMachine;

typedef struct R01eCpu {
    uint8_t a, x, y, s, p;
    uint16_t pc;
    uint64_t cycles;
    /* RDY wait-states remaining after $7F24 / $7F72 (0 = run). */
    uint32_t stalled;
} R01eCpu;

void r01e_cpu_reset(R01eCpu *cpu, struct R01eMachine *m);
/* Execute one instruction (or one RDY wait-state); returns cycles consumed. */
int r01e_cpu_step(R01eCpu *cpu, struct R01eMachine *m);
/* Assert CPU_RDY hold for N wait-states (cart/machine EE). */
void r01e_cpu_rdy_hold(R01eCpu *cpu, uint32_t holds);
/* Legacy: cart write hold. */
void r01e_cpu_rdy_hold_ee(R01eCpu *cpu);
int r01e_cpu_rdy_is_held(const R01eCpu *cpu);
uint32_t r01e_cpu_rdy_remaining(const R01eCpu *cpu);

#endif

#ifndef RETR01_EMU_CPU_H
#define RETR01_EMU_CPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RETR01_C_FLAG 0x01
#define RETR01_Z_FLAG 0x02
#define RETR01_I_FLAG 0x04
#define RETR01_D_FLAG 0x08
#define RETR01_B_FLAG 0x10
#define RETR01_U_FLAG 0x20
#define RETR01_V_FLAG 0x40
#define RETR01_N_FLAG 0x80

typedef struct retr01_cpu {
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint8_t p;
    uint64_t cycles;
    uint8_t stopped;
    uint8_t waiting;
} retr01_cpu_t;

struct retr01_emu;

void retr01_cpu_reset(struct retr01_emu *e);
void retr01_cpu_nmi(struct retr01_emu *e);
void retr01_cpu_irq(struct retr01_emu *e);
int retr01_cpu_step(struct retr01_emu *e);

#ifdef __cplusplus
}
#endif

#endif

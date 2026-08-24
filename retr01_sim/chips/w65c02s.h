#ifndef RETR01_SIM_W65C02S_H
#define RETR01_SIM_W65C02S_H

#include "retr01_sim/entity.h"

#include <stdint.h>

/*
 * Island C — W65C02S game CPU (functional Phase-1 subset).
 * Pinout: hw/md/W65C02S.md
 *
 * Implemented:
 *   - BE=0 => A/D/RWB Hi-Z
 *   - RESB hold + 7-cycle reset + vector fetch $FFFC/$FFFD
 *   - EA NOP, A9 LDA #imm, A2 LDX #imm, AD LDA abs, 8D STA abs,
 *     4C JMP abs, CA DEX, D0 BNE rel (Z from LDA/LDX/DEX)
 * tick() = one PHI2 cycle.
 */
typedef enum R01sCpuPhase {
    R01S_CPU_RES_HOLD = 0,
    R01S_CPU_RES_WAIT,
    R01S_CPU_VEC_PCL,
    R01S_CPU_VEC_PCH,
    R01S_CPU_FETCH,
    R01S_CPU_OP_IMM,  /* operand / immediate */
    R01S_CPU_OP_ADL,  /* absolute address low */
    R01S_CPU_OP_ADH,  /* absolute address high */
    R01S_CPU_OP_DATA  /* abs data R/W */
} R01sCpuPhase;

typedef struct R01sW65C02S {
    R01sEntity base;
    uint16_t pc;
    uint16_t ab; /* address bus latch */
    uint16_t ea; /* effective address while executing */
    uint8_t a, x, y, s, p;
    uint8_t ir;
    int res_cycles; /* remaining internal reset cycles before vector */
    R01sCpuPhase phase;
    int sync;
    int rwb; /* 1=read */
} R01sW65C02S;

void r01s_w65c02s_init(R01sW65C02S *chip, const char *refdes);
R01sEntity *r01s_w65c02s_entity(R01sW65C02S *chip);

uint16_t r01s_w65c02s_pc(const R01sW65C02S *chip);
uint16_t r01s_w65c02s_ab(const R01sW65C02S *chip);
uint8_t r01s_w65c02s_a(const R01sW65C02S *chip);
int r01s_w65c02s_rwb(const R01sW65C02S *chip);
R01sCpuPhase r01s_w65c02s_phase(const R01sW65C02S *chip);

#endif

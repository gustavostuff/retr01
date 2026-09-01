#include "retr01_sim/bus.h"
#include "test_common.h"
#include "w65c02s.h"

static void cpu_ready(R01sEntity *e) {
    r01s_entity_drive(e, "BE", R01S_LVL_H);
    r01s_entity_drive(e, "RDY", R01S_LVL_H);
    r01s_entity_drive(e, "IRQB", R01S_LVL_H);
    r01s_entity_drive(e, "NMIB", R01S_LVL_H);
    r01s_entity_drive(e, "PHI2", R01S_LVL_H);
}

int main(void) {
    R01sW65C02S cpu;
    R01sEntity *e;
    int i;

    r01s_w65c02s_init(&cpu, "U1");
    e = r01s_w65c02s_entity(&cpu);
    expect_true(e->pin_count == 40, "40 pins");

    /* BE=0 => bus Hi-Z */
    r01s_entity_drive(e, "BE", R01S_LVL_L);
    r01s_entity_drive(e, "RESB", R01S_LVL_H);
    r01s_entity_eval(e);
    expect_true(r01s_entity_sense(e, "A0") == R01S_LVL_Z, "BE off A Hi-Z");
    expect_true(r01s_entity_sense(e, "RWB") == R01S_LVL_Z, "BE off RWB Hi-Z");

    /* Reset hold then release, fetch vector $8000 */
    cpu_ready(e);
    r01s_entity_drive(e, "RESB", R01S_LVL_L);
    r01s_entity_eval(e);
    expect_true(r01s_w65c02s_phase(&cpu) == R01S_CPU_RES_HOLD, "held in reset");

    r01s_entity_drive(e, "RESB", R01S_LVL_H);
    for (i = 0; i < 16 && r01s_w65c02s_phase(&cpu) != R01S_CPU_VEC_PCL; i++) {
        r01s_entity_tick(e);
    }
    expect_true(r01s_w65c02s_phase(&cpu) == R01S_CPU_VEC_PCL, "vector PCL phase");
    expect_true(r01s_bus_read(e, "A", 16) == 0xFFFC, "addr FFFC");
    expect_true(r01s_level_is_high(r01s_entity_sense(e, "RWB")), "read");
    expect_true(r01s_level_is_low(r01s_entity_sense(e, "VPB")), "VPB low on vector");

    r01s_bus_write(e, "D", 8, 0x00); /* PCL */
    r01s_entity_tick(e);
    expect_true(r01s_w65c02s_phase(&cpu) == R01S_CPU_VEC_PCH, "vector PCH phase");
    expect_true(r01s_bus_read(e, "A", 16) == 0xFFFD, "addr FFFD");

    r01s_bus_write(e, "D", 8, 0x80); /* PCH */
    r01s_entity_tick(e);
    expect_true(r01s_w65c02s_pc(&cpu) == 0x8000, "PC from vector");
    expect_true(r01s_w65c02s_phase(&cpu) == R01S_CPU_FETCH, "fetch");
    expect_true(r01s_bus_read(e, "A", 16) == 0x8000, "fetch addr");
    expect_true(r01s_level_is_high(r01s_entity_sense(e, "SYNC")), "SYNC on fetch");

    r01s_bus_write(e, "D", 8, 0xEA); /* NOP */
    r01s_entity_tick(e);
    expect_true(r01s_w65c02s_pc(&cpu) == 0x8001, "PC advanced");

    return test_done("test_w65c02s");
}

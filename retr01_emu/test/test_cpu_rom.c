#include "../../retr01_world_studio/tests/test_harness.h"

#include "nmi_prg.h"
#include "retr01_emu/emu.h"

#include <stdlib.h>
#include <string.h>

TEST(reset_stores_signature)
{
    retr01_emu_t e;
    retr01_emu_init(&e);
    e.cart.prg = (uint8_t *)malloc(0x8000);
    ASSERT(e.cart.prg != NULL);
    e.cart.prg_size = 0x8000;
    retr01_test_fill_nmi_prg(e.cart.prg, e.cart.prg_size);
    retr01_emu_reset(&e);
    ASSERT_EQ(e.cpu.pc, 0x8000);

    /* LDA/STA/LDA/STA/CLI/JMP */
    retr01_cpu_step(&e);
    retr01_cpu_step(&e);
    retr01_cpu_step(&e);
    retr01_cpu_step(&e);
    retr01_cpu_step(&e);
    ASSERT_EQ(e.ram[0x0200], 0xAA);
    ASSERT_EQ(retr01_bus_read(&e, 0xFE00), 0x80);
    retr01_emu_free(&e);
}

TEST(nmi_counts_frames)
{
    retr01_emu_t e;
    retr01_emu_init(&e);
    e.cart.prg = (uint8_t *)malloc(0x8000);
    ASSERT(e.cart.prg != NULL);
    e.cart.prg_size = 0x8000;
    retr01_test_fill_nmi_prg(e.cart.prg, e.cart.prg_size);
    retr01_emu_reset(&e);
    retr01_emu_set_pad(&e, 0, RETR01_PAD_A | RETR01_PAD_RIGHT);
    ASSERT_EQ(retr01_emu_run_frames(&e, 60), 0);
    ASSERT_EQ(e.ram[0x0200], 0xAA);
    ASSERT_EQ(e.ram[0x0201], 60);
    ASSERT_EQ(e.ram[0x0202], (uint8_t)(RETR01_PAD_A | RETR01_PAD_RIGHT));
    retr01_emu_free(&e);
}

TEST(stp_halts)
{
    retr01_emu_t e;
    uint8_t *prg;
    retr01_emu_init(&e);
    prg = (uint8_t *)malloc(0x8000);
    ASSERT(prg != NULL);
    memset(prg, 0xEA, 0x8000);
    prg[0] = 0xA9; /* LDA #$5A */
    prg[1] = 0x5A;
    prg[2] = 0x8D; /* STA $0200 */
    prg[3] = 0x00;
    prg[4] = 0x02;
    prg[5] = 0xDB; /* STP */
    prg[0x7FFC] = 0x00;
    prg[0x7FFD] = 0x80;
    e.cart.prg = prg;
    e.cart.prg_size = 0x8000;
    retr01_emu_reset(&e);
    retr01_emu_run_frame(&e);
    ASSERT_EQ(e.ram[0x0200], 0x5A);
    ASSERT(e.cpu.stopped);
    retr01_emu_free(&e);
}

TEST_RUNNER_BEGIN("test_cpu_rom")
RUN_TEST_RC(reset_stores_signature);
RUN_TEST_RC(nmi_counts_frames);
RUN_TEST_RC(stp_halts);
TEST_RUNNER_END()

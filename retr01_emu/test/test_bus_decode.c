#include "../../retr01_world_studio/tests/test_harness.h"

#include "retr01_emu/emu.h"

#include <string.h>

TEST(ram_rw)
{
    retr01_emu_t e;
    retr01_emu_init(&e);
    retr01_bus_write(&e, 0x0000, 0x12);
    retr01_bus_write(&e, 0x7FFF, 0x34);
    ASSERT_EQ(retr01_bus_read(&e, 0x0000), 0x12);
    ASSERT_EQ(retr01_bus_read(&e, 0x7FFF), 0x34);
    retr01_emu_free(&e);
}

TEST(prg_window_readonly)
{
    retr01_emu_t e;
    uint8_t prg[0x8000];
    retr01_emu_init(&e);
    memset(prg, 0xEA, sizeof(prg));
    prg[0] = 0xA9;
    e.cart.prg = prg;
    e.cart.prg_size = sizeof(prg);
    ASSERT_EQ(retr01_bus_read(&e, 0x8000), 0xA9);
    retr01_bus_write(&e, 0x8000, 0x00);
    ASSERT_EQ(retr01_bus_read(&e, 0x8000), 0xA9);
    e.cart.prg = NULL;
    e.cart.prg_size = 0;
    retr01_emu_free(&e);
}

TEST(prg_bank_fe80)
{
    retr01_emu_t e;
    uint8_t prg[0x10000];
    retr01_emu_init(&e);
    memset(prg, 0, sizeof(prg));
    prg[0x0000] = 0x11;
    prg[0x8000] = 0x22;
    e.cart.prg = prg;
    e.cart.prg_size = sizeof(prg);
    ASSERT_EQ(retr01_bus_read(&e, 0x8000), 0x11);
    retr01_bus_write(&e, 0xFE80, 1);
    ASSERT_EQ(retr01_bus_read(&e, 0xFE80), 1);
    ASSERT_EQ(retr01_bus_read(&e, 0x8000), 0x22);
    e.cart.prg = NULL;
    e.cart.prg_size = 0;
    retr01_emu_free(&e);
}

TEST(pad_fe60_host_owned)
{
    retr01_emu_t e;
    retr01_emu_init(&e);
    retr01_emu_set_pad(&e, 0, 0x1F);
    ASSERT_EQ(retr01_bus_read(&e, 0xFE60), 0x1F);
    retr01_bus_write(&e, 0xFE60, 0x00);
    ASSERT_EQ(retr01_bus_read(&e, 0xFE60), 0x1F);
    retr01_emu_free(&e);
}

TEST(vram_port)
{
    retr01_emu_t e;
    retr01_emu_init(&e);
    retr01_bus_write(&e, 0xFE13, 1);
    retr01_bus_write(&e, 0xFE10, 0x00);
    retr01_bus_write(&e, 0xFE11, 0x00);
    retr01_bus_write(&e, 0xFE12, 0xAB);
    retr01_bus_write(&e, 0xFE10, 0x00);
    retr01_bus_write(&e, 0xFE11, 0x00);
    ASSERT_EQ(retr01_bus_read(&e, 0xFE12), 0xAB);
    retr01_emu_free(&e);
}

TEST_RUNNER_BEGIN("test_bus_decode")
RUN_TEST_RC(ram_rw);
RUN_TEST_RC(prg_window_readonly);
RUN_TEST_RC(prg_bank_fe80);
RUN_TEST_RC(pad_fe60_host_owned);
RUN_TEST_RC(vram_port);
TEST_RUNNER_END()

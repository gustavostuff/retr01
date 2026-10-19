#include "r01_soft_sel_demux.h"
#include "test_common.h"

int main(void) {
    expect_true(r01_soft_sel_demux(0, 0x00) == 0x00, "soft0 FE00");
    expect_true(r01_soft_sel_demux(0, 0x06) == 0x06, "soft0 FE06");
    expect_true(r01_soft_sel_demux(0, 0x07) == 0x07, "soft0 FE07");
    expect_true(r01_soft_sel_demux(0, 0x05) == 0xFF, "soft0 reject FE05");

    expect_true(r01_soft_sel_demux(1, 0x05) == 0x05, "soft1 FE05");
    expect_true(r01_soft_sel_demux(1, 0x08) == 0x08, "soft1 FE08");
    expect_true(r01_soft_sel_demux(1, 0x00) == 0xFF, "soft1 reject FE00");

    expect_true(r01_soft_sel_demux(2, 0x21) == 0x21, "soft2 FE21");
    expect_true(r01_soft_sel_demux(2, 0x5F) == 0x5F, "soft2 FE5F");
    expect_true(r01_soft_sel_demux(2, 0x72) == 0x72, "soft2 FE72");
    expect_true(r01_soft_sel_demux(2, 0x06) == 0xFF, "soft2 reject FE06");

    return test_done("test_soft_sel_demux");
}

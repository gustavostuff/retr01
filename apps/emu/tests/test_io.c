#include "retr01_emu/cpu.h"
#include "retr01_emu/machine.h"
#include "r01_hw_regs.h"
#include "stub_cart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

int main(void) {
    R01eMachine m;
    char err[256];
    uint8_t v;
    uint32_t map0;
    uint8_t *stub;
    size_t stub_len = (size_t)R01E_STUB_CART_LEN;

    stub = (uint8_t *)malloc(stub_len);
    if (!stub) {
        return fail("oom stub");
    }
    if (r01e_test_stub_cart(stub, stub_len) != 0) {
        free(stub);
        return fail("build stub");
    }
    if (r01e_machine_init_mem(&m, stub, stub_len, err, sizeof(err)) != 0) {
        fprintf(stderr, "FAIL init: %s\n", err);
        free(stub);
        return 1;
    }
    free(stub);
    r01e_mem_write(&m, 0x7F02, 0x55);
    if (m.io.scroll_x != 0x55) {
        r01e_machine_shutdown(&m);
        return fail("scroll_x latch");
    }
    r01e_mem_write(&m, 0x7F02, 0xFF);
    if (m.io.scroll_x != 0x7F) {
        r01e_machine_shutdown(&m);
        return fail("scroll_x clamp");
    }
    r01e_mem_write(&m, 0x7F03, 0x80);
    if (m.io.scroll_y != 119) {
        r01e_machine_shutdown(&m);
        return fail("scroll_y clamp");
    }

    r01e_mem_write(&m, 0x7F10, 0x00);
    r01e_mem_write(&m, 0x7F11, 0x00);
    r01e_mem_write(&m, 0x7F12, 0xAA);
    r01e_mem_write(&m, 0x7F12, 0xBB);
    if (m.video.vram[0] != 0xAA || m.video.vram[1] != 0xBB) {
        r01e_machine_shutdown(&m);
        return fail("VRAM auto-inc write");
    }
    r01e_mem_write(&m, 0x7F10, 0x00);
    r01e_mem_write(&m, 0x7F11, 0x00);
    v = r01e_mem_read(&m, 0x7F12);
    if (v != 0xAA) {
        r01e_machine_shutdown(&m);
        return fail("VRAM auto-inc read");
    }

    map0 = m.cart.off_prg; /* known absolute in image; any readable byte */
    r01e_mem_write(&m, 0x7F90, (uint8_t)(map0 & 0xFF));
    r01e_mem_write(&m, 0x7F91, (uint8_t)((map0 >> 8) & 0xFF));
    r01e_mem_write(&m, 0x7F92, (uint8_t)((map0 >> 16) & 0xFF));
    v = r01e_mem_read(&m, 0x7F93);
    if (v != r01e_cart_read(&m.cart, map0)) {
        r01e_machine_shutdown(&m);
        return fail("MAP $7F93 read");
    }
    if (m.io.map_addr != ((map0 + 1) & 0xFFFFFFu)) {
        r01e_machine_shutdown(&m);
        return fail("MAP auto-inc");
    }

    /* Unused $7F80: open as 0 today. */
    v = r01e_mem_read(&m, 0x7F80);
    if (v != 0) {
        r01e_machine_shutdown(&m);
        return fail("$7F80 unused read");
    }
    r01e_mem_write(&m, 0x7F80, 0x03); /* must not bank PRG */
    if (r01e_mem_read(&m, 0x7F80) != 0) {
        r01e_machine_shutdown(&m);
        return fail("$7F80 write ignored");
    }

    /* Cart save mailbox $7F22-$7F24. */
    r01e_mem_write(&m, 0x7F23, 0x34);
    r01e_mem_write(&m, 0x7F22, 0x12);
    r01e_mem_write(&m, 0x7F22, R01E_CARTEE_CMD_WRITE);
    r01e_mem_write(&m, 0x7F24, 0xAB);
    r01e_mem_write(&m, 0x7F22, R01E_CARTEE_CMD_READ);
    v = r01e_mem_read(&m, 0x7F24);
    if (v != 0xAB) {
        r01e_machine_shutdown(&m);
        return fail("cart save EEPROM readback");
    }

    /* Machine EEPROM $7F70-$7F72 (512 B, 9-bit). */
    r01e_mem_write(&m, 0x7F70, 0x56);
    r01e_mem_write(&m, 0x7F71, 0x01);
    r01e_mem_write(&m, 0x7F72, 0xCD);
    r01e_mem_write(&m, 0x7F70, 0x56);
    r01e_mem_write(&m, 0x7F71, 0x01);
    v = r01e_mem_read(&m, 0x7F72);
    if (v != 0xCD) {
        r01e_machine_shutdown(&m);
        return fail("machine EEPROM readback");
    }
    /* AH high bits ignored: $01xx and $FFxx alias into 9-bit space. */
    r01e_mem_write(&m, 0x7F70, 0x00);
    r01e_mem_write(&m, 0x7F71, 0xFF);
    r01e_mem_write(&m, 0x7F72, 0x5A);
    r01e_mem_write(&m, 0x7F70, 0x00);
    r01e_mem_write(&m, 0x7F71, 0x01);
    v = r01e_mem_read(&m, 0x7F72);
    if (v != 0x5A) {
        r01e_machine_shutdown(&m);
        return fail("machine EEPROM 9-bit alias");
    }

    /* RDY stall on $7F72 / $7F24 (ms-scale cycle stubs). */
    {
        uint64_t c0;
        uint32_t i;
        r01e_mem_write(&m, 0x7F70, 0x01);
        r01e_mem_write(&m, 0x7F71, 0x00);
        r01e_mem_write(&m, 0x7F72, 0x11);
        if (!r01e_cpu_rdy_is_held(&m.cpu) || r01e_cpu_rdy_remaining(&m.cpu) != R01_RDY_MEEPROM_WRITE_HOLDS) {
            r01e_machine_shutdown(&m);
            return fail("RDY held after $7F72 write");
        }
        c0 = m.cpu.cycles;
        for (i = 0; i < R01_RDY_MEEPROM_WRITE_HOLDS; i++) {
            if (r01e_machine_step_insn(&m) != 1) {
                r01e_machine_shutdown(&m);
                return fail("RDY wait-state is 1 cycle");
            }
        }
        if (r01e_cpu_rdy_is_held(&m.cpu) || m.cpu.cycles != c0 + R01_RDY_MEEPROM_WRITE_HOLDS) {
            r01e_machine_shutdown(&m);
            return fail("RDY released after MEEPROM write holds");
        }
        r01e_mem_write(&m, 0x7F22, R01E_CARTEE_CMD_WRITE);
        r01e_mem_write(&m, 0x7F24, 0x22);
        if (r01e_cpu_rdy_remaining(&m.cpu) != R01_RDY_CARTEE_WRITE_HOLDS) {
            r01e_machine_shutdown(&m);
            return fail("RDY held after $7F24 write");
        }
        (void)r01e_mem_read(&m, 0x7F24);
        if (r01e_cpu_rdy_remaining(&m.cpu) != R01_RDY_CARTEE_READ_HOLDS) {
            r01e_machine_shutdown(&m);
            return fail("RDY reloaded on $7F24 read");
        }
        for (i = 0; i < R01_RDY_CARTEE_READ_HOLDS; i++) {
            (void)r01e_machine_step_insn(&m);
        }
        if (r01e_cpu_rdy_is_held(&m.cpu)) {
            r01e_machine_shutdown(&m);
            return fail("RDY clear after $7F24 read holds");
        }
    }

    /* Soft $7F00 readback. */
    r01e_mem_write(&m, 0x7F00, 0x87);
    if (r01e_mem_read(&m, 0x7F00) != 0x87) {
        r01e_machine_shutdown(&m);
        return fail("$7F00 readback");
    }

    /* OAM $7F20/$7F21: wrap past 256 into high half. */
    {
        int i;
        r01e_mem_write(&m, 0x7F20, 0x00);
        r01e_mem_write(&m, 0x7F21, 0x10);
        r01e_mem_write(&m, 0x7F21, 0x01);
        r01e_mem_write(&m, 0x7F21, 0x00);
        r01e_mem_write(&m, 0x7F21, 0x20);
        if (m.io.oam[0] != 0x10 || m.io.oam[1] != 0x01 || m.io.oam[3] != 0x20) {
            r01e_machine_shutdown(&m);
            return fail("OAM soft write");
        }
        r01e_mem_write(&m, 0x7F20, 0x00);
        for (i = 0; i < 256; i++) {
            r01e_mem_write(&m, 0x7F21, 0xEE);
        }
        /* addr == 256 after 256 stores from 0 */
        r01e_mem_write(&m, 0x7F21, 0x42);
        if (m.io.oam[256] != 0x42) {
            r01e_machine_shutdown(&m);
            return fail("OAM wrap to [256]");
        }
        r01e_mem_write(&m, 0x7F20, 0x00);
        if (r01e_mem_read(&m, 0x7F21) != 0xEE) {
            r01e_machine_shutdown(&m);
            return fail("OAM readback");
        }
    }

    /* APU soft window $7F40-$7F5F. */
    r01e_mem_write(&m, 0x7F40, 0x8F);
    r01e_mem_write(&m, 0x7F41, 0x10);
    r01e_mem_write(&m, 0x7F5F, 0xAB);
    if (r01e_mem_read(&m, 0x7F40) != 0x8F || r01e_mem_read(&m, 0x7F41) != 0x10 ||
        r01e_mem_read(&m, 0x7F5F) != 0xAB) {
        r01e_machine_shutdown(&m);
        return fail("APU soft R/W");
    }

    /* Host Play NMI tracker -> $7F40 (builtin demo when bin missing). */
    if (r01e_machine_apu_tracker_start(&m, NULL) != 0) {
        r01e_machine_shutdown(&m);
        return fail("apu tracker start");
    }
    if ((m.io.apu[0] & 0x01u) == 0) {
        r01e_machine_shutdown(&m);
        return fail("tracker armed ch0");
    }
    r01e_machine_apu_tracker_stop(&m);

    /* BG0 scroll $7F06/$7F07. */
    r01e_mem_write(&m, 0x7F06, 0x10);
    r01e_mem_write(&m, 0x7F07, 0x20);
    if (m.io.bg0_scroll_x != 0x10 || m.io.bg0_scroll_y != 0x20) {
        r01e_machine_shutdown(&m);
        return fail("BG0 scroll latch");
    }

    printf("ok io scroll/vram/map/fe80/eeprom/oam/apu\n");
    r01e_machine_shutdown(&m);
    return 0;
}

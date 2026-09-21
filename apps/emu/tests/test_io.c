#include "retr01_emu/cpu.h"
#include "retr01_emu/io.h"
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
    /* Isolate I/O checks from Host Play; park beam in VBlank for scroll/pal. */
    m.play.enabled = 0;
    while (m.io.dot_y < R01E_VISIBLE_H) {
        r01e_io_dot(&m);
    }
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

    /* RDY stall on $7F72 / $7F24 (short handoff stubs, not full tWC). */
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

    /* OAM $7F20/$7F21: 64 sprites x 4 B = 256 B, auto-inc wraps. */
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
        /* After 256 stores, addr wrapped to 0. */
        r01e_mem_write(&m, 0x7F21, 0x42);
        if (m.io.oam[0] != 0x42) {
            r01e_machine_shutdown(&m);
            return fail("OAM wrap to [0]");
        }
        r01e_mem_write(&m, 0x7F20, 0x01);
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

    /* Mid-active scroll holds until VBlank; pads latch at VBlank enter. */
    {
        m.play.enabled = 0; /* isolate I/O fences from Host Play publish */
        m.io.frame = 1;
        m.io.dot_y = 10;
        m.io.dot_x = 0;
        m.io.scroll_x = 0x11;
        m.io.scroll_pal_pending = 0;
        r01e_mem_write(&m, 0x7F02, 0x33);
        if (m.io.scroll_x != 0x11 || !m.io.scroll_pal_pending || m.io.scroll_x_next != 0x33) {
            r01e_machine_shutdown(&m);
            return fail("mid-active scroll pending");
        }
        r01e_machine_set_pad(&m, 0, 0x55);
        if (m.io.pad0 != 0 || m.io.pad0_host != 0x55) {
            r01e_machine_shutdown(&m);
            return fail("pad staged not latched");
        }
        while (m.io.dot_y < R01E_VISIBLE_H) {
            r01e_io_dot(&m);
        }
        if (m.io.scroll_x != 0x33 || m.io.scroll_pal_pending) {
            r01e_machine_shutdown(&m);
            return fail("VBlank scroll flush");
        }
        if (m.io.pad0 != 0x55) {
            r01e_machine_shutdown(&m);
            return fail("VBlank pad latch");
        }
    }

    /* BG0 auto parallax: end-aligned (cols-1)/(cols-1), never overshoot. */
    {
        R01eVideo *vid = &m.video;
        vid->bg0_scroll_manual = 0;
        vid->bg0_cols = 2;
        vid->bg0_rows = 2;
        vid->l1_cols = 4;
        vid->l1_rows = 4;
        vid->l1_origin_x = 0;
        vid->l1_origin_y = 0;
        vid->cam_x = 0;
        vid->cam_y = 0;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 0 || vid->l0_cam_y != 0) {
            r01e_machine_shutdown(&m);
            return fail("BG0 scroll at origin");
        }
        /* Mid: cam at 1 screen of BG1 travel -> BG0 nearest pixel of 1/3 screen. */
        vid->cam_x = R01E_SCREEN_PX_W;
        vid->cam_y = R01E_SCREEN_PX_H;
        r01e_video_update_bg0_scroll(&m);
        {
            int mid_x = (R01E_SCREEN_PX_W * R01E_SCREEN_PX_W + (3 * R01E_SCREEN_PX_W) / 2) / (3 * R01E_SCREEN_PX_W);
            int mid_y = (R01E_SCREEN_PX_H * R01E_SCREEN_PX_H + (3 * R01E_SCREEN_PX_H) / 2) / (3 * R01E_SCREEN_PX_H);
            if (vid->l0_cam_x != mid_x || vid->l0_cam_y != mid_y) {
                r01e_machine_shutdown(&m);
                return fail("BG0 mid parallax ratio");
            }
        }
        /* Far end of BG1 bbox: BG0 must land on last screen origin, not past it. */
        vid->cam_x = 3 * R01E_SCREEN_PX_W;
        vid->cam_y = 3 * R01E_SCREEN_PX_H;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != R01E_SCREEN_PX_W || vid->l0_cam_y != R01E_SCREEN_PX_H) {
            r01e_machine_shutdown(&m);
            return fail("BG0 end-aligned parallax");
        }
        /* Equal extents: BG0 parked. */
        vid->l1_cols = 2;
        vid->l1_rows = 2;
        vid->cam_x = R01E_SCREEN_PX_W;
        vid->cam_y = R01E_SCREEN_PX_H;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 0 || vid->l0_cam_y != 0) {
            r01e_machine_shutdown(&m);
            return fail("BG0 parked when equal extent");
        }
        /* Wrap on: 2x2 under 4x4 is period 1/2, not end-aligned 1/3. */
        vid->bg0_wrap_x = 1;
        vid->bg0_wrap_y = 1;
        vid->l1_cols = 4;
        vid->l1_rows = 4;
        vid->cam_x = R01E_SCREEN_PX_W;
        vid->cam_y = R01E_SCREEN_PX_H;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != R01E_SCREEN_PX_W / 2 || vid->l0_cam_y != R01E_SCREEN_PX_H / 2) {
            r01e_machine_shutdown(&m);
            return fail("BG0 wrap uses period n/n");
        }
        vid->bg0_wrap_x = 0;
        vid->bg0_wrap_y = 0;
        /* 8 vs 16 cols, wrap off: 7/15 nearest. */
        vid->bg0_cols = 8;
        vid->bg0_rows = 1;
        vid->l1_cols = 16;
        vid->l1_rows = 2;
        vid->cam_y = 0;
        vid->cam_x = 0;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 0) {
            r01e_machine_shutdown(&m);
            return fail("BG0 8/16 at origin");
        }
        vid->cam_x = 1;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 0) {
            r01e_machine_shutdown(&m);
            return fail("BG0 walk holds 1 frame");
        }
        vid->cam_x = 2;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 1) {
            r01e_machine_shutdown(&m);
            return fail("BG0 walk 1 px / 2 frames");
        }
        vid->cam_x = 4;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 2) {
            r01e_machine_shutdown(&m);
            return fail("BG0 run 1 px / frame");
        }
        /* Wrap on: 8/16 is exact 1/2, including the old 7/15 hold at cam 16. */
        vid->bg0_wrap_x = 1;
        {
            int step;
            for (step = 0; step <= 16; step++) {
                vid->cam_x = step * 2;
                r01e_video_update_bg0_scroll(&m);
                if (vid->l0_cam_x != step) {
                    r01e_machine_shutdown(&m);
                    return fail("BG0 wrap 8/16 run 1 px / frame");
                }
            }
        }
        vid->cam_x = 1;
        r01e_video_update_bg0_scroll(&m);
        if (vid->l0_cam_x != 0) {
            r01e_machine_shutdown(&m);
            return fail("BG0 wrap 8/16 walk holds 1 frame");
        }
        vid->bg0_wrap_x = 0;
    }

    printf("ok io scroll/vram/map/fe80/eeprom/oam/apu\n");
    r01e_machine_shutdown(&m);
    return 0;
}

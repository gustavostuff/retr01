#include "retr01_sim/cart_slot.h"
#include "retr01_sim/flasher_bench.h"
#include "r01_flash_proto.h"
#include "sst39sf040.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

static void send_frame(R01sUsbcReceptacle *usb, uint8_t cmd, const uint8_t *payload, uint16_t len) {
    int i;
    r01s_usbc_host_send_byte(usb, R01F_MAGIC0);
    r01s_usbc_host_send_byte(usb, R01F_MAGIC1);
    r01s_usbc_host_send_byte(usb, cmd);
    r01s_usbc_host_send_byte(usb, (uint8_t)(len & 0xFFu));
    r01s_usbc_host_send_byte(usb, (uint8_t)(len >> 8));
    r01s_usbc_host_send_byte(usb, 0);
    for (i = 0; i < len; i++) {
        r01s_usbc_host_send_byte(usb, payload[i]);
    }
}

static void pump_mcu(R01sAtmega32u4 *mcu) {
    int i;
    for (i = 0; i < 64; i++) {
        if (r01s_atmega32u4_service(mcu, 65536) <= 0 && !r01s_atmega32u4_busy(mcu)) {
            break;
        }
    }
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : R01_TEST_CART;
    R01sFlasherBench bench;
    R01sSst39sf040 *flash;
    uint8_t hdr[6];
    size_t off;
    FILE *f;
    long sz;

    expect_true(r01s_flasher_bench_build(&bench) == 0, "build");
    expect_true(r01s_flasher_bench_load_rom(&bench, path) == 0, "load rom");
    expect_true(r01s_flasher_bench_insert_cart_flasher(&bench) == 0, "insert cart");
    r01s_usbc_set_vbus(&bench.usb, 1);
    r01s_atmega32u4_reset_program(&bench.mcu);

    send_frame(&bench.usb, R01F_CMD_ERASE, NULL, 0);
    pump_mcu(&bench.mcu);

    for (off = 0; off < bench.pc.rom_len; off += R01F_MAX_DATA) {
        uint16_t chunk = (uint16_t)((bench.pc.rom_len - off) > R01F_MAX_DATA ? R01F_MAX_DATA : (bench.pc.rom_len - off));
        send_frame(&bench.usb, R01F_CMD_DATA, bench.pc.rom + off, chunk);
        pump_mcu(&bench.mcu);
    }
    send_frame(&bench.usb, R01F_CMD_DONE, NULL, 0);
    pump_mcu(&bench.mcu);

    expect_true(r01s_atmega32u4_bytes_programmed(&bench.mcu) == bench.pc.rom_len, "all bytes");

    flash = r01s_cart_slot_flash(&bench.cart_slot, R01S_CART_SLOT_FLASHER);
    expect_true(flash != NULL, "flash ptr");
    memcpy(hdr, &flash->mem[0], 6);
    expect_true(memcmp(hdr, "retr01", 6) == 0, "cart magic in flash");

    /* pc_host stream path (ACK-gated) on a tiny image. */
    f = fopen(path, "rb");
    expect_true(f != NULL, "open cart");
    if (fseek(f, 0, SEEK_END) == 0 && (sz = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
        uint8_t tiny[64];
        size_t n = (size_t)sz > sizeof(tiny) ? sizeof(tiny) : (size_t)sz;
        if (fread(tiny, 1, n, f) == n) {
            char tmp_path[] = "/tmp/r01_flash_tiny.bin";
            FILE *wf = fopen(tmp_path, "wb");
            if (wf) {
                fwrite(tiny, 1, n, wf);
                fclose(wf);
                memset(&bench.cart.flash.mem, 0xFF, sizeof(bench.cart.flash.mem));
                r01s_atmega32u4_reset_program(&bench.mcu);
                expect_true(r01s_flasher_bench_load_rom(&bench, tmp_path) == 0, "load tiny");
                expect_true(r01s_flasher_bench_flash_rom(&bench) == 0, "pc stream start");
                expect_true(r01s_flasher_bench_run_until_done(&bench, 500000) == 0, "pc stream done");
                expect_true(r01s_flasher_bench_bytes_programmed(&bench) == n, "pc stream bytes");
            }
        }
    }
    if (f) {
        fclose(f);
    }

    expect_true(r01s_flasher_bench_remove_cart_flasher(&bench) == 0, "remove cart");
    expect_true(r01s_flasher_bench_flash_rom(&bench) != 0, "flash without cart fails");

    r01s_flasher_bench_shutdown(&bench);
    return test_done("test_cart_flash_flow");
}

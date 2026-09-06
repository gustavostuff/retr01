/*
 * ATmega1284P soft-$FExx stub.
 * Host toolchain: avr-gcc -mmcu=atmega1284p -Os soft_fexx.c main.c -o mcu1284.elf
 * Pin ISR wiring TBD when SEL demux is frozen (CPU_A recommended).
 */

#include "soft_fexx.h"

static R01SoftFexx g_soft;

/* Placeholders: replace with pin-change ISR sampling DQ + demuxed port. */
static void soft_poll_stub(void) {
    (void)g_soft;
}

int main(void) {
    r01_soft_fexx_init(&g_soft);
    for (;;) {
        soft_poll_stub();
    }
    return 0;
}

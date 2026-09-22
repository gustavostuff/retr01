/* Retr01 32 KB PRG. Boot $8000. Tables $80E0-$C7FF. C $C800-$FFF9. Vectors $FFFA. */

__rc0 = 0xe0;
INCLUDE imag-regs.ld
ASSERT(__rc31 == 0xff, "Inconsistent zero page map.")

MEMORY {
  zp : ORIGIN = 0x00, LENGTH = 0xe0
  ram : ORIGIN = 0x0300, LENGTH = 0x7F00 - 0x0300
  rom_boot : ORIGIN = 0x8000, LENGTH = 0x00E0
  rom_tab : ORIGIN = 0x80E0, LENGTH = 0xC800 - 0x80E0
  rom : ORIGIN = 0xC800, LENGTH = 0xFFFA - 0xC800
}

PROVIDE(irq = _irq_default);
PROVIDE(nmi = _irq_default);

REGION_ALIAS("c_readonly", rom)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
  .boot 0x8000 : { KEEP(*(.boot)) } > rom_boot
  .r01_bootmap 0x80E0 : { KEEP(*(.r01_bootmap)) } > rom_tab
  INCLUDE c.ld
}

__stack = ORIGIN(ram) + LENGTH(ram);

OUTPUT_FORMAT {
  FULL(rom_boot)
  FULL(rom_tab)
  FULL(rom)
  SHORT(nmi)
  SHORT(__do_reset)
  SHORT(_irqbrk)
}

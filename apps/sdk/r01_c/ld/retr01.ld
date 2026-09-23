/* Retr01 32 KB PRG. Boot $8000. Tables $80E0-$C3FF. C $C400-$FFF9. Vectors $FFFA. */
/* Studio bins KEEP-placed: R01P $80F0, play $8100, world dir $8500, solids $8700, extra play $8800. */
/* Solid list is the bank+tile patterns only. Collision samples MAP. Link fails if tables hit $C400. */

/* Imaginary regs $E0-$FF. nmi.s stacks this block around C NMI. */
__rc0 = 0xe0;
INCLUDE imag-regs.ld
ASSERT(__rc31 == 0xff, "Inconsistent zero page map.")

MEMORY {
  zp : ORIGIN = 0x00, LENGTH = 0xe0
  ram : ORIGIN = 0x0300, LENGTH = 0x7F00 - 0x0300
  rom_boot : ORIGIN = 0x8000, LENGTH = 0x00E0
  rom_tab : ORIGIN = 0x80E0, LENGTH = 0xC400 - 0x80E0
  rom : ORIGIN = 0xC400, LENGTH = 0xFFFA - 0xC400
}

PROVIDE(irq = _irq_default);
PROVIDE(nmi = _irq_default);

REGION_ALIAS("c_readonly", rom)
REGION_ALIAS("c_writeable", ram)

SECTIONS {
  .boot 0x8000 : { KEEP(*(.boot)) } > rom_boot
  .r01_bootmap 0x80E0 : { KEEP(*(.r01_bootmap)) } > rom_tab
  .r01_r01p 0x80F0 : { KEEP(*(.r01_r01p)) } > rom_tab
  .r01_play 0x8100 : { KEEP(*(.r01_play)) } > rom_tab
  .r01_worlddir 0x8500 : { KEEP(*(.r01_worlddir)) } > rom_tab
  .r01_solids 0x8700 : { KEEP(*(.r01_solids)) } > rom_tab
  ASSERT(SIZEOF(.r01_solids) <= 0x100, "solid list hits $8800")
  .r01_wplay 0x8800 : { KEEP(*(.r01_wplay)) } > rom_tab
  ASSERT(. <= 0xC400, "PRG tables hit $C400")
  ASSERT(SIZEOF(.r01_r01p) == 16, "R01P must be 16 B")
  ASSERT(SIZEOF(.r01_play) == 0x400, "play table must be 1024 B")
  ASSERT(SIZEOF(.r01_worlddir) == 16, "world dir must be 16 B")
  .text.nmi : { KEEP(*(.text.nmi)) } > rom
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

#ifndef R01_HW_REGS_H
#define R01_HW_REGS_H

#include <stdint.h>

/* CPU soft I/O map ($7F00-$7FFF). See docs/memory.md and docs/hardware.md.
 * Shared by emu, Studio PRG export, and game asm. */

#define R01_PORT_PPUCTRL 0x7F00u
#define R01_PORT_PPUSTATUS 0x7F01u
#define R01_PORT_SCROLL_X 0x7F02u
#define R01_PORT_SCROLL_Y 0x7F03u
#define R01_PORT_RASTER_Y 0x7F04u
#define R01_PORT_RASTER_CTRL 0x7F05u
#define R01_PORT_BG0_SCROLL_X 0x7F06u
#define R01_PORT_BG0_SCROLL_Y 0x7F07u
#define R01_PORT_PAL_ROW 0x7F08u
#define R01_PORT_PAL_DATA 0x7F09u
#define R01_PORT_VRAM_ADDR_LO 0x7F10u
#define R01_PORT_VRAM_ADDR_HI 0x7F11u
#define R01_PORT_VRAM_DATA 0x7F12u
#define R01_PORT_OAM_ADDR 0x7F20u
#define R01_PORT_OAM_DATA 0x7F21u
#define R01_PORT_CARTEE_CMD 0x7F22u
#define R01_PORT_CARTEE_ADDR 0x7F23u
#define R01_PORT_CARTEE_DATA 0x7F24u
#define R01_PORT_WORLD 0x7F30u
#define R01_PORT_PAD0 0x7F60u
#define R01_PORT_PAD1 0x7F61u
#define R01_PORT_MEEPROM_AL 0x7F70u
#define R01_PORT_MEEPROM_AH 0x7F71u
#define R01_PORT_MEEPROM_DATA 0x7F72u
#define R01_PORT_MAP_LO 0x7F90u
#define R01_PORT_MAP_MID 0x7F91u
#define R01_PORT_MAP_HI 0x7F92u
#define R01_PORT_MAP_DATA 0x7F93u

/* PPUCTRL ($7F00) -- docs/video-graphics.md */
#define R01_PPUCTRL_L1_EN 0x01u
#define R01_PPUCTRL_L0_EN 0x02u
#define R01_PPUCTRL_SPR_EN 0x04u
#define R01_PPUCTRL_L0_CAM_SHIFT 3u
#define R01_PPUCTRL_L0_CAM_MASK 0x18u
#define R01_PPUCTRL_L1_CAM_SHIFT 5u
#define R01_PPUCTRL_L1_CAM_MASK 0x60u
#define R01_PPUCTRL_NMI_EN 0x80u
#define R01_PPUCTRL_BOOT (R01_PPUCTRL_L1_EN | R01_PPUCTRL_L0_EN | R01_PPUCTRL_SPR_EN)

#define R01_PPUSTATUS_VBLANK 0x80u
#define R01_PPUSTATUS_HIT 0x40u

/* Cart save mailbox (24C64, 8 KB). */
#define R01_CARTEE_CMD_READ 0x80u
#define R01_CARTEE_CMD_WRITE 0x40u
#define R01_CARTEE_BYTES 8192u

/* Machine EEPROM (MCU-M internal, 512 B). 9-bit address via $7F70/$7F71. */
#define R01_MEEPROM_BYTES 512u

/* W65C02S PHI2 (docs/hardware.md). RDY hold stubs are CPU wait-states. */
#define R01_CPU_HZ 8000000u
#define R01_MS_TO_CPU_CYCLES(ms) ((uint32_t)(ms) * (R01_CPU_HZ / 1000u))
#define R01_US_TO_CPU_CYCLES(us) ((uint32_t)(us) * (R01_CPU_HZ / 1000000u))

/*
 * CPU_RDY holds after $7F24 / $7F72 (bring-up cycle stubs until real ACK/NVM).
 * Cart write ~tWC 5 ms (24LC64-class). Machine EEERWR typ ~10 ms (AVR128DB28).
 * Reads are short I2C / NVM access stubs.
 */
#define R01_RDY_CARTEE_WRITE_MS 5u
#define R01_RDY_CARTEE_WRITE_HOLDS R01_MS_TO_CPU_CYCLES(R01_RDY_CARTEE_WRITE_MS) /* 40000 */
#define R01_RDY_CARTEE_READ_HOLDS R01_US_TO_CPU_CYCLES(200u)                      /* 1600 */

#define R01_RDY_MEEPROM_WRITE_MS 10u
#define R01_RDY_MEEPROM_WRITE_HOLDS R01_MS_TO_CPU_CYCLES(R01_RDY_MEEPROM_WRITE_MS) /* 80000 */
#define R01_RDY_MEEPROM_READ_HOLDS R01_US_TO_CPU_CYCLES(50u)                       /* 400 */

/* Legacy alias: cart write (worst common game path). Prefer split holds. */
#define R01_RDY_EE_HOLDS R01_RDY_CARTEE_WRITE_HOLDS

#endif

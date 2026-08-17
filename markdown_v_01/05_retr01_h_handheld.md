# Retr01-H — Handheld Edition

Portable SMD expression of Retr01 with architectural parity (same map, VRAM model, CHR-from-cart, 2bpp).

**Status:** Later; do not block arcade bring-up.

## Packaging

W65C02S in dense package; SMD SRAM/logic; 4–6 layer PCB. Logical decode may move from GAL22V10 to denser CPLD — **same** CPU map.

## Power

Static-core clock halt; Li-Po + USB-C PMIC.

## Display

Raw LCD/OLED; nearest-neighbor from 256×240.

## Shared software model

Same cart rules and [08_memory_map.md](08_memory_map.md); thin platform layer for buttons/sleep/brightness.

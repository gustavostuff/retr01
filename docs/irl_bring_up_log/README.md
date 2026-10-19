# Bench authority

These notes are the pin netlist and the function of the chips they cover. A later motherboard still uses these same burned images. Extra jobs (scroll registers, raster IRQ) are absent from the images that passed.

## Color PROM

[bench-at27c256r.md](bench-at27c256r.md). One AT27C256R. The video index is A5 through A0, pins 5 through 10. The packed byte is O7 through O0 on pins 19, 18, 17, 16, 15, 13, 12, and 11. The check table is the byte in the chip for that row.

The sim `at27c256r.c` uses those pin numbers. The kit load packs the same RGB table. Rows 0, 1, 4, 12, 16, 32, 48, and 63 match the bench bytes (`00`, `20`, `24`, `01`, `25`, `92`, `FF`, `F3`).

## Beam X and Beam Y

[bench-beam-xy.md](bench-beam-xy.md). The images are [`beam_x.pld`](../../apps/flasher_tool/firmware/a/beam_x.pld) and [`beam_y.pld`](../../apps/flasher_tool/firmware/a/beam_y.pld).

The two chips share RES (pin 2 to pin 2) and power. One signal runs between them: Beam X pin 14 (HSYNC) into Beam Y pin 1 (CLK). Pins 3 through 11 and pin 13 on each chip are unused inputs. On the bench they sit on the ground rail. The Q pins are that chip's own count. They leave toward the LEDs on the bench, and toward the compositor and the VRAM mux on the motherboard. They do not run to the other PLD.

| Chip | CLK | Count | Sync |
| --- | --- | --- | --- |
| Beam X | DOT, pin 1, rising edge | 0 through 340, then 0 | HSYNC high on 264 through 288, pin 14 |
| Beam Y | Beam X HSYNC, pin 1, rising edge | 0 through 261, then 0 | VSYNC high on 243 through 245, pin 14 |

Q pin numbers, from the fuse-map header and the bench:

| Bit | Beam X pin | Beam Y pin |
| --- | --- | --- |
| Q0 | 21 | 23 |
| Q1 | 23 | 21 |
| Q2 | 16 | 15 |
| Q3 | 15 | 16 |
| Q4 | 17 | 20 |
| Q5 | 20 | 17 |
| Q6 | 19 | 19 |
| Q7 | 18 | 18 |
| Q8 | 22 | 22 |

### Sim

The sim follows this pin list. UPLDX counts dots and drives HSYNC. UPLDY counts on that HSYNC and drives VSYNC. The canvas joins them on HSYNC, and both RES pins share the CPU reset net. The video path reads the two counters. HBlank is X at least 256. VBlank is Y at least 240. NMIB is held low for a few dots when Y enters 240. Those three are not pins on the burned images.

The SKiDL export wires the same pins. UPLDX pin 1 is the DOT clock after R13. Both pin 2s share CPU reset. UPLDX pin 14 and UPLDY pin 1 share HSYNC. UPLDY pin 14 is VSYNC. Each Q pin is its own net (`BEAM_X_Q0` through `BEAM_X_Q8`, and the same for Beam Y). Pins 3 through 11 and pin 13 are tied to ground. Re-import `apps/sim/skidl/retr01_prelim.net` to replace the eight `SIG_UPLDX_P*` nets still stored in the older `.kicad_pcb` files.

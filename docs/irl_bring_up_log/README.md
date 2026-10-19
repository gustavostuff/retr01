# Bench authority

These notes are the pin netlist and the function of the chips they cover. A later motherboard still uses these same burned images. Extra jobs (scroll registers, raster IRQ) are absent from the images that passed.

## Color PROM

[bench-at27c256r.md](bench-at27c256r.md). One AT27C256R. The video index is A5 through A0, pins 5 through 10. The packed byte is O7 through O0 on pins 19, 18, 17, 16, 15, 13, 12, and 11. The check table is the byte in the chip for that row.

The sim `at27c256r.c` uses those pin numbers. The kit load packs the same RGB table. Rows 0, 1, 4, 12, 16, 32, 48, and 63 match the bench bytes (`00`, `20`, `24`, `01`, `25`, `92`, `FF`, `F3`).

## Beam X and Beam Y

[bench-beam-xy.md](bench-beam-xy.md). The images are [`beam_x.pld`](../../apps/flasher_tool/firmware/a/beam_x.pld) and [`beam_y.pld`](../../apps/flasher_tool/firmware/a/beam_y.pld).

The two chips share RES (pin 2 to pin 2) and power. One signal runs between them: Beam X pin 23 (HSYNC) into Beam Y pin 1 (CLK). Pins 3 through 11 and pin 13 on each chip are unused inputs. On the bench they sit on the ground rail. The Q pins are that chip's own count. They leave toward the LEDs on the bench, and toward the compositor and the VRAM mux on the motherboard. They do not run to the other PLD.

| Chip | CLK | Count | Sync |
| --- | --- | --- | --- |
| Beam X | DOT, pin 1, rising edge | 0 through 340, then 0 | HSYNC high on 264 through 288, pin 23 |
| Beam Y | Beam X HSYNC, pin 1, rising edge | 0 through 261, then 0 | VSYNC high on 243 through 245, pin 23 |

Both counters are 9 bits. The same pin on each chip is the same bit. Q0 is pin 14. Each next bit is the next pin. Q8 is pin 22. Pin 23 is that chip's sync pulse.

| Bit | Pin |
| --- | --- |
| Q0 | 14 |
| Q1 | 15 |
| Q2 | 16 |
| Q3 | 17 |
| Q4 | 18 |
| Q5 | 19 |
| Q6 | 20 |
| Q7 | 21 |
| Q8 | 22 |

### Sim

The sim follows this pin list. UPLDX counts dots and drives HSYNC. UPLDY counts on that HSYNC and drives VSYNC. The canvas joins them on HSYNC, and both RES pins share the CPU reset net. The video path reads the two counters. HBlank is X at least 256. VBlank is Y at least 240. NMIB is held low for a few dots when Y enters 240. Those three are not pins on the burned images.

The SKiDL export wires the same pins. UPLDX pin 1 is the DOT clock after R13. Both pin 2s share CPU reset. UPLDX pin 23 and UPLDY pin 1 share HSYNC. UPLDY pin 23 is VSYNC. Q0 through Q8 are pins 14 through 22 on each chip (`BEAM_X_Q0` through `BEAM_X_Q8`, and the same for Beam Y). Pins 3 through 11 and pin 13 are tied to ground. Re-import `apps/sim/skidl/retr01_prelim.net` after the next export. The saved `v_07` board still has the earlier pin-14 sync until that import.

## Color bars

[bench-color-bars.md](bench-color-bars.md). Not passed yet. Dot clock, three Beam X bits into the PROM, the resistor DAC, and one sync wire into an RGBS CRT. The compositor stays off that board.

## After a bench passes

A passing note here is what the board is updated from. The sim, the SKiDL netlist, and `v_07` follow the note. The note does not follow a drawing that has not been on the bench.

When a note says it passes:

1. Change the sim pin netlist so the canvas matches the bench.
2. Run `apps/sim/skidl/export_netlist.sh`. That writes `apps/sim/skidl/retr01_prelim.net` from the sim.
3. Re-import that file into `apps/sim/kicad/main-pcb/v_07.kicad_pcb`. Link footprints by reference designator. Leave **Replace footprints** and **Delete footprints** off.

The ordered board is that netlist after the benches that feed it have passed. A jumper the note calls breadboard-only stays off `v_07`.

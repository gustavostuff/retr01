# Retr01-A Simulation and Bring-up Plan

Stop using schematic AIs (Celus, Protoflow, and similar) as the source of truth. They can guess nets. They cannot freeze every resistor, capacitor, crystal load, and DAC ladder, and they will silently swap DIP 74HC for SMD 74LV.

This file is the build sequence. How the parts fit together: [05](05_how_the_machine_works.md). Architecture/chip count: [14](14_reduced_number_of_chips.md). CPU map: [08](08_memory_map.md). The schematic prompt ([15](15_schematic_prompt_coprocessor.txt)) waits until a simulator circuit is known-good.

No single free program simulates a 49-chip 5 V arcade board with analog RGBS, two AVR firmwares, a 6502, **and** every 100 nF cap. Split the work:

| Job | Program | What you get |
|-----|---------|----------------|
| See every logic chip wired, clocks, SRAM, pads, digital video | **[Digital](https://github.com/hneemann/Digital)** (HNeemann) | 74HC library, custom CPU/AVR/GAL, switches, 256×240 view |
| Guest ROM, CHR cells, MAP, NMI loop | C emulator (rewrite later, [07](07_emulator_specification.md)) | Fast, not gate-level |
| 328P APU firmware alone | **simavr** or **Wokwi** | PWM / timer audio, not the 6502 bus |
| 1284 sprite firmware alone | **simavr** or **Wokwi** | OAM eval + line-buffer fill, not analog video |
| Reset RC, R-2R DAC, regulator | Tiny **Falstad** or paper + this file | Analog only. Do not put the whole machine in SPICE |
| Real RGBS / CSYNC | Bench + RGBS monitor | After Digital says the digital timing is sane |

**Primary virtual bench is Digital.** Treat 07 as the software bench. Do not start a PCB until Digital stage 3 (below) shows a stable picture and `$FE60` / `$FE61` bits.

Tool notes: [10_hardware_simulators.md](10_hardware_simulators.md).

---

## 0. Rules for every stage

- DIP **74HC** at 5 V. Never 74LV, 74HCT, 74LS, VQFN, SOT-363, or ATF1504.
- CPU is **W65C02S**. Clock pin is **PHI2**. Pin 36 **BE** high. Pin 1 **VPB** NC. Pin 5 **MLB** NC. Do not use a MOS 6502 pinout.
- Three `AS6C62256`. OAM is inside the 1284, not SRAM.
- Pads: two bytes `$FE60` / `$FE61`. Bits 0 Dpad Right, 1 Dpad Left, 2 Dpad Down, 3 Dpad Up, 4 X, 5 Y, 6 Coin (Select on console), 7 Start. 1 = pressed.
- Passives below are **v0 defaults**. Change them only by editing this file.

---

## 1. Passives (do not ask an AI)

These are the analog parts a schematic bot will invent badly. Use them in Falstad, then copy onto the real proto.

### Power

- Barrel **5.5 × 2.1 mm**, center positive, **5 V**.
- If the barrel is already 5 V: no regulator IC. 100 µF electrolytic + 100 nF ceramic at the jack.
- If you feed 7–12 V: **7805** (TO-220) or a 5 V module. Input 10 µF, output 10 µF + 100 nF.
- Polyfuse (~500 mA–1 A) in series. Reverse diode across the 5 V rail (cathode to +5).
- **100 nF** ceramic from VCC to GND **at every IC** (as close as the breadboard allows).
- LED + 1 kΩ from +5 to GND as a power-on marker.

### CPU reset (`RESB`)

- 10 kΩ pull-up from `RESB` to +5.
- 10 µF from `RESB` to GND (slow power-on reset).
- Buffer through **SN74HC14N** (Schmitt). CPU `RESB` is the HC14 output, not the raw RC node.
- Manual reset: momentary button from the RC node to GND.

### CPU straps

- `RDY`, `BE`, `SOB`: 3.3 kΩ to +5.
- `NMIB`, `IRQB`: 3.3 kΩ to +5 (open-drain / GAL pull-up).
- `VPB`, `MLB`: no connect.

### Pads

- Each switch: 10 kΩ pull-up to +5, switch to GND (active low). Invert so the CPU byte is 1 = pressed.

### Clocks (real silicon)

- 8.000 MHz canned oscillator → PHI2. 100 nF on its VCC.
- 21.47727 MHz (or 21.477 MHz) canned oscillator → divide-by-4 in 74HC → ~5.369 MHz dot. 100 nF on its VCC.
- 16 MHz HC-49 + two 22 pF to GND for the 328P.
- 20 MHz HC-49 + two 22 pF to GND for the 1284.

In **Digital**, use ideal clock components at 8 MHz and ~5.369 MHz. Do not model crystal load caps there.

### Video DAC (real silicon only)

- 6-bit R-2R per gun (R = 1 kΩ, 2R = 2 kΩ is a fine start), then **75 Ω** series toward the jack.
- Target ~0.7 V into 75 Ω. Tune on the bench ([OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) B3).
- **CSYNC negative.** Do not SPICE the whole PPU to get this ladder.

### AVR ISP

- 6-pin ICSP: MISO, MOSI, SCK, RESET, VCC, GND. 10 kΩ on RESET to +5.

---

## 2. Digital: what to model how

Build **one `.dig` circuit**, grow it. Save a copy at the end of each stage.

| Silicon | In Digital |
|---------|------------|
| W65C02S | Custom CPU component (or a 6502 core you retarget to PHI2 / BE / RESB). Cycle-accurate enough to run a NOP loop and `STA $FExx`. |
| AS6C62256 ×3 | RAM components, 32 KB. Line buffer: only 512 bytes used. |
| ATF22V10 ×3 | Truth table / LUT / AND-OR. Names GAL-DEC, GAL-TIM, GAL-PPU. |
| 74HC161 / 157 / 245 / 573 / 00 / 04 / 14 / 08 / 32 / 86 / 688 | Built-in 74xx. Use **HC** parts. |
| ATmega1284P | Stage 3: a **behavioral box** (Java/Verilog inside Digital, or a ROM that writes the next line buffer). Full AVR core is optional and slow. |
| ATmega328P | **Out of this circuit.** Own simavr project. |
| Cart PRG/CHR/MAP | ROM components. CHR on the PPU fetch bus. MAP only via `$FE90`. |
| Pads | 16 switches + invert. |
| Video | Beam X/Y + 6-bit color → graphics / LED-matrix widget (256×240). This is digital RGB, not analog RGBS. |
| Passives | Pull-ups as Digital “pull up” pins. Skip 100 nF. |

---

## 3. Stages (do not skip)

### Stage 1 — CPU island (Digital)

Wire: 8 MHz clock, W65C02S, HC14 reset, 32 KB system RAM `$0000–$7FFF`, GAL-DEC (`RAM_CS`, `IO_CS`, `PRG_OE`), a small PRG ROM `$8000` and `$FFFC` vector.

Pass: reset, fetch NOPs or `JMP $8000`, no bus fight.

### Stage 2 — I/O page + pads (Digital)

Add 573s / `$FE00` family stubs. Implement `$FE60` / `$FE61` reads from the 16 switches. A guest loop that reads P1 and stores it in RAM.

Pass: flipping Right sets bit 0 of `$FE60`.

### Stage 3 — BG PPU (Digital)

Add 4× 161 (341×262), 4× 157 VRAM mux, VRAM SRAM, PHI2 interleave, nametable fetch, 2bpp, raster `$FE05` / IRQ, NMI at line 240. Palette as a small LUT. Draw 256×240.

Pass: a baked nametable in VRAM shows tiles. NMI fires ~60 Hz in sim time (you can clock slower). Wrong-phase VRAM access is a probe / assert.

Do **not** add 16 sprite shifters.

### Stage 4 — Sprites as a box (Digital)

Third SRAM line buffer, 2× 157, compositor vs BG. 1284 is a box that, during the visible line, fills the **next ping-pong half** (max 16 sprites). Guest writes `$FE20` / `$FE21` into that box’s OAM.

Pass: one sprite appears one line late, same as silicon.

### Stage 5 — 1284 firmware (simavr / Wokwi)

Own project. Inputs: HBLANK, OAM write strobe, CHR bytes. Output: 256-byte next-line buffer. No 6502 in this project.

Pass: same sprite list as stage 4, same buffer bytes.

### Stage 6 — APU (simavr / Wokwi)

Own project. `$FE40–$FE5F` writes in, PWM / PCM out. Play a pulse. Merge to the 6502 bus only on real hardware (or later in the C emu).

### Stage 7 — C emulator rewrite

[07](07_emulator_specification.md). This is where ROMs and MAP RLE run at full speed. Digital stays the bus/PPU oracle.

### Stage 8 — KiCad by hand

Draw from the **working Digital circuit**, not from Celus CUBOs. Passives from section 1. CPU symbol: WDC DIP-40 or [Alarm-Siren `W65C02S_P`](https://github.com/Alarm-Siren/6502-kicad-library). Reject any 74LV / ATF15xx leftover.

### Stage 9 — Breadboard / proto PCB

CPU island first (stage 1 on copper), then pads, then BG video into a 15.7 kHz RGBS monitor. Sprites after the 1284 firmware from stage 5 exists. Island-by-island checks: [17](17_protoboard_test_plan.md).

---

## 4. What “done” looks like before a full board

- Digital stage 4: BG + one sprite + two pad bytes.
- simavr: 1284 fills a line buffer; 328P beeps.
- This file still matches the passives you soldered.
- Doc 15 is a later paste target, not the current workflow.

---

## 5. Explicitly out of scope until stage 8

- Celus / Protoflow BOM generation.
- HDMI, S-Video encoder ICs.
- Retr01-C 3-wire protocol ([OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) B5).
- Ordering a 4-layer PCB from an AI schematic.

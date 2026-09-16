# Retr01 hardware bring-up (tiers A-H)

Breadboard path from a video-only lab to full counted-BOM console behavior (CPU, cart, pads, audio). **Eight tiers.** A-C are video lab. D-H add one major subsystem each.

**Scope:** Late-phase hardware bring-up after Studio/Emu contracts and the design SoT are stable. Not day-one software work.

**Design SoT:** `general_docs/hardware.md`, `general_docs/video-graphics.md`, `general_docs/memory.md`, `general_docs/ic-comms-risks.md`, `general_docs/software-api.md`, plus `ic_behavior/`.

| Tier | Name | One-line goal | Detail |
| --- | --- | --- | --- |
| **A** | Beam + composite | Sync + color bars / solid color on a TV | [tier-a-video-lab.md](tier-a-video-lab.md) |
| **B** | Compositor | 6-bit kit index + layer priority via third PLD | [tier-b-video-lab.md](tier-b-video-lab.md) |
| **C** | S1 field fill | VBlank sprite field (+ optional BG0 HBlank lines) from S1 | [tier-c-video-lab.md](tier-c-video-lab.md) |
| **D** | M <-> S1 OAM path | MCU-M sends sprite table over SPI; S1 builds field from it | [tier-d-oam-spi.md](tier-d-oam-spi.md) |
| **E** | PHI2 + VRAM | Interleaved BG1 nametable path (HC157s + VRAM SRAM) | [tier-e-vram.md](tier-e-vram.md) |
| **F** | 6502 soft I/O | Game CPU runs test PRG; soft `$7Fxx` + scroll/OAM via M | [tier-f-cpu-soft-io.md](tier-f-cpu-soft-io.md) |
| **G** | Cart + MAP | Passive flash cart, PRG fetch, MAP stream, optional save IC | [tier-g-cart-map.md](tier-g-cart-map.md) |
| **H** | Pads + audio | MCU-S2: controllers + PWM audio; full counted BOM behavior | [tier-h-pads-audio.md](tier-h-pads-audio.md) |

---

## Feasibility (summary)

The ladder is **feasible** against the locked design:

1. **Risk isolation matches SoT.** Clocks, soft `$7Fxx`, VRAM interleave, S1 field ALE/`/WE`, cart OE, and pads are separate failure domains in `ic-comms-risks.md`. One domain per tier is the right shape.
2. **Video-first (A-C) is the right start.** Composite lock and Compositor priority do not need AVRs or a CPU. Field fill (C) is the first hard real-time AVR window and can be proven without M or the 6502.
3. **D before E/F is deliberate.** Separating "who owns OAM" (M) from "who paints the field" (S1) matches final SPI mailbox rules before PHI2 interleave and soft I/O complexity land.
4. **E before F is deliberate.** VRAM mux + PHI2-low beam fetch is a pure digital timing problem. A lab PHI2-high writer may stand in for `$7F10-$7F12` until the 6502 exists; the interleave itself must not wait on full soft I/O.
5. **G before H is deliberate.** Cart OE/MAP fights are orthogonal to pads/audio. S2 is the remaining AVR and is safe last.
6. **Hardest remaining risks (not blockers to the plan):** (a) MCU-M answering soft `$7Fxx` inside one PHI2 cycle without habitual `CPU_RDY`; (b) VRAM half-cycle margin through 3x HC157 + PLD vs 55 ns SRAM; (c) full sprite field + early-VBlank OAM SPI both fitting before active display. Those are measurement gates at F, E, and D/C - not reasons to reorder tiers.

---

## Tier sketches (A-H)

### A - Beam + composite

**Add:** DOT, Beam X/Y, PROM, DAC, AD724, FSC. 
**Prove:** Stable H/V (or CSYNC), kit colors, composite lock. 
**Skip:** All AVRs, CPU, cart, SRAM, 74xx mux/latches.

### B - Compositor

**Add:** Third ATF22V10. 
**Prove:** Index and priority come from Compositor; CPU/MAP pins stubbed safe. 
**Skip:** Still no AVRs / CPU / cart.

### C - S1 field fill

**Add:** MCU-S1, **one** field AS6C62256 (#3 role), 74HC573 for ALE, optional BG0 ping-pong **regions on that same chip**. 
**Prove:** VBlank field writes; simple BG + 1-2 small sprites from S1 firmware. 
**Skip:** M, 6502, cart, SPI OAM from outside S1. A second SRAM is not the design path for BG0 (temporary lab shortcut only if explicitly marked).

### D - MCU-M and OAM SPI

**Add:** MCU-M; SPI MOSI/MISO/SCK, `/SS_S1`, `VBL` into M, optional `S1_RDY`. 
**Prove:** Early VBlank (or `S1_RDY`): M transmits a small OAM-like table; S1 builds field from that table; no SPI in HBlank. 
**Exit:** Changing OAM data on M visibly changes sprites without reflashing S1 blitter logic.

### E - PHI2 and interleaved VRAM

**Add:** PHI2 **8.000 MHz** canned oscillator (series ~33 ohm); AS6C62256 as interleaved VRAM; 3x 74HC157 (G never floating). 
**Prove:** PHI2 high = lab writer or later CPU `$7F10-$7F12` fill; PHI2 low = beam BG1 fetch; island tests CPU-only, beam-only, then both. 
**Exit:** Stable BG1 from VRAM under live beam + S1 sprites on top.

### F - W65C02 and soft `$7Fxx`

**Add:** W65C02S @ PHI2; system RAM; M as soft `$7Fxx` device (Hi-Z on D unless selected; `CPU_RDY` OD only when needed); live `SEL_SOFT*` / `LE_7F02/03/04`; optional HC574. 
**PRG without cart:** temporary image in system RAM with PLD decode to `$8000-$FFFF`, or a socketed EPROM/flash substitute - not a mapper fantasy. 
**Exit:** 6502-authored motion of Tier C/D sprites and scroll; M/S1 still video assist only.

### G - Cart and MAP

**Add:** SST39SF040 (or lab equivalent); OE# gated for PRG and intentional MAP/CHR windows; MAP `$7F90-$7F93`; optional 24C64/FRAM (chunked, short RDY). 
**Exit:** `.retr01`-style image boots and shows authored BG + entities driven by PRG.

### H - Pads and audio

**Add:** MCU-S2; pad path (TRS OD UART and/or arcade GPIO); S2 PWM audio; SPI slave under M for pad/APU mailbox. 
**Exit:** Playable test cart with pads, audio, and composite out. Optional polish (light gun, FRAM preference, 4-layer spin) stays post-H.

---

## Cross-cutting rules (every tier)

1. **One new risk domain per tier.** Cart + pads + 6502 are not combined in one jump.
2. **PLDs / PROM are programmed before power-up** with new bus devices attached.
3. **Hi-Z and single-driver rules** on D and on S1 AD (`general_docs/ic-comms-risks.md`).
4. **Video windows stay sacred:** sprites in VBlank, BG0 line in HBlank, scroll/pal in NMI/VBlank, OAM SPI in early VBlank or when `S1_RDY` is ready.
5. **Canned CMOS oscillators** for PHI2/DOT. No Schmitt unless the scope says otherwise.
6. **Exit criteria are written before wiring.** Each tier ends with a boring, repeatable demo.

---

## Mapping to full BOM

| Function | First appears |
| --- | --- |
| Beam X/Y, Compositor, PROM, AD724 | A-B |
| S1 + field (+ HC573; BG0 ping-pong on field chip) | C |
| M + SPI | D |
| PHI2, VRAM, HC157s | E |
| W65C02, system RAM, soft I/O, HC574 | F |
| Cart flash, MAP, optional save EEPROM/FRAM | G |
| S2, pads, audio PWM | H |

Optional outside the 19: 74HC14, ATtiny85 in the pad, UPDI Friend as tool.

---

## Suggested demo ladder (acceptance)

| Tier | Boring demo |
| --- | --- |
| A | Solid color + color bars |
| B | Priority box over bars |
| C | One static + one animated small sprite from field SRAM |
| D | Same sprites; motion table updated by M over SPI |
| E | BG1 tile map from VRAM + sprites |
| F | 6502 moves player sprite / camera |
| G | Boot `.retr01` from cart |
| H | Play with real pad + sound |

When the Tier H demo is dull, the breadboard path has reached full console behavior with pads. PCB spin and software depth are separate tracks.

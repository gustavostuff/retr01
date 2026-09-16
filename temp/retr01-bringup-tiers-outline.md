# Retr01 breadboard → full console — tier outline

Baby steps from a video-only lab to a complete system (CPU, cart, pads, audio).  
**Eight tiers total (A–H).** A–C are video lab; D–H add the rest of the machine one concern at a time.

Design SoT: repo `general_docs/hardware.md`, `general_docs/video-graphics.md`, `general_docs/memory.md`, `general_docs/ic-comms-risks.md`, `general_docs/software-api.md`.

| Tier | Name | One-line goal |
| --- | --- | --- |
| **A** | Beam + composite | Sync + color bars / solid color on a TV |
| **B** | Compositor | 6-bit kit index + layer priority via third PLD |
| **C** | S1 field fill | VBlank sprite field (+ optional BG0 HBlank lines) from firmware |
| **D** | M ↔ S1 OAM path | MCU-M sends sprite table over SPI; S1 builds field from it |
| **E** | PHI2 + VRAM | Interleaved BG1 nametable path (HC157s + VRAM SRAM) |
| **F** | 6502 soft I/O | Game CPU runs test PRG; soft `$7Fxx` + scroll/OAM via M |
| **G** | Cart + MAP | Passive flash cart, PRG fetch, MAP stream, optional save IC |
| **H** | Pads + audio | MCU-S2: controllers + PWM audio; full counted BOM behavior |

Detailed lab notes for **A–C** live in separate docs. Below is the **outline only** for the path and for **D–H**.

---

## A — Beam + composite *(done / doc exists)*

**Add:** DOT, Beam X/Y, PROM, DAC, AD724, FSC.  
**Prove:** Stable H/V (or CSYNC), kit colors, composite lock.  
**Skip:** All AVRs, CPU, cart, SRAM, 74xx mux/latches.

---

## B — Compositor *(done / doc exists)*

**Add:** Third ATF22V10.  
**Prove:** Index and priority come from Compositor; CPU/MAP pins stubbed safe.  
**Skip:** Still no AVRs / CPU / cart.

---

## C — S1 field fill *(done / doc exists)*

**Add:** MCU-S1, field SRAM, optional HC573 / second SRAM for BG0 lines.  
**Prove:** VBlank field writes; simple BG + 1–2 small sprites from S1 firmware.  
**Skip:** M, 6502, cart, SPI OAM from outside S1.

---

## D — MCU-M and OAM SPI

**Why this next:** Separates “who decides sprites” (M) from “who paints the field” (S1), matching the final architecture, without a 6502 yet.

**Add**
- 1× AVR128DB28 as **MCU-M**
- SPI: MOSI/MISO/SCK, `/SS_S1`, `VBL` into M, optional `S1_RDY`

**Prove**
- Early VBlank (or `S1_RDY`): M transmits a small OAM-like table
- S1 builds field from that table only (firmware tables optional for boot)
- No SPI in HBlank; field fill still completes before active display

**Content:** Same simple sprites as C; M walks a frame/position table.

**Don’t:** Soft CPU data-bus driver on M yet; no cart; no full-bus `$7Fxx` decode.

**Exit:** Changing OAM data on M visibly changes sprites without reflashing S1 blitter logic.

---

## E — PHI2 and interleaved VRAM

**Why this next:** Real BG1 comes from nametable memory with the locked PHI2 interleave, still before a full game CPU.

**Add**
- PHI2 **8.000 MHz** canned oscillator (series ~33 Ω)
- 1× AS6C62256 as **interleaved VRAM**
- 3× 74HC157 (A mux CPU vs beam); HC157 **G** never floating
- Optional: tiny state machine on M (or a second test AVR) to write VRAM on PHI2-high only — **or** wait for F and let the 6502 do it

**Prove**
- PHI2 high: lab writer fills `$7F10–$7F12`-style VRAM window
- PHI2 low: beam fetches BG1; Compositor shows nametable art
- Island tests: CPU-only fill, beam-only fetch, then both

**Don’t:** Full soft `$7Fxx` product; cart OE fights; long RDY holds.

**Exit:** Stable BG1 from VRAM under live beam + S1 sprites on top.

---

## F — W65C02 and soft `$7Fxx`

**Why this next:** Game CPU and the hard real-time soft-I/O path are the riskiest digital slice; introduce them with **test PRG in RAM or a tiny ROM substitute**, not a full cart.

**Add**
- 1× W65C02S @ PHI2
- System RAM (AS6C62256 #1) for `$0000–$7EFF` (and/or a minimal boot image source)
- M as soft `$7Fxx` device: Hi-Z on D unless selected; **RDY** open-drain only when needed
- PLD decode stubs already in Compositor become live: `SEL_SOFT*`, `LE_7F02/03/04`, etc.
- Optional HC574 for scroll X if not hardwired

**Prove**
- Reset vectors, NMI from VBlank, tight loop writes scroll / OAM mailbox / palette row in VBlank only
- Common soft I/O **without** RDY on the hot path
- Scope: only one driver on D for `$0000`, `$7Fxx`, `$8000` (if any ROM)

**Don’t:** STP in play; mid-frame scroll/pal; SPI/I2C inside a soft-read cycle without RDY.

**Exit:** 6502-authored motion of the Tier C/D sprites and scroll; M/S1 still do video assist only.

---

## G — Cart and MAP

**Why this next:** Passive cartridge is the software delivery vehicle; add read path and optional save after the CPU is trustworthy.

**Add**
- SST39SF040 (or lab equivalent) on a cart-like footprint
- OE# gated only for PRG `$8000–$FFFF` and intentional MAP/CHR windows
- MAP port `$7F90–$7F93` (seek + auto-inc data) via M/PLD as designed
- Optional 24C64 or FRAM for saves (chunked, short RDY, UI alive — see `ic-comms-risks.md`)

**Prove**
- Boot from cart PRG; MAP streams a screen into VRAM; no OE fight with RAM/soft
- Flasher path (UPDI Friend + DIP select) for AVRs + cart as documented — can stay partial

**Don’t:** Mapper fantasies; WE# floating low; full-save freeze under one long RDY.

**Exit:** `.retr01`-style image boots and shows authored BG + entities driven by PRG.

---

## H — Pads and audio (full console behavior)

**Why last:** Input and sound are orthogonal to the video/CPU bring-up; S2 is the remaining AVR.

**Add**
- 1× AVR128DB28 as **MCU-S2**
- Pad path: TRS open-drain UART and/or arcade GPIO; poll in VBlank (`0x55` / `0xAA` pattern as designed)
- Audio: S2 PWM (TCA) toward the board’s audio out path
- SPI slave under M for pad mailbox / APU window as in the soft map

**Prove**
- P1/P2 bits affect PRG; timeout keeps last good or clears
- Host-like BGM/SFX replaced by real `$7F40`-class / S2 path at least for a beep and a pulse line
- Idle-safe pads (OD only on DATA); no push-pull fights

**Don’t:** Poll pads in a tight 6502 loop without VBlank discipline; block video windows with audio DMA fantasies on S1.

**Exit:** Counted motherboard set exercised end-to-end: playable test cart, pads, audio, composite out. Optional polish (light gun, FRAM preference, 4-layer spin) stays post-H.

---

## Cross-cutting rules (every tier)

1. **One new risk domain per tier** — don’t combine cart + pads + 6502 in one jump.  
2. **Program PLDs / PROM before power-up** with new bus devices attached.  
3. **Hi-Z and single-driver rules** on D and on S1 AD (`ic-comms-risks.md`).  
4. **Video windows stay sacred:** sprites in VBlank; BG0 line in HBlank; scroll/pal in NMI/VBlank.  
5. **Canned CMOS oscillators** for PHI2/DOT → no Schmitt unless the scope says otherwise.  
6. **Exit criteria written before wiring** — each tier ends with a boring, repeatable demo.

---

## Mapping to full BOM

| Function | First appears |
| --- | --- |
| Beam X/Y, Compositor, PROM, AD724 | A–B |
| S1 + field (+ optional BG0 SRAM, HC573) | C |
| M + SPI | D |
| PHI2, VRAM, HC157s | E |
| W65C02, system RAM, soft I/O, HC574 | F |
| Cart flash, MAP, optional save EEPROM/FRAM | G |
| S2, pads, audio PWM | H |

Optional outside the 19: 74HC14, ATtiny85 in the pad, UPDI Friend as tool.

---

## Suggested demo ladder (acceptance)

| Tier | “Boring” demo |
| --- | --- |
| A | Solid color + color bars |
| B | Priority box over bars |
| C | One static + one animated small sprite |
| D | Same sprites; motion table updated by M over SPI |
| E | BG1 tile map from VRAM + sprites |
| F | 6502 moves player sprite / camera |
| G | Boot `.retr01` from cart |
| H | Play with real pad + sound |

When H’s demo is dull, the breadboard path has reached **full console with pads and everything**; PCB spin and software depth are separate tracks.

---

*Eight tiers, one major subsystem each, from TV bars to playable cart.*

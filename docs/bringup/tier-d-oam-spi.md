# Retr01 Tier D - MCU-M OAM SPI path

**Prerequisite:** Tier C working (S1 fills field SRAM in VBlank, at least one clear sprite on RGBS). 
**Goal:** Separate "who decides sprites" (MCU-M) from "who paints the field" (MCU-S1), matching the final SPI mailbox architecture, still **without** a 6502 or cart.

**SoT:** `docs/general/hardware.md` (SPI mailbox rules), `docs/general/ic-comms-risks.md` (sec  OAM / VBlank), `docs/ic_behavior/AVR128DB28.md`.

---

## 1. Parts added

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **AVR128DB28** | **MCU-M** @ 24 MHz HFOSC |
| - | Wiring | SPI: MOSI/MISO/SCK, `/SS_S1`, `VBL` into M, optional `S1_RDY` |

Pin freeze (design intent):

| Role | Nets |
| --- | --- |
| M SPI1 master | PC0 MOSI, PC1 MISO, PC2 SCK, PC3 `/SS_S1` |
| S1 SPI1 slave ALT1 | PD4 MOSI, PD5 MISO, PD6 SCK, PD7 `/SS` |

Idle: `/SS_S1` **high**. Exactly one slave selected when traffic runs (S2 does not exist yet).

**Still omit:** W65C02, system/VRAM SRAMs as soft devices, cart, MCU-S2, soft `$7Fxx` CPU data-bus driver on M.

---

## 2. Why this tier

Tier C proved S1 can finish a field in VBlank. The product path does **not** keep OAM authorship on S1 forever - M (and later the 6502 via M) publishes an OAM block. S1 rasterizes.

Doing that **before** PHI2 interleave and soft I/O avoids conflating three hard problems.

---

## 3. Timing rules (locked)

| Work | Window |
| --- | --- |
| OAM SPI M->S1 | **Early VBlank**, or when **`S1_RDY`** says ready |
| Sprite field rebuild | Rest of VBlank on S1 after OAM is accepted |
| BG0 next line | **HBlank only** - **no** OAM SPI here |

- Do **not** blast OAM during HBlank (steals the BG0 line window).
- Prefer **dirty / delta** OAM once a full table works. Full-table every frame is a lab convenience, not the play-path ideal.
- Bound every transfer. Leave margin so S1 still finishes the field before active display.

---

## 4. Prove

1. On VBlank (or `S1_RDY`): M transmits a small OAM-like table (positions, tile ids, attrs).
2. S1 builds the field **from that table only** (firmware-local tables optional for boot/fallback only).
3. Scope: SPI activity clustered in early VBlank. `/WE` bursts still finish before active display.
4. Changing OAM data on M (or a host UART bridge into M) visibly moves/changes sprites **without** reflashing S1 blitter logic.

**Content:** Same simple sprites as Tier C. M walks a frame/position table.

---

## 5. Rules

### Required

- OAM is gated on early VBlank / `S1_RDY`.
- Payloads stay small and frame-bounded. Message ID/length differ so a mis-select fails closed.
- S1 AD hi-Z rules from Tier C stay in force.
- Scope order: OAM end -> field fill end -> first active line.

### Forbidden

- CPU D driven from M on this tier (soft `$7Fxx` is Tier F).
- Cart or I2C on this tier.
- SPI inside a future soft-read cycle without `CPU_RDY` (foreshadow only, no soft bus yet).
- Late-VBlank full OAM plus full field rebuild if the top of the frame glitches. Move OAM earlier or shrink work.

---

## 6. Exit criteria -> Tier E

- Sprites update from M-published OAM over SPI.
- No SPI in HBlank during the demo.
- Field fill still completes before active display with visible margin on the scope.

**Next:** [tier-e-vram.md](tier-e-vram.md) - PHI2 + interleaved VRAM + HC157s for real BG1 nametable fetch.

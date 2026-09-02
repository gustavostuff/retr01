# Lab 04: `$FExx` latches

**Goal:** Add the nine **SN74HC573** latches (U5A-U5I) so CPU writes to graphics/MAP control ports **stick** as stable bytes for later PLDs (scroll, raster compare, PPUCTRL, MAP seek, palette address).

**Pass:** Complete the **Success metrics** section (KiCad table required). Probing Q after `STA $FExx` is optional later.

**Depends on:** [Lab 03](03_decode_pld_and_bus.md) (`PERIPH_D*`, U3 strobes).

**Do not add yet:** VRAM SRAM, HC157 muxes, beam PLDs, Color PROM. Those **read** the Q nets later.

**Related:** [`../graphics.md`](../graphics.md#hc573-latch-map-9-chips). [`../../hw/md/SN74HC573.md`](../../hw/md/SN74HC573.md).

---

## 0. Ideas in plain language

A latch is a **byte of memory that is not in RAM**. The CPU writes it with `STA $FExx`. Hardware (beam, compositor) can read the Q pins every dot without stealing a CPU cycle.

Software analogy:

```text
volatile uint8_t scroll_x   // memory-mapped at $FE02
// hardware thread reads scroll_x continuously
```

There are **nine** packages, one byte each (not nine bits).

---

## 1. New sheet

1. Hierarchical sheet name: `Latches`
2. File: `sheets/04_latches.kicad_sch`
3. Import labels:

| Label | Direction |
|-------|-----------|
| `+5V` `GND` | power |
| `PERIPH_D0`..`PERIPH_D7` | from Lab 03 |
| `LE_FE00`, `LE_FE02`, ... | from U3 |
| `RWB` `PHI2` optional | if LE qualify needs them |

Export Q buses for later sheets: `PPUCTRL0`..`7`, `SCR1_X*`, etc.

---

## 2. Refdes and ports (lock this table)

From [`../graphics.md`](../graphics.md#hc573-latch-map-9-chips):

| Ref | CPU port | Q net prefix | Who cares later |
|-----|----------|--------------|-----------------|
| U5A | `$FE00` | `PPUCTRL` | beam NMI enable, layer enables |
| U5B | `$FE02` | `SCR1_X` | BG1 scroll X |
| U5C | `$FE03` | `SCR1_Y` | BG1 scroll Y |
| U5D | `$FE04` | `RAST_Y` | raster compare |
| U5E | `$FE05` | `RAST_CTL` | IRQ enables |
| U5F | `$FE08` | `PAL_ADDR` | palette index pointer |
| U5G | `$FE90` | `MAP_A0` | MAP seek low |
| U5H | `$FE91` | `MAP_A8` | MAP seek mid |
| U5I | `$FE92` | `MAP_A16` | MAP seek high |

**Not** in this lab: `$FE06`/`$FE07` BG0 scroll, `$FE10`-`$FE12` VRAM, OAM, APU. Those use other paths.

**Sim note:** Sim may still show VRAM addr on HC573 until freeze ([`../hardware.md`](../hardware.md#runners-today-vs-silicon-target)). This lab follows the **silicon** nine-chip map above.

---

## 3. One chip template (copy nine times)

SN74HC573 pinout: [`../../hw/md/SN74HC573.md`](../../hw/md/SN74HC573.md).

| Pin | Connect |
|-----|---------|
| 1 OE | `GND` (Q always driven toward PLDs) |
| 2-9 1D-8D | `PERIPH_D0`..`PERIPH_D7` (**same order on every latch**) |
| 11 LE | `LE_FExx` for that port |
| 19-12 1Q-8Q | `NAME0`..`NAME7` for that port |
| 20 VCC | `+5V` + **100 nF each chip** |
| 10 GND | `GND` |

Bit 0 of the byte is `PERIPH_D0` -> `1D` -> `1Q` -> `NAME0`. Keep that convention everywhere or debugging hurts.

### 3.1 LE timing intent

HC573: while LE is **high**, Q follows D. When LE goes **low**, Q holds.

Decode PLD should raise `LE_FExx` during a valid write cycle to that address (data stable on `PERIPH_D*`), then lower LE to capture. Match the spirit of Sim `wire_io` (pulse on store). Exact PHI2 edge belongs in U3 equations.

On the latch sheet, draw:

```text
U3 SEL/LE --> LE pin
Do not tie LE to +5V permanently (that makes a transparent buffer, not a hold latch)
```

---

## 4. Wire U3 strobes (from Lab 03)

For each port, U3 must decode the full address and RWB=0 (write):

| Port | Suggest net name into LE |
|------|--------------------------|
| `$FE00` | `LE_FE00` |
| `$FE02` | `LE_FE02` |
| `$FE03` | `LE_FE03` |
| `$FE04` | `LE_FE04` |
| `$FE05` | `LE_FE05` |
| `$FE08` | `LE_FE08` |
| `$FE90` | `LE_FE90` |
| `$FE91` | `LE_FE91` |
| `$FE92` | `LE_FE92` |

If U3 runs out of pins, combine with external demux later. Prefer one LE per latch for clarity on the first schematic.

Also ensure `$FExx` writes enable U4 (`CPU_245_OE#` low, DIR CPU->periph) so `PERIPH_D*` carries CPU data.

---

## 5. Layout tips on the sheet (readability)

You are not doing PCB layout yet. On the **schematic**:

1. Place U5A-U5I in a column.
2. Draw `PERIPH_D[7..0]` once as a bus into all D pins.
3. Place LE labels on the left of each chip.
4. Place Q buses on the right with **hierarchical labels** for later beam/video sheets.
5. One text block listing the table from section 2.

---

## 6. Success metrics

### In KiCad (required to pass Lab 04)

| # | Check | What you should see |
|---|--------|---------------------|
| 1 | Annotate | U5A .. U5I present (nine HC573). Names match the port table in section 2 |
| 2 | ERC | **0 errors**. Each U5x has VCC, GND, and a 100 nF nearby |
| 3 | Highlight `PERIPH_D0` | Reaches **1D on all nine** latches and U4. Spot-check `PERIPH_D7` |
| 4 | Highlight `LE_FE02` | From U3 (or hierarchical pin from decode sheet) to **U5B LE only** |
| 5 | Highlight `LE_FE00` | To **U5A LE only**. Repeat idea for one MAP latch (`LE_FE90` -> U5G) |
| 6 | Highlight `SCR1_X0` (or your Q0 name for `$FE02`) | From **U5B 1Q** to a hierarchical label leaving the sheet |
| 7 | Highlight `PPUCTRL0` | From **U5A 1Q** to hierarchical label |
| 8 | OE pins | All nine OE pins tied to `GND` (or documented PLD qualify). Not floating |
| 9 | Port table on sheet | Visible text block listing U5A-U5I <-> `$FExx` |
| 10 | No VRAM confusion | `$FE10`-`$FE12` **not** drawn as these nine latches (silicon map). Comment OK if Sim still differs |

**Lab 04 KiCad pass** = rows 1-10 all true.

### Optional later (hardware / Sim)

| Test | Expect |
|------|--------|
| `LDA #$10` / `STA $FE02` on bench with U3 JEDEC | `SCR1_X` Q pins show `$10` after LE falls |
| `STA $FE03` with `$20` | `SCR1_Y` Q shows `$20` |
| Sim island D | Nine HC573 group on canvas ([`../../retr01/sim/README.md`](../../retr01/sim/README.md)) |

You will not see Q values change inside KiCad itself.

---

## 7. Done / not done

**Done**

- Full silicon HC573 map on the schematic
- Stable Q nets ready for beam / MAP / palette consumers
- Decode LE contract named

**Not done**

- Consumers of Q (beam X/Y, compositor, MAP flash seek mux)
- VRAM `$FE10`-`$FE12` path
- BG0 scroll ports if they stay off this latch bank
- Programming U3 to actually pulse LE

---

## What you have built after Labs 01-04

```text
+5V / PHI2 / RESB
     |
   W65C02S ---- CPU_A/D ---- system RAM ($0000-$7FFF)
     |                 |
     |                 +---- cart flash PRG ($8000+)
     |
    U3 decode ---- LE_* ---- U5A-U5I latches
     |
    U4 HC245 ---- PERIPH_D ---- latch D inputs
```

That is island **A/C/D/J** in bring-up terms, without video paint yet.

---

## Next (not written yet)

Suggested Lab 05: VRAM + HC157 interleave (`$FE10`-`$FE12`).  
Suggested Lab 06: beam PLDs + IRQ/NMI.  

Return to [`README.md`](README.md) for the index.

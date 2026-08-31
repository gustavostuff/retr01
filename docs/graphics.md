# Retr01 Graphics

How the picture is built: VRAM workbench, BG fetch, sprites, palettes. Software-visible behavior lives here.

**Related:** [`memory.md`](memory.md) (cart layout, chip timing). [`hardware.md`](hardware.md) (BOM, beam path).

---

## Display

| Item | Value |
|------|-------|
| Logical playfield | **128x120** (**16x15** tiles). Games and Studio work here |
| RGBS active field | **256x240** inside **341x262** |
| Clocks | CPU **8.000 MHz**, dot **5.369318 MHz**, ~**60.098 Hz** |
| SCALE DIP | **2x** default (fills CRT). **1x** centers 128x120. Not a `$FExx` bit |

Games author and scroll in **128x120**. Hardware scales that rectangle into the RGBS field. Emulators draw the logical field (often presented 2x for pixels).

---

## Tiles (CHR)

| Item | Value |
|------|-------|
| Size | **8x8** pixels |
| Depth | **2 bpp** (color index **0-3** inside a 4-color palette) |
| Bytes | **16** per tile (NES-style bitplanes) |
| Banks | **4** BG + **4** sprite per world (**256** tiles each, **4 KB**/bank) |

Layout of one tile:

```text
 bytes 0-7   bitplane 0 (LSB of color index), one byte per row
 bytes 8-15  bitplane 1 (MSB of color index), one byte per row
 bit 7 = leftmost pixel
```

Color index **0** is transparent for sprites and shared backdrop for BG. Final RGB comes from active palette indices -> board Color PROM. CHR lives in **cart flash**, not CPU address space. Attr **BANK** bits pick which of the four banks supplies the tile.

---

## VRAM workbench

Eight live nametable slots in 32 KB VRAM. Each slot **512 B** (240 tile + 240 attr at `+0xF0`).

| Slots | Role |
|-------|------|
| **0-3** | L1 / BG1 camera field (**2x2**, main playfield) |
| **4-7** | L0 / BG0 camera field (**2x2**, structured second BG) |

Scroll `$FE02`/`$FE03` (L1): **0-127** / **0-119**. Scroll `$FE06`/`$FE07` (L0): same ranges for the far plane. Hardware does **not** auto-load MAP. Crossing a screen border = software streams **480 B**/screen via `$FE12` (or MAP `$FE93` -> VRAM).

**How L1 scroll works in practice:**

1. Keep four neighboring playfield screens loaded in slots **0-3** (the 2x2 workbench).
2. Write L1 scroll latches as the camera moves inside that 128x120 window.
3. When the camera would leave the workbench, stream the newly needed screen(s) into the far slots (interleaved VRAM writes), then keep scrolling.
4. Mid-frame scroll changes apply on the **next** tile fetch.

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |
+-------------+-------------+
|  Slot 2     |  Slot 3     |
+-------------+-------------+
        ^ 128x120 viewport (L1 scroll)
```

```text
MAP grid (6 screens)             VRAM slots (2x2 load)
+-----+-----+-----+              +---------+---------+
|  A  |  B* |  C  |              | 0: B    | 1: C    |
+-----+-----+-----+              +---------+---------+
|  D  |  E  |  F  |  player -->  | 2: E    | 3: F    |
+-----+-----+-----+              +---------+---------+

 With zero scroll values for X and Y, VRAM sees B, C, E and F.

 If we scroll left a bit, VRAM loads:

+---------+---------+
| 0: A    | 1: B    |
+---------+---------+
| 2: D    | 3: E    |
+---------+---------+
```

| VRAM offset | Use |
|-------------|-----|
| `$0000`-`$07FF` | L1 camera slots 0-3 |
| `$0800`-`$0FFF` | L0 camera slots 4-7 |
| `$1000`-`$3FFF` | Scratch |
| `$4000`-`$7FFF` | Reserved |

**Streaming cost:** ~480 B per screen (~**11** CRT lines @ ~12 cyc/B with interleave). Scroll 1 px = 1-2 latch writes. See [`memory.md`](memory.md) for PHI2 CPU/PPU phases.

---

## Background fetch (hardware)

Each visible dot:

1. Beam + L1 scroll ---> VRAM tile index + attr byte (slots 0-3).
2. Attr **BANK** (bits 1-0) ---> cart CHR tile fetch.
3. Attr **PAL** + active palette row ---> master index ---> **Color PROM** (board).
4. Compositor picks sprite vs L1 vs L0 vs backdrop (see **Second background** below).

Mid-frame scroll applies on the **next** tile fetch. Do not edit a nametable cell under the beam (tear).

```text
BG attr byte
7 6 5 4 3 2 1 0
| | | | | | |_|__ BANK 0-3   (hardware)
| | | | |_|______ PAL 0-3    (hardware)
| | | |__________ FLIP_H/V   (hardware)
| | |____________ SOLID      (software only, video ignores)
|________________ ANIM       (software only, 4-frame strip B..B+3)
```

`$FE31`-`$FE37` optional bank **stamp** helpers. Live fetch uses per-tile / per-OAM attr bits.

---

## Sprites

**64** OAM entries via `$FE20`/`$FE21` (in **1284**, not CPU RAM). Entry: `Y, tile, attr, X`.

```text
OAM attr byte
7 6 5 4 3 2 1 0
| | | | | | |_|__ BANK 0-3
| | | | |_|______ PAL 0-3
| | | |__________ FLIP_H/V
| |______________ PRIORITY
|________________ SIZE (0=8x8, 1=8x16 tile pair)
```

**Locked raster split (with L0):** fill the **full 120x128** sprite field in **VBlank** (walk Y in 8 px or 16 px bands). Give **HBlank** to L0 line fill. Beam reads sprite pixels from the field during active display. Cap **16** sprites per **logical** scanline. Host Play packs X/Y as signed viewport-relative bytes. Sprites clip to **128x120**.

Phase 1 bring-up may still use a simpler HBlank sprite line path until the full VBlank field lands. Software-visible priority and clip rules stay the same.

```text
Priority (opaque wins):
  sprite  >  L1  >  L0  >  backdrop
```

---

## Palettes

Two layers: **cart indices** and **board Color PROM**.

1. **Cart** stores 8 global BG rows + 8 global sprite rows (**256 B** total). Each entry is a **6-bit master index** (0-63), not RGB.
2. **Active row:** software picks row N (often via `$FE38` hint) then copies **4 BG + 4 sprite** palettes (**32** indices) into `$FE08`/`$FE09`.
3. **Color PROM (board):** 64 entries of packed **R3G3B2**. Studio quantizes kit swatches when burning the PROM.
4. **Shared color 0** across all 8 active palettes (backdrop / L1 show-through / sprite transparency).

`$FE08` = address into the 32-byte active buffer. `$FE09` = data with auto-inc. No `$FE08`/`$FE09` load at boot = undefined colors until PRG writes them. Phase 1 boot PRG streams the start row from cart pals.

---

## Second background (L0 / BG0)

Structured far plane. Studio name **BG0**. Hardware / docs name **L0**. Main playfield is **L1** / **BG1**.

This **replaces** the old two-slot parallax payload model (former slots 4-5 only, Single / Pair H / Pair V).

| Item | Value |
|------|-------|
| Layout | Filled rectangle, `cols * rows` in **1..8** (examples: 1x1 static, 2x2, 2x3, 4x2) |
| Live window | VRAM slots **4-7** (2x2), same MAP stream path as L1 |
| Scroll | `$FE06` / `$FE07` (0-127 / 0-119 inside the L0 workbench) |
| Cart | Up to **8** present L0 screens per world (dir + **480 B** payloads after L1 MAP) |
| Authoring | Studio Worlds: BG1/BG0 sub-button. BG0 **Mode** cycles 1x1 / 2x2 / 2x4 / 4x2 / 1x8 / 8x1 (centered on 8x8 chess) |

### Show-through

Where L1 palette index is **0**, the compositor shows the L0 pixel (else backdrop if L0 is also transparent).

```text
if sprite opaque      -> sprite
else if L1 index != 0 -> L1
else                  -> L0 (or backdrop)
```

Host Play already composites this way from the cart BG0 cache. Silicon target: live L1 on active dots, L0 line fill in **HBlank** from slots 4-7 + cart CHR.

### Proportional scroll

Default is software (6502 or Host Play), not a PLD auto-ratio:

```text
scroll_L0_x = scroll_L1_x * cols_L0 / cols_L1
scroll_L0_y = scroll_L1_y * rows_L0 / rows_L1
```

`cols_*` / `rows_*` are the **enclosing present extents** of each plane (used screens bbox), not the virtual 8x8. Example: L0 **2x2**, L1 **4x4** -> L0 scrolls at half rate on both axes. If `cols_L0 == 1`, X stays 0 (same for rows). If L0 extent is **equal or larger** than L1 on an axis (`cols_L0 >= cols_L1`), that axis does not scroll. Absolute L0 scroll override is allowed for cutscenes.

World/screen/cart caps: [`memory.md`](memory.md).

---

## Graphics `$FExx` ports

| Addr | Name | Role |
|------|------|------|
| `$FE00` | `PPUCTRL` | bit0 BG enable, bit7 NMI enable, camera slot mode bits TBD |
| `$FE01` | `PPUSTATUS` | bit7 VBlank, bit6 raster hit (read clears latched bits) |
| `$FE02`/`$FE03` | L1 scroll X/Y | 0-127 / 0-119 inside the L1 2x2 workbench |
| `$FE04`/`$FE05` | raster / IRQ | Scanline compare + control |
| `$FE06`/`$FE07` | L0 scroll X/Y | 0-127 / 0-119 inside the L0 2x2 workbench |
| `$FE08`/`$FE09` | pal addr/data | Active master indices (**32 B**), auto-inc |
| `$FE10`-`$FE12` | VRAM addr/data | hi, lo, data auto-inc (interleaved) |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc into 1284 OAM |
| `$FE30` | `WORLD` | Active world index **0-7** (select helper) |
| `$FE31`-`$FE37` | bank helpers | Optional attr stamps |
| `$FE38` | `PAL_ROW` | Palette row hint (software still copies `$FE08`/`$FE09`) |
| `$FE40`-`$FE5F` | APU | Bytecode window to 328P ([`sound.md`](sound.md)) |
| `$FE60`/`$FE61` | pads P1/P2 | Bit set = pressed (R L D U X Y Coin Start) |
| `$FE70`-`$FE72` | machine EEPROM | Handshake TBD ([`memory.md`](memory.md)) |
| `$FE90`-`$FE93` | MAP | Cart seek + read auto-inc ([`memory.md`](memory.md)) |

`$FE80` unused. Silicon packs many ports into **9x HC573** (bitfield table still TBD).

---

## Open topics

| Topic | Note |
|-------|------|
| 8x16 sprite fetch | 1284 tile-pair timing still evolving |
| BG `ANIM` rate | Global vs per-game |
| Living-tile list cap | **32** vs **64** cells (`retr01_ANIM_MAX`) |
| VBlank sprite field | Full 120x128 clear+plot vs Phase 1 HBlank line path |
| L0 HBlank fill | Linebuf halves + cart CHR arbitration with MAP |
| `PPUCTRL` camera mode bits | Exact bitfield TBD |

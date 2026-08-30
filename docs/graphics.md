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

---

## VRAM workbench

Six live nametable slots in 32 KB VRAM. Each slot **512 B** (240 tile + 240 attr at `+0xF0`).

| Slots | Role |
|-------|------|
| **0-3** | 2x2 camera field (**256x240** logical tiles) |
| **4-5** | Parallax planes (**two** resident at a time) |

Scroll `$FE02`/`$FE03`: **0-127** / **0-119**. Hardware does **not** auto-load MAP. Crossing a screen border = software streams **480 B**/screen via `$FE12` (or MAP `$FE93` -> VRAM).

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |
+-------------+-------------+
|  Slot 2     |  Slot 3     |
+-------------+-------------+
        ^ 128x120 viewport (scroll)
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
| `$0000`-`$07FF` | Camera slots 0-3 |
| `$0800`-`$0BFF` | Parallax slots 4-5 |
| `$0C00`-`$3FFF` | Scratch |
| `$4000`-`$7FFF` | Reserved |

**Streaming cost:** ~480 B per screen (~**11** CRT lines @ ~12 cyc/B with interleave). Scroll 1 px = 1-2 latch writes.

---

## Background fetch (hardware)

Each visible dot:

1. Beam + scroll ---> VRAM tile index + attr byte.
2. Attr **BANK** (bits 1-0) ---> cart CHR tile fetch.
3. Attr **PAL** + active palette row ---> master index ---> **Color PROM** (board).
4. Compositor picks BG vs sprite pixel.

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

**Pipeline:** 1284 evaluates OAM, fetches CHR in **HBlank**, fills **next** scanline into line-buffer SRAM. Beam reads **current** line from the other half. Not a framebuffer.

```text
         Half A ($000-$07F)          Half B ($080-$0FF)
Line N  | SHOW (beam)      |        | fill N+1 (HBlank)|
Line N+1| fill N+1         |        | SHOW             |
```

Cap **16** sprites per **logical** scanline. Host Play packs X/Y as signed viewport-relative bytes. Sprites clip to **128x120**.

---

## Palettes

- **Cart:** 8 global BG rows + 8 global sprite rows (**256 B** total). Indices into Color PROM only.
- **Active row:** one row N selects **4 BG + 4 sprite** palettes together via `$FE08`/`$FE09`.
- **Color PROM (board):** 64 entries, R3G3B2. Studio quantizes kit swatches to PROM on burn.
- **Shared color 0** across all 8 active palettes.
- **`$FE38` PAL_ROW:** hint only. Software still copies into `$FE08`/`$FE09`.

No `$FE08`/`$FE09` load at boot = undefined colors until PRG writes them.

---

## Parallax (slots 4-5)

Up to **8** payloads per world in cart. PRG loads **two** into VRAM slots **4-5**. Same **480 B** shape as playfield screens.

| Mode | Behavior |
|------|----------|
| **Single** | One **128x120** map, scroll/repeat H and/or V |
| **Pair H** | Two screens side-by-side (**256x120**), horizontal repeat |
| **Pair V** | Two stacked (**128x240**), vertical repeat |

Plane scroll/band: `$FE06`/`$FE07`. When a parallax band is active, main camera may lock on that axis for the frame.

**Slices (optional):** **1..120** bands of variable thickness with per-band H or V offset (roads/curves). Data in system RAM or future `$FExx`. Applies to slots **4-5** only, not playfield slots **0-3**.

```text
H-band example (120 rows):
  Y 0-59:  1 px bands, fine dx ramp
  Y 60-89: 30 px band, constant dx
  Y 90-119: 30 px band, constant dx
```

World/screen/cart caps: [`memory.md`](memory.md).

---

## Graphics `$FExx` ports

| Addr | Name | Role |
|------|------|------|
| `$FE00` | `PPUCTRL` | BG/sprite/NMI enables, camera slot mode |
| `$FE01` | `PPUSTATUS` | VBlank, raster hit (read clears) |
| `$FE02`/`$FE03` | scroll X/Y | 0-127 / 0-119 |
| `$FE04`/`$FE05` | raster / IRQ | Scanline compare + control |
| `$FE06`/`$FE07` | plane band | Parallax band + scroll |
| `$FE08`/`$FE09` | pal addr/data | Active master indices, auto-inc |
| `$FE10`-`$FE12` | VRAM addr/data | auto-inc |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc |
| `$FE31`-`$FE37` | bank helpers | Optional attr stamps |
| `$FE38` | `PAL_ROW` | Palette row hint |

Pads, APU, MAP, EEPROM: [`memory.md`](memory.md), [`sound.md`](sound.md).

Silicon packs `$FExx` into **9x HC573** (bitfield table still TBD).

---

## Open topics

| Topic | Note |
|-------|------|
| 8x16 sprite fetch | 1284 tile-pair timing still evolving |
| BG `ANIM` rate | Global vs per-game |
| Living-tile list cap | **32** vs **64** cells (`retr01_ANIM_MAX`) |
| `$FE07` band end | Second latch may be needed |

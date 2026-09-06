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
| **0-3** | BG1 camera field (**2x2**, main playfield) |
| **4-7** | BG0 camera field (**2x2**, second BG behind BG1) |

**BG1** is the normal graphics layer: the playable, navigable screens (camera, collision, platforms). **BG0** is the second background behind that main BG. It shows through where BG1 uses color index **0**. This BG is not interactive for the player. Details: [Second background (BG0)](#second-background-bg0).

Scroll `$FE02`/`$FE03` (BG1): **0-127** / **0-119**. Scroll `$FE06`/`$FE07` (BG0): same ranges for the far plane. Hardware does **not** auto-load MAP. Crossing a screen border = software streams **480 B**/screen via `$FE12` (or MAP `$FE93` -> VRAM).

**How BG1 scroll works in practice:**

1. Keep four neighboring playfield screens loaded in slots **0-3** (the 2x2 workbench).
2. Write BG1 scroll latches as the camera moves inside that 128x120 window.
3. When the camera would leave the workbench, stream the newly needed screen(s) into the far slots (interleaved VRAM writes), then keep scrolling.
4. Mid-frame scroll changes apply on the **next** tile fetch.

```text
+-------------+-------------+
|  Slot 0     |  Slot 1     |
+-------------+-------------+
|  Slot 2     |  Slot 3     |
+-------------+-------------+
        ^ 128x120 viewport (BG1 scroll)
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
| `$0000`-`$07FF` | BG1 camera slots 0-3 |
| `$0800`-`$0FFF` | BG0 camera slots 4-7 |
| `$1000`-`$3FFF` | Scratch |
| `$4000`-`$7FFF` | Reserved |

**Streaming cost:** ~480 B per screen (~**11** CRT lines @ ~12 cyc/B with interleave). Scroll 1 px = 1-2 latch writes. See [`memory.md`](memory.md) for PHI2 CPU/PPU phases.

---

## Background fetch (hardware)

Each visible dot:

1. Beam + BG1 scroll ---> VRAM tile index + attr byte (slots 0-3).
2. Attr **BANK** (bits 1-0) ---> cart CHR tile fetch.
3. Attr **PAL** + active palette row ---> master index ---> **Color PROM** (board).
4. Compositor picks sprite vs BG1 vs BG0 vs backdrop (see **Second background** below).

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

**Locked raster split (with BG0):** fill the **full 120x128** sprite field in **VBlank** (walk Y in 8 px or 16 px bands). Give **HBlank** to BG0 line fill. Beam reads sprite pixels from the field during active display. Where BG1 color index is **0**, the compositor shows the prepared BG0 line (BG1 mask / show-through). Cap **16** sprites per **logical** scanline.

**VBlank budget (1284 @ 20 MHz):** ~20 scanlines of vertical blank, about **25,000** CPU cycles. Evaluating all **64** OAM entries and plotting **8x16** tiles into the line-buffer SRAM costs about **9,600** cycles. Large margin before active video. Sim models this as `linebuf_oam_fill_field` during VBlank.

```text
Priority (opaque wins):
  sprite  >  BG1  >  BG0  >  backdrop
```

---

## Palettes

Two layers: **cart indices** and **board Color PROM**.

1. **Cart** stores 8 global BG rows + 8 global sprite rows (**256 B** total). Each entry is a **6-bit master index** (0-63), not RGB.
2. **Active row:** software picks row N (often via `$FE38` hint) then copies **4 BG + 4 sprite** palettes (**32** indices) into `$FE08`/`$FE09`.
3. **Color PROM (board):** 64 entries of packed **R3G3B2**. Studio quantizes kit swatches when burning the PROM.
4. **Shared color 0** across all 8 active palettes (backdrop / BG1 show-through / sprite transparency).

`$FE08` = address into the 32-byte active buffer. `$FE09` = data with auto-inc. No `$FE08`/`$FE09` load at boot = undefined colors until PRG writes them. Phase 1 boot PRG streams the start row from cart pals.

---

## Second background (BG0)

**Pitch:** Retr01's SNES-like parallax plane. BG1 is the playfield. BG0 is a real second tilemap that scrolls underneath wherever BG1 uses palette index **0**. Make the far world's present bbox smaller than BG1's and you get slower motion for free (`scroll_BG0 = scroll_BG1 * extent_BG0 / extent_BG1`). Not a mapper IRQ trick. Compositor show-through every dot (Sim / silicon). Emu Host Play composites the same order in the full-frame renderer. Selling narrative: [`selling_points.md`](selling_points.md#snes-like-parallax-bg0).

Structured far plane (**BG0**) behind the main BG. **BG1** is the normal playable/navigable playfield.

This **replaces** the old two-slot parallax payload model (former slots 4-5 only, Single / Pair H / Pair V).

| Item | Value |
|------|-------|
| Layout | Up to **8** present screens anywhere on the **16x16** map. Scroll ratio uses the enclosing present bbox |
| Live window | VRAM slots **4-7** (2x2), same MAP stream path as BG1 |
| Scroll | `$FE06` / `$FE07` (0-127 / 0-119 inside the BG0 workbench) |
| Cart | Up to **8** present BG0 screens per world (dir + **480 B** payloads after BG1 MAP). Dir coords are bbox-origin relative |
| Authoring | Studio Worlds: BG1/BG0 sub-button. Place screens freely on the **16x16** chess (max **8** present) |

### Show-through

Where BG1 palette index is **0**, the compositor shows the BG0 pixel (else backdrop if BG0 is also transparent). **Missing / unloaded BG1 VRAM slots** (outside the present-screen world bbox streamed into the 2x2 workbench) show **backdrop**, not BG0 - only true color-0 show-through reveals the far plane.

```text
if sprite opaque      -> sprite
else if BG1 index != 0 -> BG1
else                  -> BG0 (or backdrop)
```

Emu Host Play composites BG0 under BG1 color 0 from the cart BG0 cache in the full-frame renderer. Sim prepares each BG0 line in **HBlank** and applies the BG1 color-0 mask on active dots. Silicon target matches that split (VRAM slots 4-7 + cart CHR on the BG0 path).

### Proportional scroll (parallax)

Default is software (6502 or Host Play), not a PLD auto-ratio. This is the **parallax depth** knob:

```text
scroll_BG0_x = scroll_BG1_x * cols_BG0 / cols_BG1
scroll_BG0_y = scroll_BG1_y * rows_BG0 / rows_BG1
```

`cols_*` / `rows_*` are the **enclosing present extents** of each plane (used screens bbox), not the virtual **16x16**. Example: BG0 **2x2**, BG1 **4x4** -> BG0 scrolls at half rate on both axes (mountains crawl while the playfield runs). If `cols_BG0 == 1`, X stays 0 (same for rows). If BG0 extent is **equal or larger** than BG1 on an axis (`cols_BG0 >= cols_BG1`), that axis does not scroll. Absolute BG0 scroll override is allowed for cutscenes.

World/screen/cart caps: [`memory.md`](memory.md) (**48** BG1 present/world, **16x16** virtual grid, cell coords as nibbles).

---

## Graphics `$FExx` ports

| Addr | Name | Role |
|------|------|------|
| `$FE00` | `PPUCTRL` | See [bitfield](#ppuctrl-fe00) |
| `$FE01` | `PPUSTATUS` | bit7 VBlank, bit6 raster hit (read clears latched bits) |
| `$FE02`/`$FE03` | BG1 scroll X/Y | 0-127 / 0-119 inside the BG1 2x2 workbench |
| `$FE04`/`$FE05` | raster / IRQ | Scanline compare Y (`$FE04`) + raster control (`$FE05`) |
| `$FE06`/`$FE07` | BG0 scroll X/Y | 0-127 / 0-119 inside the BG0 2x2 workbench |
| `$FE08`/`$FE09` | pal addr/data | Active master indices (**32 B**), auto-inc on `$FE09` |
| `$FE10`-`$FE12` | VRAM addr/data | hi, lo, data auto-inc (interleaved) |
| `$FE20`/`$FE21` | OAM addr/data | auto-inc into 1284 OAM |
| `$FE22`-`$FE24` | cart EEPROM | Save mailbox via 1284 ([`memory.md`](memory.md)) |
| `$FE30` | `WORLD` | Active world index **0-7** (select helper) |
| `$FE31`-`$FE37` | bank helpers | Optional attr stamps |
| `$FE38` | `PAL_ROW` | Palette row hint (software still copies `$FE08`/`$FE09`) |
| `$FE40`-`$FE5F` | APU | Bytecode window to 328P ([`sound.md`](sound.md)) |
| `$FE60`/`$FE61` | pads P1/P2 | Bit set = pressed ([`controllers.md`](controllers.md)) |
| `$FE70`-`$FE72` | machine EEPROM | 1284 internal EEPROM mailbox ([`memory.md`](memory.md)) |
| `$FE90`-`$FE93` | MAP | Cart seek + read auto-inc ([`memory.md`](memory.md)) |

`$FE80` unused.

### `PPUCTRL` (`$FE00`)

**Silicon target** bitfield:

| Bit | Name | Meaning |
|-----|------|---------|
| 7 | NMI_EN | **1** = assert CPU **NMI** on VBlank |
| 6-5 | L1_CAM | BG1 camera mode: **00** clamp, **01** wrap H, **10** wrap V, **11** wrap both |
| 4-3 | L0_CAM | BG0 camera mode (same encoding) |
| 2 | SPR_EN | Sprites enable |
| 1 | L0_EN | Layer 0 / BG0 enable |
| 0 | L1_EN | Layer 1 / BG1 enable |

**Runners today (Emu):** **L1_EN**, **L0_EN**, **SPR_EN**, and **NMI_EN** affect rendering. Camera-wrap bits (4-3, 6-5) are stored but not enforced yet. See [`hardware.md`](hardware.md#runners-today-vs-silicon-target).

### `$FExx` ownership

Critical real-time bytes are registered inside existing ATF22V10s. Soft bytes live on the ATmega1284P. Software writes the same `$FExx` addresses either way.

| Port | Owner | Notes |
|------|-------|-------|
| `$FE02` | UPLDB (scroll X reg) | Load on `SEL_FE02` + CPU D. Used every beam/dot. |
| `$FE03` | UPLDX (scroll Y reg) | Load on `SEL_FE03`. |
| `$FE04` | UPLDY (raster Y reg) | Internal compare vs beam Y. `EQ#` -> IRQB. No external Q hop. |
| `$FE00` | ATmega1284P soft | `PPUCTRL` |
| `$FE05` | ATmega1284P soft | Raster control |
| `$FE06` / `$FE07` | ATmega1284P soft | BG0 scroll (already soft) |
| `$FE08` | ATmega1284P soft | Palette address |
| `$FE90`-`$FE92` | ATmega1284P soft | Full 24-bit MAP seek |
| `CART_A0`-`A13` | W65C02S `CPU_A` | Unchanged direct to J36 |
| `CART_A14`-`A18` | UPLDV registered export | Hard pin path for MAP high bits (1284 GPIO budget exhausted). Loaded from CPU D on `SEL_FE91`/`FE92`. |

Ports **not** in this table (`$FE09` palette data, `$FE10`-`$FE12` VRAM, OAM, APU, mailboxes) use other paths (MCU, qualified strobes, or direct read ports).

**Sim note:** soft `$FExx` on `R01sBoard` (`peek_fe` / `poke_fe`) and health under island L (1284). No separate soft-$FExx DIP island on the Sim canvas. See [runners vs silicon](hardware.md#runners-today-vs-silicon-target).

**Fitter escape:** if product terms or pins overflow on the three PLDs holding 24 scroll/raster bits, add +1 ATF22V10 for those bits only.

---

## Resolved topics

| Topic | Resolution |
|-------|------------|
| `PPUCTRL` camera bits | Bitfield above |
| `$FExx` ownership | table above |
| 8x16 sprite VBlank timing | ~9.6k / ~25k cycles ([sprites](#sprites)) |
| BG0 HBlank fill | Next BG0 line into linebuf `$4000` ping-pong. BG1 color-0 mask on active dots |

| Topic | Still open |
|-------|------------|
| BG `ANIM` rate | Global vs per-game |
| Living-tile list cap | **32** vs **64** cells (`retr01_ANIM_MAX`) |

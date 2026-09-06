# PDIP package dimensions (Retr01 sim / board)

Canonical **plastic DIP** body sizes used by the simulator canvas and for layout planning.

Where a part has a measured / datasheet body, the sim uses **`r01s_entity_set_dip_mm()`**.
Otherwise it falls back to the pin-count defaults below (74HC N-package family for 14/16/20,
JEDEC-class 600 mil for larger counts).

## Sim canvas scale

| Constant | Value | Notes |
|----------|-------|-------|
| `R01S_PX_PER_MM` | **4** | **1 mm real = 4 logical px** (bbox / 4 -> mm) |
| Pin pitch | **2.54 mm** -> **10 px** | JEDEC 0.100" (fixed) |

**Layout workflow:** COMPACT pack -> enclose in an image editor -> **/ 4** for approx real mm.

## Part-specific bodies (authoritative)

| Part | Pins | Body LxW (mm) | Horizontal px @ 4 px/mm | Notes |
|------|------|---------------|-------------------------|-------|
| **W65C02S** | 40 | **52 x 16** | 208 x 64 | Measured / vendor. WDC DS omits outline |
| **SST39SF040** | 32 | **42 x 14** | 168 x 56 | |
| **AS6C62256** | 28 | **37 x 13** | 148 x 52 | 600 mil |
| **ATF22V10** | 24 | **32 x 8** | 128 x 32 | **300 mil**. KiCad `DIP-24_W7.62mm` |
| **ATmega1284P** | 40 | **53 x 14** | 212 x 56 | 600 mil |
| **ATmega328P** | 28 | **35 x 8** | 140 x 32 | **300 mil**. KiCad `DIP-28_W7.62mm` |

## 74HC family DIP (N / through-hole)

From typical 74HC PDIP drawings (body width ~= 6.35 mm -> **6 mm** in sim. Lengths rounded):

| Pins | Length (mm) | Body width (mm) | Total width (mm) | Pitch | Sim LxW px |
|------|-------------|-----------------|------------------|-------|------------|
| 14 | ~19.3 -> **19** | 6.35 -> **6** | 7.62 | 2.54 | 76 x 24 |
| 16 | ~19.8 -> **20** | 6.35 -> **6** | 7.62 | 2.54 | 80 x 24 |
| 20 | ~25.4 -> **25** | 6.35 -> **6** | 7.62 | 2.54 | 100 x 24 |

Used by: 74HC00/04/08/14/32 (14), 74HC157/161 (16), 74HC245/573/688 (20).

## Fallback pin-count table

| Pins | Row class | Length D (mm) | Width E1 (mm) | Used when |
|------|-----------|---------------|---------------|-----------|
| 8 | 300 mil | **9** | **6** | 24C64, etc. |
| 14 | 300 mil | **19** | **6** | HC 14-pin |
| 16 | 300 mil | **20** | **6** | HC 16-pin |
| 20 | 300 mil | **25** | **6** | HC 20-pin |
| 24 | 300 mil | **32** | **8** | ATF22V10 (not 600 mil) |
| 28 | 600 mil | **36** | **14** | SRAM / EPROM class |
| 28n | 300 mil | **35** | **8** | ATmega328P-PU |
| 32 | 600 mil | **42** | **14** | default 32-pin |
| 40 | 600 mil | **52** | **14** | default 40-pin |

## Orientation (sim UI)

- **Horizontal (default):** length along X. Pins on top/bottom. Notch left. Pin 1 bottom-left.
- **Vertical:** length along Y. Pins left/right. Notch top. Pin 1 top-left.

Right-click or **R** (selected) to toggle. Labels rotate with the package.

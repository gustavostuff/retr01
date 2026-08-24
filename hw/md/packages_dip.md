# PDIP package dimensions (Retr01 sim / board)

Canonical **plastic DIP** body sizes used by the simulator canvas and for layout planning.
Sources: [Microchip Packaging Spec DS00049](https://ww1.microchip.com/downloads/en/PackagingSpec/00049w.pdf) (PDIP family, nominal mm), JEDEC MS-001 / MS-011 row spacing, and WDC W65C02S as a standard **40-pin 600 mil** PDIP (datasheet omits outline; body matches JEDEC-class 40-pin parts).

## Sim canvas scale

| Constant | Value | Notes |
|----------|-------|-------|
| `R01S_PX_PER_MM` | **4** | One logical pixel = 0.25 mm |
| Example | W65C02S ≈ **52 × 14 mm** → **208 × 56 px** (horizontal) | |
| Pin pitch | **2.54 mm** → **10 px** (fixed; never scaled to body) | JEDEC 0.100″ |

All IC packages share this single scale. Glyphs (PWR / OSC / LCD) are not JEDEC DIPs; they keep separate art sizes.

## Body sizes (molded package)

`Length` = overall body **D** (along the pin rows). `Width` = molded body **E1** (across the two rows). Row spacing **E** is the lead-row distance (300 mil or 600 mil), slightly wider than E1.

| Pins | Row | Length D (mm) | Width E1 (mm) | Horizontal px (L×W) | Typical Retr01 parts |
|------|-----|---------------|---------------|---------------------|----------------------|
| 8 | 300 mil | **9** | **6** | 36 × 24 | 24C64 (cart I2C), can osc shells |
| 14 | 300 mil | **19** | **6** | 76 × 24 | 74HC00/04/08/14/32 |
| 16 | 300 mil | **19** | **6** | 76 × 24 | 74HC157, 74HC161 |
| 20 | 300 mil | **26** | **6** | 104 × 24 | 74HC245, 74HC573, 74HC688 |
| 24 | 600 mil | **32** | **14** | 128 × 56 | ATF22V10, AT28C16 |
| 28 | 600 mil | **36** | **14** | 144 × 56 | AS6C62256, ATmega328P |
| 32 | 600 mil | **42** | **14** | 168 × 56 | SST39SF040 |
| 40 | 600 mil | **52** | **14** | 208 × 56 | W65C02S, ATmega1284P |

Nominal Microchip PDIP figures (where published) round to the mm values above for a clean 4 px/mm grid (e.g. 40-pin E1 nom **13.84 mm** → **14 mm**; D nom ~**52.3 mm** → **52 mm**).

## Orientation (sim UI)

- **Horizontal (default):** length along X; pins on top and bottom; notch on the left; pin 1 bottom-left.
- **Vertical:** length along Y; pins on left and right; notch on top; pin 1 top-left (classic DIP drawing).

Right-click a chip in the simulator to toggle orientation. Labels rotate with the package.

## Per-part notes

Each IC markdown in this folder should list its Retr01 package and point here for the numeric outline. Vendor PDFs remain authoritative for PCB footprints (pad size, lead span `eB`, tolerances).

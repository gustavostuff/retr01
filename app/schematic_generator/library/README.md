# Retr01_Lib (KiCad symbols)

**Pin numbers are locked to KiCad 10 stock libraries** (`/usr/share/kicad/symbols`).
See `retr01_schem/pinmap.py` and `retr01_schem/kicad_pin_extract.json`.

SKiDL still instantiates inline `Part(tool=SKIDL)` parts, but every pin **name and number** is the physical DIP pin (`"1"`...`"40"`). Official KiCad pin names (e.g. `Load`, `A->B`, `~{CS}`) are attached as **aliases** for readability.

## Stock mapping (already extracted)

| Retr01 MPN | KiCad lib | KiCad symbol |
|------------|-----------|--------------|
| SN74HC573 | 74xx | **74LS573** (no HC573 in lib. Same DIP) |
| SN74HC157 | 74xx | **74LS157** |
| SN74HC245 | 74xx | **74HC245** -> extends 74LS245 |
| SN74HC14 | 74xx | **74HC14** |
| AS6C62256 | Memory_RAM | **KM62256CLP** (JEDEC twin) |
| AT27C256R | Memory_EPROM | **27C256** |
| SST39SF040 | Memory_Flash | **SST39SF040** |
| 24C64 | Memory_EEPROM | **24LC64** |
| ATmega1284P | MCU_Microchip_ATmega | **ATmega1284P-P** |
| ATmega328P | MCU_Microchip_ATmega | **ATmega328P-P** |
| R / C | Device | **R**, **C** |

## Still need custom Retr01_Lib symbols / footprints

| Part | Why |
|------|-----|
| W65C02S | Not in stock CPU lib (symbol) |
| ATF22V10 | No 22V10 stock symbol |
| Switchcraft **35RAPC2BVN4** | Custom TRS footprint - **done** (`Retr01_Lib.pretty`, Tip=4 / Ring=2 / Sleeve=1) |
| CUI **RCJ-01x** | Custom RCA footprint - **done** (`CUI_RCJ-01x_Vertical`. RCJ-012/014 share holes) |
| EDAC **395-036-559-212** | Optional: replace PinSocket stand-in with manufacturer CAD |

**Stock now (do not reinvent):** Abracon **ACO** cans -> KiCad `Oscillator:ACO-xxxMHz-A` + `Oscillator_DIP-14`. **AD725ARZ** chip is wide SOIC-16. **Mobo places DIP-16** for **Proto Advantage PA0006** (SOIC-16 300 mil to DIP-16). **CUI PJ-063AH** barrel. HC/AVR/memory symbols per table above. RCA uses stock symbol **`Conn_Coaxial`** + custom RCJ footprint.

**You do not need to intervene for Quilter / netlist:** SKiDL emits pin numbers + footprints. Draw W65C02S / ATF22V10 `.kicad_sym` only when you want a human-readable KiCad schematic.

## Workflow

1. For custom parts: draw `.kicad_sym` here with **matching pin numbers**.
2. Optionally switch `make_part()` to `Part("74xx", "74HC245", ...)` once footprint assignment is confirmed.
3. `python generate.py --check && python generate.py`

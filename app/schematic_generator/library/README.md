# Retr01_Lib (KiCad symbols)

**Pin numbers are locked to KiCad 10 stock libraries** (`/usr/share/kicad/symbols`).  
See `retr01_schem/pinmap.py` and `retr01_schem/kicad_pin_extract.json`.

SKiDL still instantiates inline `Part(tool=SKIDL)` parts, but every pin **name and number** is the physical DIP pin (`"1"`…`"40"`). Official KiCad pin names (e.g. `Load`, `A->B`, `~{CS}`) are attached as **aliases** for readability.

## Stock mapping (already extracted)

| Retr01 MPN | KiCad lib | KiCad symbol |
|------------|-----------|--------------|
| SN74HC573 | 74xx | **74LS573** (no HC573 in lib; same DIP) |
| SN74HC157 | 74xx | **74LS157** |
| SN74HC245 | 74xx | **74HC245** → extends 74LS245 |
| SN74HC14 | 74xx | **74HC14** |
| AS6C62256 | Memory_RAM | **KM62256CLP** (JEDEC twin) |
| AT27C256R | Memory_EPROM | **27C256** |
| SST39SF040 | Memory_Flash | **SST39SF040** |
| 24C64 | Memory_EEPROM | **24LC64** |
| ATmega1284P | MCU_Microchip_ATmega | **ATmega1284P-P** |
| ATmega328P | MCU_Microchip_ATmega | **ATmega328P-P** |
| R / C | Device | **R**, **C** |

## Still need custom Retr01_Lib symbols

| Part | Why |
|------|-----|
| W65C02S | Not in stock CPU lib |
| ATF22V10 | No 22V10 stock symbol |
| OSC cans, cart edge, arcade/TRS/barrel | Connectors / cans |

Pin numbers for those come from `hw/md/*.md` / datasheets (same physical DIP numbering).

## Workflow

1. For custom parts: draw `.kicad_sym` here with **matching pin numbers**.
2. Optionally switch `make_part()` to `Part("74xx", "74HC245", …)` once footprint assignment is confirmed.
3. `python generate.py --check && python generate.py`

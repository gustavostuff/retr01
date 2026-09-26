# KiCad schematic Tier H: symbols and libraries

**Project path:** `apps/sim/tier-h/kicad/main-pcb/v_01/` (`v_01.kicad_pro`, `v_01.kicad_sch`, `v_01.kicad_pcb`).

**Footprint library on disk:** `${KIPRJMOD}/library/Retr01_Lib.pretty` (copied by `apps/sim/tier-h/skidl/export_netlist.sh` from `apps/sim/tier-h/skidl/library/`).

---

## 1. Open and configure the project

1. Launch KiCad. **File -> Open Project** and select `v_01/v_01.kicad_pro`.
2. Open **Schematic Editor** (Eeschema) from the project manager.
3. **Preferences -> Manage Symbol Libraries**. Enable stock libraries (KiCad 8/9: `Symbol Libraries` tab). Typical installs use `/usr/share/kicad/symbols`.
4. **Preferences -> Manage Footprint Libraries**. Confirm project table **`fp-lib-table`** lists `Retr01_Lib` at `${KIPRJMOD}/library/Retr01_Lib.pretty`.
5. **File -> Schematic Setup -> Electrical Rules**. Set **GND** and **+5V** as power nets. Enable ERC before PCB export.

---

## 2. Reference designators (motherboard)

Use these refdes on symbols. Values come from [`hardware.md`](../general/hardware.md).

| Refdes | Part | KiCad symbol hint | Footprint (`Retr01_Lib` or stock) |
| --- | --- | --- | --- |
| **U1** | W65C02S | `CPU_W65C02SxPA` or generic 6502 family | DIP-40 |
| **U3** | AS6C62256 | `Memory_RAM:AS6C62256` or KM62256 class | DIP-28 |
| **U6** | AS6C62256 | same | DIP-28 |
| **U41** | AS6C62256 | same | DIP-28 |
| **U4** | AT27C256R (PRG) | `Memory_EPROM:27C256` | DIP-28 |
| **U24** | AT27C256R (color PROM) | same | DIP-28 |
| **UM** | AVR128DB28 (MCU-M) | `MCU_Microchip_AVR:AVR128DB28-xx` SPDIP-28 | DIP-28 narrow |
| **US1** | AVR128DB28 (MCU-S1) | same | DIP-28 narrow |
| **US2** | AVR128DB28 (MCU-S2) | same | DIP-28 narrow |
| **UPLDX** | ATF22V10 (Beam X) | `Programmable Logic:ATF22V10xx` | DIP-24 |
| **UPLDY** | ATF22V10 (Beam Y) | same | DIP-24 |
| **UPLDV** | ATF22V10 (Compositor) | same | DIP-24 |
| **U7A**, **U7B**, **U7C** | SN74HC157 | `74xx_74LS157` (same DIP pinout) | DIP-16 |
| **U573** | SN74HC573 | `74xx_74LS573` | DIP-20 |
| **U574** | SN74HC574 | `74xx_74LS574` | DIP-20 |
| **U725** | AD724 | SOIC-16 on adapter or project symbol | DIP-16 adapter |
| **Y1**, **Y2**, **Y3** | Crystals / osc cans | `Device:Crystal` or canned osc symbol | DIP-8 can per `tier_h_map.py` |
| **J1** | Barrel 5 V | `Connector:Barrel_Jack` | CUI PJ-063AH class |
| **J2** | RGB + sync 1x6 | Connector symbol | 1x6 header |
| **J3**, **J4** | TRS pad jacks | Custom / Switchcraft | `Retr01_Lib` TRS |
| **J5**, **J6** | Arcade 1x10 | Pin header | 1x10 |
| **J7** | Cabinet power/reset 1x4 | Pin header | 1x4 |
| **J8**, **J9** | RCA | Custom | `Retr01_Lib` RCJ |
| **J36** | Cart socket 2x18 | Custom | `EDAC_395_MoboSocket_2x18_2.54x5.08mm` |
| **C1-C27**, **R1-R30**, **E1** | Passives | `Device:C`, `Device:R`, polarized bulk | THT ceramic / axial |

Cart flash **U40** and EEPROM **U50** belong on the **cart PCB** schematic. The motherboard sheet stops at **J36** (see wiring doc).

Optional **74HC14** (not in counted 19): add only when clock/reset conditioning is required ([`hardware.md`](../general/hardware.md)).

---

## 3. Place symbols on the schematic

1. **Place -> Symbol** (or hotkey). Pick library + symbol from section 2.
2. Set **Reference** immediately (U1, UM, ...). **Value** field = MPN or role (`AS6C62256`, `MCU-M`).
3. **Properties -> Footprint** = `Retr01_Lib:FootprintName` when custom (J36, J3, J8, ...). Use stock KiCad DIP footprints for generic ICs if Retr01 copy is not synced yet.
4. Repeat until every BOM line has a symbol. Use **Place -> Power Symbol** for `+5V` and `GND` (global labels propagate).
5. **Place -> No Connection** on unused MCU pins, PROM address lines tied to GND in hardware, and NC footprint pads documented in Skidl export.

---

## 4. Net labels (naming convention)

Use global labels (not hidden net names only) for buses that cross sheet areas:

| Label | Meaning |
| --- | --- |
| `+5V` | 5 V rail after **J1** / bulk |
| `GND` | Common ground |
| `PHI2` | CPU clock (after **R12**) |
| `DOT` | Beam dot clock (after **R13**) |
| `CPU_A0`..`CPU_A15` or `A[0:15]` | CPU address (match symbol pin names) |
| `CPU_D0`..`CPU_D7` | CPU data |
| `CART_OE#`, `CART_WE#` | Mobo-side cart control (before **R22** / **R23**) |
| `FSC_XTAL` | **Y3** output toward AD724 |
| `SPI_MOSI`, `SPI_MISO`, `SPI_SCK`, `/SS_S1`, `/SS_S2` | MCU SPI mailbox |

Match [`hardware.md`](../general/hardware.md) net names on MCU ports (`CPU_D0`, `I2C_SDA`, `ALE`, ...).

---

## 5. Annotation and ERC

1. **Tools -> Annotate Schematic**. Prefix **U** for ICs, **R**/**C**/**Y**/**J** per refdes table.
2. **Inspect -> Electrical Rules Checker**. Resolve:
   - Unconnected power pins on ICs
   - Input pins floating (tie unused PLD/MCU inputs to GND or `+5V` per data sheet)
   - Power pins not driven
3. **File -> Save** schematic.

---

## 6. Link to PCB

1. **Tools -> Update PCB from Schematic** (F8).
2. Match each refdes to an existing footprint on `v_01.kicad_pcb` or accept new footprints.
3. **Tools -> Update Footprints from Library** on connectors if KiCad substituted stock shapes.

Symbol pin numbers must match the physical DIP (see `apps/sim/tier-h/skidl/retr01_kicad/pinmap.py` for KiCad vs datasheet numbering on 74xx and memories).

---

## Related

- [`kicad-schematic-tier-h-passives.md`](kicad-schematic-tier-h-passives.md): next step after symbols exist
- [`kicad-schematic-tier-h.md`](kicad-schematic-tier-h.md): overview

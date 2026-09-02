# Lab 02: Cart PRG read

**Goal:** Add the **36-pin cart edge** (or a DIP flash socket for bring-up) and wire the **SST39SF040** so the 6502 can **read** program bytes in `$8000-$FFFF` (with the `$FE00-$FEFF` I/O hole still temporary / ignored for smoke).

**Pass:** Complete the **Success metrics** section (KiCad table required). Optional flash program / Sim later.

**Depends on:** [Lab 01](01_cpu_and_system_ram.md).

**Do not add yet:** save EEPROM traffic beyond stubbing SDA/SCL pins, flasher board, CHR fetch, MAP port, HC245 (Lab 03).

**Related:** [`../cart.md`](../cart.md). [`../../hw/md/SST39SF040.md`](../../hw/md/SST39SF040.md). [`../memory.md`](../memory.md).

---

## 0. Ideas in plain language

Cart flash is ROM the CPU executes from. On Retr01:

| CPU address | What should answer |
|-------------|--------------------|
| `$0000-$7FFF` | System RAM (Lab 01) |
| `$8000-$FDFF` and `$FF00-$FFFF` | Cart flash PRG |
| `$FE00-$FEFF` | I/O (Labs 03-04). Must **not** enable flash |

Think of flash `OE#` as `if (prg_selected && cpu_read) drive_data_bus`.

Production flash lives on the **cartridge PCB**. Early bring-up may use a **DIP socket on the motherboard** with the same nets. Electrically you still name them `CART_*`.

---

## 1. New hierarchical sheet

On the root schematic:

1. **Insert -> Hierarchical Sheet**
2. Sheet name: `Cart`
3. File: `sheets/02_cart.kicad_sch`
4. Import hierarchical labels you need from Lab 01 onto the sheet boundary:

| Label (import) | Why |
|----------------|-----|
| `+5V` `GND` | power |
| `CPU_A0`..`CPU_A15` | address |
| `CPU_D0`..`CPU_D7` | data |
| `RWB` | read vs write |
| `PHI2` | optional qualify later |
| `RESB` | optional |

Create new labels on this sheet for cart-only nets: `CART_OE#`, `CART_WE#`, `CART_CE#` (cart-local), `SDA`, `SCL`.

---

## 2. Build or place the 36-pin cart symbol

Follow the pinout in [`../cart.md`](../cart.md#36-pin-edge-pinout).

### 2.1 KiCad symbol (once)

1. Symbol Editor -> new symbol `Conn_Cart_Edge_36` in a `Retr01` library.
2. Use pin numbers `A1`..`A18` and `B1`..`B18` (alphanumeric pins are OK).
3. Electrical types: power pins Passive, buses Bidirectional, `OE#`/`WE#` Output from mobo view, `SDA` Bidirectional, `SCL` as driven by host.

### 2.2 Motherboard connector wiring (play)

| Edge pin | Net |
|----------|-----|
| A1 A18 B1 | `GND` |
| A2 B2 | `+5V` |
| A3 | `SDA` (Lab 02: pull-up 4.7k, no 1284 yet) |
| B3 | `SCL` (same) |
| A4 .. A17 | `CPU_A0` .. `CPU_A13` for bring-up **or** `CART_A0`..`CART_A13` if you already mux |
| B13 .. B17 | upper address `CART_A14`..`CART_A18` |
| B4 .. B11 | `CPU_D0` .. `CPU_D7` (temporary direct, Lab 03 adds HC245) |
| B12 | `CART_OE#` |
| B18 | `CART_WE#` |

**Address mapping for PRG smoke (temporary, direct):**

PRG at `$8000` means CPU A15=1. Flash needs a full 19-bit address for 512 KB, but the CPU only sees 16 bits. For **32 KB PRG window** smoke:

| Flash address bit | Source for Lab 02 smoke |
|-------------------|-------------------------|
| A0-A14 | `CPU_A0`-`CPU_A14` |
| A15-A18 | tie **GND** for a fixed 32 KB image in the low flash region **or** decode later |

That matches "32 KB PRG at `$8000+`" with no banking ([`../memory.md`](../memory.md)). Larger cart layout still uses A16-A18 for CHR/MAP regions. Those fetches come later. For Lab 02, tying A16-A18 low is OK if your test image sits at the start of flash.

Document on the sheet: `LAB02: CART_A16..A18 = 0 for PRG smoke`.

---

## 3. Flash chip (on cart sheet or socket)

Place `SST39SF040` (DIP-32). Pinout: [`../../hw/md/SST39SF040.md`](../../hw/md/SST39SF040.md).

| Flash pin | Net |
|-----------|-----|
| A0-A14 | from edge / `CPU_A0`-`CPU_A14` |
| A15 | from policy above (often CPU_A15 through mapping, or GND if window fixed) |
| A16-A18 | GND for Lab 02 smoke |
| DQ0-DQ7 | `CPU_D*` |
| CE# | **GND** on cart (always enabled). Mobo uses OE#/WE# |
| OE# | `CART_OE#` |
| WE# | `CART_WE#` |
| VDD / VSS | `+5V` / `GND` + 100 nF |

---

## 4. Temporary PRG chip-select (Lab 02)

Conflict to avoid:

- Lab 01: RAM when `CPU_A15=0`
- Lab 02: flash OE when PRG region and read

**Temporary rule:**

```text
PRG_READ = (CPU_A15 == 1) && (RWB == 1) && not_io_hole
CART_OE# = 0 when PRG_READ else 1
CART_WE# = 1 always in play (pull-up 10k to +5V)
```

### 4.1 I/O hole `$FE00-$FEFF`

Full decode needs A8-A15 == `$FE`. For Lab 02 smoke you can:

**Option A (strict enough):** disable flash when `CPU_A8`-`CPU_A15` match `$FE` (needs a few gates or a 74HC138).

**Option B (sloppy smoke):** ignore the hole until Lab 03. Risk: fetches from `$FExx` hit flash. Fine for a first LED blink in `$8000` if vectors are not in `$FExx`.

Vectors live at `$FFFC` etc. (not in `$FExx`). Option B is acceptable for "LED blink from `$8000`" if reset vector is programmed in flash.

**Recommend Option A** if you already place Lab 03 soon. Otherwise Option B + comment.

### 4.2 Gate sketch (Option A, conceptual)

```text
io_hole = A15 & A14 & A13 & A12 & A11 & A10 & A9 & !A8
          (that is A15..A8 = 11111110 = $FE)
prg     = A15 & !io_hole
CART_OE# = !(prg & RWB)     active low OE
```

Use HC gates or leave a box "TO PLD LAB 03" with soft labels. If you use discrete gates, mark **TEMPORARY**.

---

## 5. Pull-ups and idle levels

| Net | Level in play |
|-----|----------------|
| `CART_WE#` | pulled **high** (10k). Never write flash from game CPU |
| `CART_OE#` | high when idle, low only on PRG read |
| `SDA` `SCL` | 4.7k to `+5V`. No master until 1284 lab |

---

## 6. 24C64 (optional this lab)

The save EEPROM counts in the 32-IC system tally. For Lab 02 you may:

- Place the symbol on a **cart module** sheet with SDA/SCL to the edge, or
- Omit the body and only reserve edge pins

If you place it: A0-A2 straps to `GND`, VCC/GND normal, WP per datasheet policy.

---

## 7. Conflict with Lab 01 RAM

Verify:

| A15 | RAM_CE# (Lab 01) | CART_OE# (Lab 02) |
|-----|------------------|-------------------|
| 0 | active (low) | inactive (high) |
| 1 | inactive (high) | active on read if PRG |

If both drive `CPU_D*` at once, you have a **bus fight**. ERC will not always catch that. Trace OE/CE by hand.

---

## 8. Success metrics

### In KiCad (required to pass Lab 02)

| # | Check | What you should see |
|---|--------|---------------------|
| 1 | Annotate | Cart connector and flash (or socket) have refdes. No `?` |
| 2 | ERC | **0 errors** on the cart sheet and root. Floating SDA/SCL OK only if pull-ups are drawn |
| 3 | Highlight `CPU_A0` | Still reaches U1, U2, **and** cart/flash A0 (or `CART_A0` join) |
| 4 | Highlight `CPU_D0` | Reaches U1, U2, **and** flash DQ0 / edge D0 |
| 5 | Highlight `CART_OE#` | Reaches flash **OE#** (and edge B12 if using 36-pin). Driven by temporary PRG-read logic or a labeled stub, not floating |
| 6 | Highlight `CART_WE#` | Reaches flash **WE#** / edge B18. Only a **pull-up to `+5V`**, no CPU write gate that can pull it low |
| 7 | Highlight `CART_A16` (or tied pin) | Shows tie to `GND` if you used Lab 02 PRG smoke mapping (document on sheet) |
| 8 | Bus-fight table | On-sheet or notebook: A15=0 -> RAM only, A15=1 read -> flash OE only |
| 9 | Pinout vs docs | Edge A/B names match [`../cart.md`](../cart.md). Flash pins match [`../../hw/md/SST39SF040.md`](../../hw/md/SST39SF040.md) |
| 10 | PDF | Plot cart sheet saved |

**Lab 02 KiCad pass** = rows 1-10 all true.

### Optional later (hardware / Sim)

| Test | Expect |
|------|--------|
| Program minimal image, reset vector -> `$8000+` code | CPU fetches opcodes from flash after reset |
| Code stores a marker in system RAM | Read-back matches (proves Lab 01 + Lab 02 together) |
| Scope / LED on a known write | Confirms execution, not just power |
| Sim cart / island J path | Cart flash answers PRG reads ([`../../retr01/sim/README.md`](../../retr01/sim/README.md)) |

---

## 9. Done / not done

**Done**

- Cart edge (or socket) in the schematic
- Flash read path for `$8000+`
- `CART_WE#` safe for play
- Temporary OE decode documented

**Not done**

- HC245 cart domain isolation (Lab 03)
- Full ATF22V10 decode (Lab 03)
- MAP `$FE90-$FE93`, CHR fetch, I2C master
- USB-C flasher PCB (separate project)

---

## Next

[Lab 03: Decode PLD and CPU bus transceiver](03_decode_pld_and_bus.md)

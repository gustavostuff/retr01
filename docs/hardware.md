# Retr01 Hardware

32-IC Retr01-A motherboard + cart. Through-hole DIP target, ~**14 x 12 cm** minimum 4-layer PCB.

**Related:** [`memory.md`](memory.md) (chips, read/write timing). [`graphics.md`](graphics.md) (VRAM, sprites). [`sound.md`](sound.md) (APU). IC notes: [`hw/md/`](../hw/md/). Bring-up sim: [`retr01_sim/`](../retr01_sim/README.md). Passives / ports / protection: **Non-IC components** below.

---

## 32 IC parts is the current goal

| Block | Parts | Role on PCB |
|------|-------|-------------|
| CPU | W65C02S | Game logic, `$FExx` writes, MAP/VRAM streaming |
| Helper MCUs | ATmega1284P, ATmega328P | Sprites/pads/EEPROM vs dedicated APU |
| SRAM x3 | AS6C62256 | System RAM, interleaved VRAM, sprite line buffer |
| Cart storage | SST39SF040 + 24C64 | 512 KB flash on cart + I2C save EEPROM |
| Video glue | 5x ATF22V10, 6x HC157, 9x HC573, 3x HC245 | Decode, beam, interleave, latches, bus isolation |
| Color out | AT28C16 (or OTP) | 64-entry R3G3B2 PROM -> R-2R -> RGBS |

**Count:** **32** (31 mobo + 1 cart save). Escape **+1 PLD** if compositor overflows. **74HC14** for reset/clock is outside the 32 if not absorbed.

**Clocks:** CPU **8.000 MHz**, dot **5.369318 MHz**, 1284 **20 MHz**, 328P **16 MHz**. Raster **341x262**, ~**60.098 Hz**.

---

## Bill of materials

| Qty | Part | PCB role |
|-----|------|----------|
| 1 | W65C02S | 8 MHz game CPU |
| 1 | ATmega1284P | 20 MHz: OAM, sprite line fill, pads, machine EEPROM |
| 1 | ATmega328P | 16 MHz: APU (`$FE40`-`$FE5F`) |
| 1 | AS6C62256 | 32 KB system RAM |
| 1 | AS6C62256 | 32 KB interleaved VRAM |
| 1 | AS6C62256 | 32 KB sprite line-buffer SRAM |
| 1 | SST39SF040 | 512 KB cart flash (socket on mobo early) |
| 5 | ATF22V10 | Decode, VRAM interleave, X/Y beam, compositor priority |
| 9 | 74HC573 | Bit-packed `$FExx` latches |
| 6 | 74HC157 | VRAM + line-buffer address mux |
| 3 | 74HC245 | CPU / video / cart-OAM bus isolation |
| 1 | AT28C16 | Color PROM (6-bit index -> R3G3B2) |
| 1 | 24C64 (cart) | Per-game save EEPROM |

Datasheets: [W65C02S](https://westerndesigncenter.com/wdc/documentation/w65c02s.pdf), [AS6C62256](https://www.alliancememory.com/wp-content/uploads/pdf/datasheets/AS6C62256.pdf), [ATF22V10](https://ww1.microchip.com/downloads/en/DeviceDoc/ATF22V10-Datasheet-DS50002239D.pdf), [ATmega1284P](https://ww1.microchip.com/downloads/en/DeviceDoc/40002047A.pdf), [ATmega328P](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega328P-DS-DS40002061A.pdf), [SST39SF040](https://ww1.microchip.com/downloads/en/DeviceDoc/20005051C.pdf).

---

## How blocks connect on the PCB

Four compute domains share **5 V** and **never** paint a full framebuffer:

```text
                    +------------------+
  Cart SST39SF040 --| PRG / CHR / MAP  |-- CHR read (BG dots + 1284 HBlank)
                    +--------+---------+
                             |
  W65C02S -------------------+------- $FExx latches (HC573) + PLD decode
       |                     |              |
       | PHI2                |              +---> ATmega328P (APU)
       v                     |
  AS6C62256 sys RAM          +---> ATmega1284P (OAM, pads, machine EEPROM)
       |
       +-- $FE10-$FE12 -----> AS6C62256 VRAM (interleaved with BG fetch)
                                    |
  Dot clock + beam PLDs ------------+---> tile/attr -> cart CHR -> palette -> PROM
                                    |
  ATmega1284P ---------------------> AS6C62256 line buffer (ping-pong 128 px)
                                    |
  Compositor PLD -------------------> RGBS (+ SCALE DIP 1x/2x)
```

**Bus rule:** three HC245 domains + PLD `/OE` ---> one driver at a time per domain.

**SCALE DIP:** **2x** default (128x120 logical fills **256x240** RGBS). **1x** centers 128x120. Raster timing unchanged.

**VRAM interleave (island G):** PHI2 high = CPU may R/W `$FE10`-`$FE12`. PHI2 low = BG fetch owns VRAM. Three HC157 mux low address bits between CPU latch and beam VA (line-buffer uses the other three). Details: [`memory.md`](memory.md).

---

## Signal paths

### Background video

1. Beam PLDs step **341x262** and fetch tile/attr from **VRAM** on PPU phases (L1 slots 0-3).
2. L1 scroll latches (`$FE02`/`$FE03`) offset into the L1 2x2 workbench ([`graphics.md`](graphics.md)).
3. Attr **BANK** picks CHR tile from cart flash. Active palette indices -> **Color PROM** -> DAC.
4. Compositor PLD muxes sprite vs L1 vs L0 (show-through when L1 index is **0**) vs backdrop.

### Second background (L0)

1. Software keeps L0 screens in VRAM slots **4-7** and sets `$FE06`/`$FE07` (often proportional to L1 scroll).
2. Target silicon: **HBlank** fills the next L0 line from slots 4-7 + cart CHR into the linebuf SRAM (sprites use VBlank for a full playfield field so they do not steal HBlank).
3. Emu and Sim Host Play already composite L0 under L1 color 0 from the cart BG0 cache (preview overlay, not pin-level HBlank fill). See [`graphics.md`](graphics.md).

### Sprites

1. CPU fills **OAM** in 1284 via `$FE20`/`$FE21`.
2. Locked split with L0: **VBlank** plots the full **120x128** sprite field. **HBlank** is for L0. Cap **16** sprites per logical scanline.
3. Phase 1 bring-up may still fill one scanline in HBlank until the full VBlank field lands.

### Pads

1. Cabinet / console sticks and buttons feed the **1284**.
2. CPU reads packed bits at **`$FE60`** (P1) and **`$FE61`** (P2). Bit set = pressed ([`graphics.md`](graphics.md)).
3. **Retr01-A:** cabinet microswitches / IDC into the 1284.
4. **Retr01-C:** each pad is a small board with an **ATtiny85** (draft) talking a **3-wire** link (VCC / DATA / GND) into the console. Both console and controllers use **female 3.5 mm TRS** jacks; the cable is any standard **male–male 3.5 mm aux** (user picks length). Software contract stays `$FE60` / `$FE61`.

### Audio

1. CPU writes **`$FE40`-`$FE5F`** (bytecode to 328P). See [`sound.md`](sound.md).
2. Path = PLD decode + CPU HC245 + 328P latch/port. **1284 does not synthesize.**

### Raster IRQ

1. `$FE04` compare value latched in HC573.
2. Y-beam PLD match ---> **IRQB** to 6502.

### Five ATF22V10 roles (target)

| PLD | Job |
|-----|-----|
| Decode | Chip-selects for `$FExx` windows and bus `/OE` |
| VRAM glue | Interleave enable with PHI2 + HC157 AB |
| Beam X | Dot counter / H timing inside 341 |
| Beam Y | Line counter / `$FE04` compare / NMI edges |
| Compositor | Sprite vs L1 vs L0 show-through + Color PROM index |

---

## Protoboard bring-up

Prove **islands** on breadboard before full PCB. Pass = island smoke test, not a shipped game.

```text
A Power --> B Clocks --> C CPU+RAM+PRG --> D $FExx latch --> E Pads
                              |
                              +--> G VRAM interleave --> H Beam --> I BG fetch --+
                              |                                                    |
J Cart flash -----------------+                                                    +--> O RGBS --> P Integration
K 328P APU ------------------------------------------------------------------------+
L 1284 --> M Line buf --> N Sprites ------------------------------------------------+
```

| Island | Pass |
|--------|------|
| **G** VRAM | `$FE10`-`$FE12` R/W, no PHI2 fight |
| **H** Beam | Stable 341x262, NMI stub |
| **N** Sprites | Expected pixels in line buffer |
| **O** Video | RGBS stable at 2x or 1x SCALE |

Full letter list, sim canvas grouping, and port smoke checks: [`retr01_sim/README.md`](../retr01_sim/README.md).

---

## Form factors

| Variant | Shell |
|---------|-------|
| **Retr01-A** | Arcade mobo, RGBS/S-Video/composite, cabinet IDC, 5 V barrel |
| **Retr01-C** | Console, same core, 2x female 3.5 mm TRS pad ports + ATtiny85 pads (draft) |
| **Retr01-H** | Handheld SMD later, same cart contract |

---

## Non-IC components (passives, ports, protection)

ICs are the 32-count BOM above. Everything else exists to **power**, **clock**, **terminate**, **protect**, and **exit** the board. Power assumption: **stable external 5 V** (barrel / PSU). No on-board switching regulator in the baseline.

### Power entry and rail hygiene

External 5 V is trusted for regulation, not for abuse or RF. Treat the barrel as a noisy cable entry.

| Item | Role |
|------|------|
| Barrel jack (2.1 mm class) | 5 V in. Retr01-A / -C |
| Series PPTC on VIN | Board-level short / overload. Hold above full-board idle, trip on hard short |
| Reverse-polarity diode (or P-FET ideal diode) | Blocks reverse barrel plug |
| Bulk cap at entry (**100–470 µF** low-ESR electrolytic or polymer) | Holds rail through plug bounce and load steps |
| Input ferrite (or CMC on 5 V / GND pair) | Damps cable-borne RF before the plane |
| Local **100 nF X7R** on every IC VCC pin (mm from pin) | HF bypass; mandatory for HC / PLD / AVR edge rates |
| Local **1–10 µF** ceramic per island / large DIP | Mid-band reservoir (6502, 1284, 328P, PLD cluster, SRAM bank) |
| Ferrite + **10 µF** into **analog / video** spur | Isolates Color PROM R-2R and APU DAC from digital di/dt |

**Layout:** 4-layer with a solid GND plane. Star or short-fat 5 V pours from entry; never snake return current through video or pad-port copper. Stitch GND vias at every connector shell and under each DIP.

### Clocks and reset (RF-sensitive)

Four domains (**8.000**, **5.369318**, **20**, **16 MHz**) are the primary conducted/radiated emitters.

| Item | Role |
|------|------|
| Canned oscillators (PHI2 8 MHz, dot ~5.369 MHz) | Prefer cans over bare crystals for edge control and lower stray radiation |
| Crystals + load caps for AVRs if not using cans | 20 MHz (1284), 16 MHz (328P); keep loops tiny |
| Series damping **22–47 Ω** on clock nets leaving a can / buffer | Softens edges into long traces; cuts harmonic splash |
| **74HC14** (outside 32-count if needed) | Schmitt cleanup for reset / slow edges |
| RC + Schmitt (or supervisor, e.g. MCP120-class) on `/RESB` and AVR `RESET` | Power-on reset; hold low until 5 V is solid |
| Pull-ups on open-drain resets / IRQB | Typical **4.7–10 kΩ** |

Keep clock traces short, away from cart edge and pad jacks. No unterminated stubs.

### Video and audio analog (outside the 32)

| Item | Role |
|------|------|
| Color PROM **R-2R** ladder (**1%** metal film) | R3G3B2 -> analog guns ([`AT28C16`](../hw/md/AT28C16.md)) |
| **75 Ω** series per R/G/B (+ sync termination as needed) | Drive RGBS into 75 Ω video plant |
| Optional ferrite beads on RGBS | Cable RF; place at connector |
| APU **R-2R** (or PWM RC) from 328P | Line-level mix ([`sound.md`](sound.md)) |
| AC-coupling cap + series build-out on audio out | Blocks DC into TVs / amps |
| Video / AV connectors | Retr01-A: RGBS (+ S-Video / composite path TBD). Levels bench-tuned |

### Cart edge and user I/O ESD

Anything a human can touch gets a clamp **at the connector**, then a series limiter, then the IC.

| Item | Role |
|------|------|
| TVS array (5 V working, e.g. PESD5V0-class) on cart address/data/control as needed | ESD into flash / HC245 domain |
| Series **22–100 Ω** on slow GPIO / pad DATA | Limits IC clamp current; damps cable resonances |
| SCALE DIP + pull-ups/downs | 1x / 2x select; define idle state |

### Retr01-C controller ports (3.5 mm TRS)

Design goal: **female jack on console and on each controller**. The interconnect is a commodity **male–male 3.5 mm aux** cable of any length. No proprietary tether.

| Item | Spec / role |
|------|-------------|
| Jack | **Switchcraft 35RAPC** series, **TRS (stereo)** — e.g. **35RAPC3BH3** (horizontal, threaded bushing) for panel/PCB. Same family on pad PCBs |
| Conductors | **Tip / Ring / Sleeve** = **VCC / DATA / GND** (exact T/R assignment locked at schematic; Sleeve = GND + shell) |
| Port count | **2** (P1, P2) on console |
| Pad MCU | **ATtiny85** draft on the controller board; 1284 still presents `$FE60` / `$FE61` |
| PPTC (Polyfuse) per port on **VCC** | Shorted aux tip–ring or crushed cable must not toast the plane. Size **Ihold** for one ATtiny85 + switches/LEDs (roughly **100–250 mA** class, **Vmax ≥ 6 V**); place on the console **and** consider a mate on the pad board |
| TVS to GND on VCC and DATA at each jack | ESD / hot-plug; PTC alone is too slow for ESD |
| Series **R** on DATA (both ends if practical) | Current limit into MCU pins + RF damping on long aux runs |
| Local **100 nF** on port VCC after the PTC | Decouples the cable stub |

```text
  Console 5 V --[PPTC]--+--[TVS]-- Tip (VCC) ---- aux M-M ---- Tip --[TVS]--+--> pad 5 V
                        |                                                 |
                     100 nF                                              MCU
                        |                                                 |
  GND plane ------------+-- Sleeve (GND) ---------------- Sleeve ---------+
                        |
  1284 / pad bridge ----+--[R]--[TVS]-- Ring (DATA) ---- Ring --[R]--[TVS]--> ATtiny85
```

**Why PTC + TVS:** PPTC covers **sustained shorts** (user cables). TVS covers **nanosecond ESD**. Neither replaces the other.

**RF note:** a long floating aux is an antenna. Keep the on-console DATA run short to the bridge, clamp at the jack, and avoid routing DATA parallel to PHI2 / dot clocks.

### Retr01-A cabinet I/O (contrast)

| Item | Role |
|------|------|
| IDC / discrete wiring to sticks and buttons | Direct GPIO into 1284 (with series R + optional TVS) |
| No 3.5 mm pad ports on the arcade shell | Controllers are built into the cabinet |

### Passive count mindset (planning)

Exact E24 values land at schematic time. Budget order-of-magnitude for a Retr01-C mobo + 2 pads:

- **~40–60×** 100 nF decoupling
- **~10–15×** 1–10 µF island caps + **1×** bulk at barrel
- **R-2R** networks (video + audio) + **75 Ω** build-outs
- **2×** port PPTC + **2–4×** board/entry PPTC/ferrite as needed
- **TVS** packs at cart + both pad jacks (+ audio/video if exposed)
- **4×** Switchcraft 35RAPC TRS (2 console + 1 per controller)
- Oscillators / crystals, reset RC, pull-ups, SCALE DIP, barrel, AV connectors

Passives are **outside** the 32-IC goal.

---

## Open topics

| Topic | Note |
|-------|------|
| RGBS analog tuning | Digital timing set, bench levels TBD |
| Color PROM part speed | AT28C16 150 ns vs faster OTP, 1-dot pipeline |
| HC573 bitfield packing | 9-chip map still TBD in [`graphics.md`](graphics.md) |
| Cart I2C / machine EEPROM API | Mailbox protocol TBD in [`memory.md`](memory.md) |
| Retr01-C pad timing | 3-wire poll edges TBD |
| TRS pin map (T/R = VCC/DATA) | Lock at schematic; document for third-party pads |
| PPTC Ihold per pad port | Bench ATtiny85 + LED budget, then pick family (e.g. Bourns MF-MSMF / Littelfuse 1206L) |
| FCC / CE pre-compliance | 4 clocks + long aux: expect ferrite + clamp iteration on first spin |

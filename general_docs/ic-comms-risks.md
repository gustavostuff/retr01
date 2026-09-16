# IC communication risks

Retr01 is a multi-clock, multi-driver board. The 6502, three AVRs, three PLDs, SRAM, cart flash, glue, and the video path all share nets and must take turns talking. This note catalogs the main failure modes for that symphony, and how to mitigate them.

**Related:** `hardware.md` (wiring, BOM, layout), `../ic_behavior/` (per-chip Inputs / Process / Outputs), `video-graphics.md` (VBlank / HBlank jobs), `memory.md` (CPU map).

## Clock domains at a glance

| Domain | Rate | Who lives here |
| --- | --- | --- |
| **PHI2** | 8.000 MHz (125 ns) | W65C02S, system RAM, soft `$7Fxx`, VRAM CPU half |
| **DOT** | 5.369318 MHz (~186 ns) | Beam PLDs, VRAM beam half, Color PROM, compositor |
| **AVR HFOSC** | 24 MHz internal (each) | MCU-M / S1 / S2 firmware |
| **SPI** | firmware-chosen | M master <-> S1 / S2 slaves |
| **I2C** | firmware-chosen | M <-> cart save (24C64 or I2C FRAM) |
| **Pad UART** | 115200 8N1 | S2 <-> ATtiny85 |
| **FSC** | 3.58 / 4.43 MHz | AD724 only |

These domains are mostly asynchronous to each other. Crossing them without clear ownership, Hi-Z rules, or a stall (`RDY`) is where bugs hide.

## Risk catalog

Severity: **High** = silent bus fight or guaranteed visual/CPU fail if wrong. **Med** = frame glitches, stalls, or hard-to-debug intermittents. **Low** = bring-up pain or edge cases.

### 1. CPU data bus multi-driver fights (High)

**What:** Cart flash, system RAM, VRAM (CPU phase), and MCU-M soft ports can all touch **D[7:0]**. Two drivers at once = X / smoke / random opcodes.

**When it bites:** Soft `$7Fxx` decode overlaps a RAM or PRG select. MCU-M leaves D pins driven after the cycle. Cart `OE#` stays low while RAM `OE#` is also low. Reset / unprogrammed PLD asserts bad `/OE`.

**Mitigate:**

- Keep the three-way rule sacred: PLD `/OE` + cart `OE#` + MCU-M **hi-Z** when not serving soft reads.
- MCU-M default after reset: CPU D pins as inputs / hi-Z until a proven soft-read window.
- Scope D during `$0000`, `$7Fxx`, and `$8000` reads. Only one chip should light up.
- Prefer sockets for PLDs so a bad JEDEC can be swapped without desoldering.

### 2. Soft `$7Fxx` timing vs PHI2 (High)

**What:** MCU-M is a 24 MHz firmware machine answering an 8 MHz synchronous bus. It must see `SEL_SOFT*`, sample `CPU_A_SAMPLE`, and drive or capture D inside the same PHI2 cycle (or assert `CPU_RDY` early enough).

**When it bites:** Late drive on soft reads returns garbage. Late sample on soft writes drops OAM / APU / MAP commands. Missed SEL edges look like "dead I/O".

**Mitigate:**

- Treat soft ports as a hard real-time ISR / CCL / event path, not a polite main-loop poll.
- If the window is too tight, **pull `CPU_RDY` low** before PHI2 fall and finish the work, then release.
- Bring-up: GPIO toggle on SEL edge and on D drive enable. Measure vs PHI2 on a scope.
- Keep SPI / I2C out of the critical soft-read path unless RDY is asserted first.

### 3. `CPU_RDY` and open-drain sharing (Med)

**What:** MCU-M drives `CPU_RDY` open-drain. The W65C02S can also pull RDY low during **WAI**. Long EEPROM / flash / SPI work stalls the whole game CPU.

**When it bites:** Stuck-low RDY freezes the machine. Two drivers fighting a push-pull pin (if wired wrong) damages parts. Holding RDY across VBlank makes S1 miss sprite work relative to game logic expectations.

**Mitigate:**

- Wire RDY as true open-drain / open-collector with a pull-up. Never push-pull from M.
- Bound every RDY-held operation (I2C page write, flash poll, long SPI). Fail safe and release.
- Do not use STP in normal play. Use WAI only with a clear wake source (NMI/IRQ).
- Heartbeat LED on M should still blink on a timer even when RDY is pulsed (proves firmware alive).

### 4. VRAM PHI2 interleave margin (High)

**What:** AS6C62256 #2 is shared. PHI2 high = CPU `$7F10-$7F12`. PHI2 low = BG fetch. Address path goes through **3x 74HC157** plus PLD select. Budget is roughly half of 125 ns minus decode and mux delay, against **55 ns** SRAM.

**When it bites:** Mux/PLD too slow -> wrong nametable bytes, sparkle, or CPU VRAM corruption. HC157 **G** held high forces Y **low** (not Hi-Z), which can look like address 0 stuck.

**Mitigate:**

- Prefer 55 ns SRAM. Keep VRAM mux and decode traces short.
- Verify A/B select tracks PHI2 with clean edges (series 33 ohm already on PHI2).
- Never leave G floating. Confirm G polarity in JEDEC and on the bench.
- Island test: CPU-only VRAM fill with beam fetch disabled, then beam-only with CPU idle, then both.

### 5. Field SRAM AD turnaround (High)

**What:** MCU-S1 multiplexes address and data on **AD[7:0]**. Sequence is: present A[14:0] (low byte via HC573 + ALE), then present data, then `/WE`. PLD owns `/CE`/`/OE`. Beam may read the same chip outside S1's write windows.

**When it bites:** ALE too short / too early. `/WE` overlapping OE from the beam. AD still driving when the PLD enables SRAM OE. Sprite field or BG0 line buffer corruption.

**Mitigate:**

- Firmware state machine with explicit dead cycles between address latch and data write.
- S1 AD pins hi-Z whenever not in an owned write window (especially during active display for regions the beam reads).
- PLD equations: field `/OE` for beam vs S1 `/WE` must be mutually exclusive by design.
- Scope ALE, `/WE`, and one AD bit during VBlank fill and during HBlank BG0 fill.

### 6. HBlank / VBlank job overruns (High)

**What:** S1 must finish the **full sprite field** in VBlank and the **next BG0 line** in HBlank only. HBlank is short. SPI OAM traffic from M steals S1 time if it lands in the wrong window.

**When it bites:** Missed VBlank -> sprite glitches or stale field. Missed HBlank -> BG0 show-through errors on the next line. M flooding SPI during HBlank makes both worse.

**Mitigate:**

- Gate SPI OAM transfers to early VBlank (or when `S1_RDY` says ready). Do not blast OAM in HBlank.
- Budget S1 cycles on paper at 24 MHz before writing firmware. Leave margin.
- Expose `S1_RDY` (already pinned) and have M wait rather than overwrite.
- If BG0 line fill cannot close, reduce BG0 complexity or move work earlier (design change, not a silent hope).

### 7. SPI master / dual-slave contention (Med)

**What:** One SPI master (M) and two slaves (S1, S2) share MOSI/MISO/SCK with separate `/SS`. Wrong `/SS` or both asserted -> bus fight on MISO or confused command streams.

**When it bites:** OAM block lands on S2. Pad/APU traffic corrupts S1 mid-field. Floating `/SS` on a slave makes it listen to the other chip's traffic.

**Mitigate:**

- Exactly one `/SS` low at a time. Idle both high.
- Slave firmware: ignore SPI unless `/SS` is low. Keep MISO hi-Z when deselected (hardware SPI usually does this. Verify).
- Separate message IDs / lengths for S1 vs S2 so a mis-select fails closed.
- Bring-up with only one slave populated first.

### 8. Cart flash vs MAP / CHR / program path (High)

**What:** Cart `CE#` is tied active. Motherboard gates **`OE#`**. `WE#` is idle-high in play and used by the MCU-M flash bridge when the program DIP selects cart. A0-A13 from CPU, A14-A18 from Compositor MAP.

**When it bites:** `OE#` true during a soft-port or RAM cycle (decode bug). `WE#` glitch during play programs random bytes. MAP bits change mid-PRG fetch. Program DIP left on with Friend attached.

**Mitigate:**

- Decode: cart `OE#` only for PRG `$8000-$FFFF` reads and intentional MAP/CHR windows. Never with RAM or soft selects.
- Hardware pull-up on `WE#`. Bridge must actively drive only in cart program mode.
- DIP default **all OFF**. Silkscreen `ALL OFF = SAFE`. Never two DIP positions ON.
- Series 33 ohm on cart D/OE/WE as already planned.

### 9. Color PROM and compositor per-dot path (Med)

**What:** Every DOT (~186 ns) the compositor presents a 6-bit index to the AT27C256R (45 ns) and the resistor DAC. No CPU on this bus, but beam + PROM + AD724 must stay coherent with sync.

**When it bites:** Index changes too late for `tACC`. Unused PROM address pins float. Digital bus noise couples into RGB or FSC.

**Mitigate:**

- Tie unused PROM address lines to GND.
- Prefer a 1-dot index latch in the compositor if fit allows.
- Keep analog island quiet (see layout rules in `hardware.md`). Short RGB/sync to J2/J9.
- Do not poke `$7F08`/`$7F09` outside VBlank.

### 10. Async clocks: PHI2 vs DOT vs AVR (Med)

**What:** CPU and beam are different crystals. AVRs use internal HFOSC. Soft SEL edges, VBL, and beam compares are metastability surfaces.

**When it bites:** Occasional missed NMI, doubled soft writes, or one-pixel sync jitter. Rare and nasty on the bench.

**Mitigate:**

- Synchronize async inputs into each AVR with two flops (or event system) before acting.
- PLD registered paths for beam. Do not sample CPU D on DOT without a PHI2-qualified enable (`LE_7Fxx` already does this for scroll/raster).
- Canned oscillators for PHI2/DOT. Add 74HC14 only if edges are soft.
- Never assume AVR cycle counts equal PHI2 cycles without measuring.

### 11. Reset and power-up bus chaos (High)

**What:** At power-up, AVR GPIO may be inputs or undefined until firmware runs. Blank PLDs do not decode. CPU may fetch before cart OE is sane. AD724 and clocks start at different times.

**When it bites:** Boot fights on D. Running garbage until reset release. One AVR comes up late and drives SPI or AD early.

**Mitigate:**

- Hold **RESB** until clocks are up and a supervisor / RC delay says so.
- External pull-ups/downs on critical enables (`OE#`, `WE#`, `/SS_*`, ALE low, `/WE` high) so undriven means safe.
- Firmware: hi-Z everything shared, then handshake (VBL seen, S1_RDY, etc.) before enabling bus drivers.
- Program PLDs and color PROM before first power-on with CPU populated when possible.

### 12. Pad OD UART and arcade GPIO mix (Low / Med)

**What:** TRS `PAD_DATA` is open-drain with a 4.7 kohm pull-up. Two pads (and later a gun) share the idea of one bus. Arcade microswitches are separate hard GPIOs into S2.

**When it bites:** Pad firmware drives DATA push-pull against the pull-up network. Poll timeout > 200 us starves input. Arcade + TRS both wired with conflicting assumptions in software.

**Mitigate:**

- Pad DATA must be open-drain only. Host PF0 likewise.
- Poll P1 `0x55` then P2 `0xAA` in VBlank with a hard timeout. On timeout, keep last good or clear.
- Document which shell is populated. Do not require pads when arcade headers are the input path.

### 13. I2C save EEPROM stalls (Med)

**What:** 24C64 writes can take milliseconds. A single page program alone is often on the order of **~5 ms**. A full save can span **many frames** (~16.6 ms each at ~60 Hz). MCU-M must not sit on the CPU data bus while waiting unless `CPU_RDY` is low. Cart SDA/SCL share the edge with long stubs.

**When it bites:** Game hitch on save. I2C busy loop without RDY. Holding `CPU_RDY` for the **entire** save freezes the 6502 so PRG cannot animate a spinner (beam would only repeat the last picture). Noise on SDA from parallel cart bus edges.

**Mitigate:**

- Prefer **I2C FRAM** (64 Kbit class, 24C64-protocol-adjacent, often DIP-8 compatible) on the cart when BOM cost allows. FRAM has **no multi-ms internal page program**. Writes complete at bus speed. That removes the main EEPROM charge-pump stall in this item. Transfer time remains: dumping ~8 KB over I2C still costs real milliseconds at 400 kHz / 1 MHz, so a naive full-array write under one long `CPU_RDY` can still freeze PRG for many frames. FRAM turns the problem into bus occupancy, not program wait.
- With 24C64 (or any EEPROM): ACK polling with timeout after each page. Expect ~5 ms program gaps.
- Saves are **explicit** only (pause / fade / dedicated saving screen). Never in the physics hot path.
- **Multi-frame saves are expected and OK.** Chunk I2C work across VBlanks. Use **short** `CPU_RDY` pulses for mailbox handoff or one chunk (EEPROM: one page program/poll; FRAM: a bounded byte burst), then **release RDY** so PRG can run.
- **Keep the picture alive.** Beam / PLDs / S1 keep scanning. PRG updates a spinner (or other saving UI) each frame while the save state machine advances. Do **not** freeze the display for the whole save operation.
- Series 33 ohm on SDA/SCL already planned (layout / stub noise is unchanged by FRAM vs EEPROM).
- Keep machine EEPROM (`$7F70-$7F72`) separate from cart saves so cabinet config cannot brick on a missing cart.

### 14. Scroll / palette mid-frame updates (Med)

**What:** `$7F02` (HC574), `$7F03`/`$7F04` (PLD FFs), and `$7F08`/`$7F09` change beam math and colors. Writes are PHI2-qualified, but changing them mid-active-display is usually wrong.

**When it bites:** Torn scroll. Wrong MAP bank for a slice of the frame. Palette flash.

**Mitigate:**

- PRG convention: update scroll and palette row in NMI/VBlank only (already locked for `$7F08`/`$7F09`).
- Do not clock HC574 from free-running PHI2. Only `LE_7F02`.
- If a split-screen effect is desired later, define it deliberately (raster IRQ + documented rules). Do not invent it by accident.

### 15. Program header vs live bus (Med)

**What:** Adafruit's UPDI Friend on a shared header can talk to an AVR UPDI pin or the cart flash bridge. That path is hostile to a running game bus if enabled by mistake.

**When it bites:** Two DIP switches ON shorts UPDI lines. Cart mode while CPU runs injects WE# cycles. Friend attached with a switch left ON after a session.

**Mitigate:**

- Default all DIP OFF. One ON at a time.
- Firmware: refuse cart-bridge commands unless a strap/DIP read says cart mode.
- Bring-up checklist: verify DIP before power application when Friend is clipped on.

## Priority matrix (what to prove first)

| Order | Prove | Why |
| --- | --- | --- |
| 1 | CPU D ownership (RAM vs PRG vs soft hi-Z) | Without this, nothing else is trustworthy |
| 2 | Reset safe enables | Avoid power-up fights |
| 3 | VRAM PHI2 interleave alone | Video foundation |
| 4 | Soft `$7Fxx` + RDY | MCU-M contract |
| 5 | S1 field ALE/`/WE` in VBlank | Sprites |
| 6 | S1 BG0 HBlank fill | Parallax show-through |
| 7 | SPI M->S1 / M->S2 | Mailboxes |
| 8 | Cart OE# / WE# play vs program | Cart integrity |
| 9 | Pads / I2C / AD724 | Peripherals and analog |

## Design rules of thumb

1. **One talker per net.** Everyone else sleeps in hi-Z or is deselected.
2. **Hard video stays in PLDs + glue.** AVRs help only in VBlank/HBlank windows they own.
3. **If firmware might miss a PHI2 window, use `RDY`.** Do not hope.
4. **Cross clock domains with sync flops or qualified enables.**
5. **Idle-safe pull resistors on enables.** Power-up and crashed firmware should not drive buses.
6. **Measure the budget.** 55 ns SRAM and 45 ns PROM only work if decode + mux + firmware leave margin.

## Performance: what not to do

The rules above are mostly **correctness** fences. Followed carefully, they should **not** slow real gameplay. The beam path stays in PLDs + glue. PRG keeps the NES-style win of not burning the whole VBlank painting the picture.

They **do** hurt frame time or feel hitchy if `RDY` and VBlank become a dumping ground. Avoid the patterns below.

### Free on the play path (do these)

| Practice | Why it is free |
| --- | --- |
| One talker / hi-Z discipline | Stops fights. No extra 6502 cycles |
| Hard video in PLDs + HBlank/VBlank AVR windows | That **is** the performance design |
| Sync flops on `VBL` / `SEL_SOFT*` into AVRs | A few AVR clocks of latency, not game FPS |
| Idle-safe pulls on enables | Power-up / crash safety only |
| 55 ns SRAM / 45 ns PROM with decode margin | Required for clean video, not a software tax |

### Costly if abused (do not)

| Anti-pattern | What goes wrong | Do instead |
| --- | --- | --- |
| Soft `$7Fxx` that **always** needs `CPU_RDY` | Stalls the 6502 on routine I/O every frame | Fix MCU-M so common reads/writes close in one PHI2. RDY is the escape hatch, not the default |
| Holding `CPU_RDY` across long SPI / I2C / flash work during play | Whole game freezes. S1 can miss VBlank work relative to PRG | Bound every RDY hold. Keep play-path SPI short. Saves are explicit only |
| Cart save mid-action with no UI, or one long RDY for the whole save write | Multi-ms hitch (EEPROM program waits, or a long I2C dump), or a **frozen** picture with no spinner | Save on pause / fade / dedicated saving screen. Prefer FRAM to drop program stalls. **Chunk across many VBlanks.** Short RDY only. Keep UI (spinner) updating. Never in the physics hot path |
| Full OAM table SPI every frame, or OAM during HBlank | Steals VBlank from sprite field fill. HBlank BG0 line fails | Dirty / delta OAM in **early VBlank** (or wait for `S1_RDY`). Never in HBlank |
| Late VBlank OAM then expecting a full field rebuild | Sprite glitches or dropped frames of overlay | Publish OAM early enough that S1 still finishes the field before active display |
| Mid-frame scroll / palette writes "because it works on the bench" | Tear, wrong MAP bank slice, palette flash | `$7F02`/`$7F03`/`$7F08`/`$7F09` in NMI/VBlank. Split-screen only via deliberate `$7F04` + IRQ rules |
| Polling pads or I2C in a tight 6502 loop without RDY / VBlank discipline | Wasted PRG time and possible bus stalls | Pads: S2 polls in VBlank. Saves: mailbox + RDY as documented |
| Using **STP** or blind **WAI** in ship PRG | Dead CPU until reset / wrong wake | No STP in play. WAI only with a known NMI/IRQ wake |

### Budget mindset

1. **PHI2 soft I/O:** aim for zero RDY on the hot path. Measure. If RDY is common, that is a firmware bug.
2. **VBlank:** OAM SPI + S1 sprite field must both fit. Leave margin. Do not treat VBlank as infinite DMA time.
3. **HBlank:** BG0 next-line fill only. Nothing else from M or S1.
4. **Saves:** rare, multi-frame OK. Prefer FRAM when practical. Chunk I2C, keep the saving UI alive. Not a substitute for streaming game state every frame.

Author-facing timing locks also live in `software-api.md`. Hardware mailbox locks in `hardware.md` and `video-graphics.md`.

## Related

`hardware.md` | `../ic_behavior/` | `video-graphics.md` | `memory.md` | `cartridge.md` | `software-api.md` | `open-questions.md`

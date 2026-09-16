# Retr01 Tier E — PHI2 and interleaved VRAM

**Prerequisite:** Tier D working (M→S1 OAM SPI; sprites stable).  
**Goal:** Real BG1 from nametable memory under the locked PHI2 interleave, still before a full game CPU soft-I/O stack.

**SoT:** `general_docs/hardware.md`, `general_docs/memory.md` (`$7F10–$7F12`), `general_docs/ic-comms-risks.md` (VRAM PHI2 interleave), `ic_behavior/AS6C62256.md`, `ic_behavior/74HC157.md`.

---

## 1. What to add

| Qty | Part | Role |
| --- | --- | --- |
| 1 | PHI2 canned oscillator | **8.000 MHz**, series ~**33 Ω** |
| 1 | **AS6C62256-55** | Interleaved **VRAM** (#2 role) |
| 3 | **74HC157** | VRAM A[11:0] mux: CPU-side vs beam-side |

**HC157 G** must never float. Held high forces Y **low** (not Hi-Z) — looks like address 0 stuck if mishandled.

**Still omit (preferred):** Full soft `$7Fxx` product, cart OE, long `CPU_RDY` holds. W65C02 may wait for Tier F.

### Lab writer for PHI2-high (until Tier F)

Product path: CPU writes nametables via soft ports `$7F10–$7F12` on PHI2 high.  
Until the 6502 exists, use **one** temporary writer:

- MCU-M bit-banging a minimal `$7F10–$7F12`-style window on PHI2-high only, **or**
- A throwaway AVR / PLD state machine that only fills VRAM

That writer is a **lab stand-in**. Retire it when Tier F owns the ports for real. Do not invent a permanent second CPU on the VRAM bus.

---

## 2. Interleave (locked)

| PHI2 | Owner | Action |
| --- | --- | --- |
| **High** | CPU / lab writer | `$7F10–$7F12` VRAM window |
| **Low** | Beam | BG1 nametable/attr fetch |

Budget: roughly half of **125 ns** minus decode + mux delay, against **55 ns** SRAM. This is a **high** risk domain — prove it with island tests.

---

## 3. Island tests (required order)

1. **CPU-only / writer-only fill** — beam OE disabled; write known pattern; read back offline or via writer.
2. **Beam-only fetch** — VRAM preloaded; PHI2-low beam path alone; Compositor shows nametable art; no writer activity.
3. **Both** — live interleave under composite; sprites from S1 still on top.

Do not jump to (3) first.

---

## 4. Prove

- Stable BG1 from VRAM under live beam.
- S1 sprites still composite correctly (no regression from D).
- Scope: A/B select tracks PHI2 cleanly; no `/OE` fights; HC157 G defined.
- Sparkle or wrong tiles → mux/PLD delay or floating G before blaming the PROM.

---

## 5. Do / don’t

### Do

- Prefer 55 ns SRAM; keep mux + PLD path short on breadboard.
- Verify series termination on PHI2.
- Keep OAM SPI in early VBlank; do not steal HBlank for VRAM experiments on S1.

### Don’t

- Don’t hang cart flash OE on this tier.
- Don’t hold RDY across multi-ms work “to make VRAM easy.”
- Don’t leave HC157 enables floating.
- Don’t call the lab writer the final soft I/O design.

---

## 6. Exit criteria → Tier F

- Boring demo: BG1 tile map from VRAM + sprites from field.
- Island tests 1–3 all pass with notes on measured margins.

**Next:** [tier-f-cpu-soft-io.md](tier-f-cpu-soft-io.md) — W65C02 + soft `$7Fxx` via M; retire the VRAM lab writer.

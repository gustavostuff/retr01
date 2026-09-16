# Retr01 Tier G — Cart and MAP

**Prerequisite:** Tier F working (6502 + soft `$7Fxx` + VRAM BG1 + sprites).  
**Goal:** Passive cartridge as the software delivery vehicle: PRG fetch, MAP stream into VRAM, optional save IC.

**SoT:** `general_docs/cartridge.md`, `general_docs/memory.md`, `general_docs/hardware.md` (cart edge / OE#), `general_docs/ic-comms-risks.md` (cart OE / saves), `ic_behavior/SST39SF040.md`, `ic_behavior/24C64.md`.

---

## 1. What to add

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **SST39SF040** (or lab equivalent) | Cart flash on a cart-like footprint |
| — | Mobo OE# gate | PRG `$8000–$FFFF` and intentional MAP/CHR windows only |
| — | MAP port | `$7F90–$7F93` (seek + auto-inc data) via M/PLD as designed |
| 0–1 | **24C64** or FRAM | Optional saves (chunked; short RDY; UI alive) |

**CE#** tied active on the cart. Motherboard gates **`OE#`**. **`WE#`** idle-high in play (board pull-up); used by the MCU-M flash bridge when the program DIP selects cart.

**Still omit:** MCU-S2 / pads / audio (Tier H). No mapper ICs.

### Flasher (can stay partial)

Adafruit UPDI Friend + 4-pos DIP (M / S1 / S2 / cart). Default all OFF. Scope: **AVRs + cart only**. PLDs / color PROM stay pre-programmed or external tools.

---

## 2. Prove

1. Boot from cart PRG (replace Tier F temporary PRG source).
2. MAP streams a screen into VRAM; no OE fight with RAM/soft.
3. Scope: cart OE asserted only for intended windows; WE# never floating low in play.
4. Optional save path: chunked across VBlanks; short `CPU_RDY` only; spinner/UI can still update (see `ic-comms-risks.md`).
5. Boring demo: `.retr01`-style image boots and shows authored BG + entities driven by PRG.

---

## 3. Do / don’t

### Do

- Keep A0–A13 from CPU; A14–A18 from Compositor MAP.
- Fail closed on OE decode mistakes (prefer no fetch over a fight).
- Prefer FRAM if EEPROM program stalls hurt the save demo.

### Don’t

- Don’t invent mappers or PRG banking.
- Don’t leave WE# floating low.
- Don’t freeze the machine under one long RDY for a full save write.
- Don’t combine pad bring-up into this tier.

---

## 4. Exit criteria → Tier H

- Cart boots a test `.retr01` (or equivalent) with MAP-fed BG and PRG-driven entities.
- OE/WE play-vs-program story understood on the bench.
- Temporary Tier F PRG substitute removed.

**Next:** [tier-h-pads-audio.md](tier-h-pads-audio.md) — MCU-S2 pads + PWM audio.

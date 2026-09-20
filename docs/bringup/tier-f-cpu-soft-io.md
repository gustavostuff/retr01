# Retr01 Tier F - W65C02 and soft `$7Fxx`

**Prerequisite:** Tier E working (interleaved VRAM BG1 + sprites). 
**Goal:** Game CPU and the hard real-time soft-I/O path, using **test PRG** - not a full cart yet.

**SoT:** `docs/general/memory.md`, `docs/general/hardware.md`, `docs/general/ic-comms-risks.md` (soft `$7Fxx` / `CPU_RDY`), `docs/general/software-api.md`, `docs/ic_behavior/W65C02S.md`, `docs/ic_behavior/AVR128DB28.md`.

---

## 1. Parts added

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **W65C02S** | Game CPU @ PHI2 8.000 MHz |
| 1 | **AS6C62256-55** | **System RAM** (#1): `$0000-$7EFF` |
| - | MCU-M | Soft `$7Fxx` device: Hi-Z on D unless selected. **`CPU_RDY`** open-drain only when needed |
| - | Compositor/PLD | Live `SEL_SOFT*`, `LE_7F02` / `03` / `04` (stubs from Tier B become real) |
| 0-1 | **74HC574** | Scroll X `$7F02` if not hardwired to 0 |

**Still omit:** Cart flash as the sole PRG source (Tier G), MCU-S2, pads, audio.

### PRG without a cart (lab options)

Pick **one** temporary PRG source:

1. **System RAM image** loaded offline (UPDI/host) with PLD decode presenting RAM (or a mirror) at `$8000-$FFFF` for bring-up only, **or**
2. **Socketed EPROM / flash substitute** on a cart-like footprint with OE gated like product PRG

No mappers or banking. Flat 32 KB PRG remains the contract.

Retire temporary decode when Tier G's SST39SF040 owns `$8000-$FFFF`.

---

## 2. Soft I/O rules (locked)

- M drives CPU D **only** when soft-selected for a read. Otherwise **Hi-Z**.
- Aim for **zero `CPU_RDY`** on the hot path (scroll, OAM mailbox, palette row, common soft regs). RDY is the escape hatch, not the default.
- If the PHI2 window is too tight: assert **`CPU_RDY`** before PHI2 fall, finish, release. Bound every hold.
- Do **not** run SPI or I2C inside a soft-read cycle unless RDY is already low.
- Scroll `$7F02`/`$7F03` and palette `$7F08`/`$7F09`: update in **NMI / VBlank** only.
- Do **not** use STP in play-path test PRG.

---

## 3. Prove

1. Reset vectors, NMI from VBlank, tight loop writes scroll / OAM mailbox / palette row in VBlank only.
2. Common soft I/O **without** RDY on the hot path (measure with GPIO toggles on SEL and D-drive enable vs PHI2).
3. Scope: only one driver on D for `$0000`, `$7Fxx`, `$8000` (PRG source).
4. 6502-authored motion of Tier C/D sprites and camera/scroll. M/S1 remain video assist.
5. Retire Tier E's VRAM lab writer - CPU owns `$7F10-$7F12`.

---

## 4. Rules

### Required

- Decode islands come up separately: RAM alone, soft alone, PRG alone, then combined.
- OAM SPI stays early VBlank / `S1_RDY`.
- Soft-cycle timing vs PHI2 is measured and documented.

### Forbidden

- Cart OE fights on this tier.
- Pad polling (no S2 yet).
- RDY held across full OAM SPI or multi-ms work.
- Mid-frame scroll/palette writes "because the bench looks fine."

---

## 5. Exit criteria -> Tier G

- Boring demo: 6502 moves player sprite / camera against VRAM BG1 + field sprites.
- Soft hot path mostly RDY-free under the test PRG.
- Single-driver rules verified on the scope.

**Next:** [tier-g-cart-map.md](tier-g-cart-map.md) - passive flash cart, MAP stream, optional save IC.

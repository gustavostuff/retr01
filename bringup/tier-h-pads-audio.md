# Retr01 Tier H - Pads and audio

**Prerequisite:** Tier G working (cart PRG + MAP + video path). 
**Goal:** MCU-S2 brings controllers and PWM audio online so the counted motherboard set is exercised end-to-end.

**SoT:** `general_docs/hardware.md` (pads, S2), `general_docs/sound.md`, `general_docs/memory.md` (`$7F60`/`$7F61`, APU window), `general_docs/ic-comms-risks.md`, `ic_behavior/AVR128DB28.md`, `ic_behavior/ATtiny85.md`.

---

## 1. Parts added

| Qty | Part | Role |
| --- | --- | --- |
| 1 | **AVR128DB28** | **MCU-S2** @ 24 MHz HFOSC |
| - | Pad path | TRS open-drain UART and/or arcade GPIO |
| - | Audio | S2 PWM (TCA WO1 on **PF1**) toward board audio out |
| - | SPI | S2 slave under M for pad mailbox / APU window (`/SS_S2`) |

Optional outside the 19: **ATtiny85** in the TRS pad (poll reply `< 200 us`).

SPI rule remains: exactly one of `/SS_S1` or `/SS_S2` low at a time; idle both high.

---

## 2. Why last

Input and sound are orthogonal to video/CPU/cart bring-up. S2 is the remaining counted AVR. Putting H last avoids pad/audio noise while PHI2 soft I/O and cart OE are still unstable.

---

## 3. Prove

1. **Pads:** P1/P2 bits affect PRG. Poll pattern `0x55` / `0xAA` in VBlank with a hard timeout; on timeout keep last good or clear.
2. **Idle-safe DATA:** open-drain only on pad DATA; no push-pull fights.
3. **Audio:** at least a beep / pulse line through the real `$7F40`-class / S2 PWM path (full tracker depth can wait).
4. Boring demo: playable test cart with real pad + sound + composite out.

---

## 4. Rules

### Required

- Pads are polled in VBlank (S2), not in a tight 6502 spin without discipline.
- OAM stays on `/SS_S1`. Pad/APU stays on `/SS_S2`, with distinct message IDs.
- SPI to S2 stays bounded. S1's HBlank BG0 window is not stolen.

### Forbidden

- Audio DMA fantasies on S1 that block video windows.
- Push-pull drive on the TRS DATA line.
- Light gun / FRAM preference / 4-layer PCB spin as Tier H exit criteria. Those are post-H polish.

---

## 5. Exit criteria (end of breadboard path)

- Counted motherboard set exercised end-to-end: playable test cart, pads, audio, composite out.
- Cross-cutting rules from [README.md](README.md) still hold under the full stack.

**After H:** PCB spin, shell, and software depth are separate tracks. Optional polish stays post-H.

# Sound (APU)

MCU-S2 owns audio. Soft mailbox **`$7F40`-`$7F5F`** is an **8 voice x 4 register** window. MCU-M forwards PRG writes over SPI. S2 turns the register file into **PWM** on **PF1**.

## Soft window

| Range | Role |
| --- | --- |
| `$7F40`-`$7F5F` | 32 bytes. Voice `V` uses `$7F40 + 4*V` .. `$7F43 + 4*V` |

Exact per-voice field meanings (volume, duty, period, noise mode, and so on) stay with firmware / Studio BGM export. Host Play may preview with a softsynth. That preview is **not** the cart protocol.

## Timing

- Prefer APU mailbox updates in **NMI / VBlank**, same discipline as OAM SPI.
- Do not burn active-display PRG time on long APU streams.

## Related

- Pad / PWM pinout: `hardware.md`
- Soft SEL2 family: `hardware.md`, `memory.md`
- Comms risks: `ic-comms-risks.md`

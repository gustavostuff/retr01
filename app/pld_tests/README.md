# PLD fit tests (ATF22V10 / GAL22V10)

Headless galette harness for soft `$FExx` / PLD register equations. Not part of Sim.
Sim models behavior. This pack checks whether equations **fit** the 22V10
product-term map, plus behavioral scenarios for decode / registers / EQ.

## Quick start

```bash
./app/pld_tests/ensure_galette.sh   # once (needs cargo + network)
./app/pld_tests/run_tests.py
```

Or via `./unit-tests` (includes this suite).

## Layout

| Path | Role |
|------|------|
| `equations/expect_fit/` | Real Retr01 equation shapes that must produce `.jed` |
| `equations/expect_fail/` | Intentional overflow (harness must reject) |
| `retr01_pld/` | Galette runner + behavioral models |
| `tests/` | Unit tests |
| `ensure_galette.sh` | Clone/build [galette](https://github.com/simon-frankau/galette) into `.cache/` |

## What is covered

- **Fit:** UPLDA SEL shares, scroll X/Y regs, raster Q regs, MAP A14-18, cascaded 8-bit EQ
- **Overflow:** flat 8-bit XOR-OR on one OLMC (must fail)
- **Scenarios:** every decoded `$FExx` port, shared SEL pins, read/BE/FE qualifies, scroll/MAP load-hold, full 8-bit EQ space

Galette targets **GAL22V10** fuse maps (compatible with ATF22V10C class). Official WinCUPL remains the release gate when you have it.

**Not covered yet:** co-fitting full beam counters / VRAM interleave / compositor glue with these register blocks on the same die. Add those equations to `expect_fit/` when they exist. Escape remains +1 PLD if that combined fit fails.

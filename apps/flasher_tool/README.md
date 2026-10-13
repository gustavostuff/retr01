# Retr01 flasher

Keyboard menu for the Tier A, B, and C burns.

Programmers:

- XGecu T48 or TL866II Plus. Color PROM, Beam X, Beam Y, and Compositor. The tool calls `minipro`.
- HiLetgo CH340 USB-serial lead, wired as SerialUPDI to AVR pin 19. MCU-S1. The tool calls `pymcuprog`.

From the repo root:

```bash
python3 apps/flasher_tool/flasher.py
```

`minipro` is the AUR package `minipro-git` (`pamac build minipro-git`). `pymcuprog` is the `python-pymcuprog` package.

`--virtual` programs in-memory chips and does not open a programmer. Up and Down move the highlight. Enter starts the burn. q quits.

`--check` only reports whether the T48 and the CH340 lead are both visible. It does not open the menu and it does not burn a chip. Exit status is 0 when both are ready.

```bash
python3 apps/flasher_tool/flasher.py --check
```

The color PROM row stops unless the chip reads blank, then waits for `y`, because that part is one-time. Beam X and Beam Y use the JEDEC files under `docs/misc/irl_bring_up_log/firmware/a/`. The Compositor row expects `firmware/b/compositor.jed`. The MCU-S1 row expects `firmware/c/mcu_s1.hex`. A missing file ends that burn before a programmer is told to write. A failed burn stays on the menu with the programmer's own error under `write failed` or `verify failed`.

`verify.py` is the read-back for the same five rows and the same two programmers. Up and Down move the highlight. Enter compares the chip in the socket with the image file and prints `match` or `mismatch`. The script does not write a chip. `--virtual` compares the in-memory chips.

```bash
python3 apps/flasher_tool/verify.py
```

```bash
python3 -m unittest apps/flasher_tool/test_flasher.py
```

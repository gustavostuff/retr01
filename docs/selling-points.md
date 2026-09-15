# Selling points

## One PCB for console and arcade

A single motherboard footprint set covers **home console** and **arcade** installs. Populate TRS pad jacks, arcade microswitch headers, or both. Shell and BOM choose the path. No separate A/C SKUs.

## Dual sync RGB + composite

One RGB header supports **CSYNC** or **H/V sync** (jumper/cable mode) into a frozen **AD724** composite path. Cabinets and RGBHV upscalers share the same connector family.

## Hardware does the picture

True dual BG planes (BG1 over BG0 show-through), hardware scroll, cart MAP/nametable streaming, sprites filled in **VBlank**, BG0 line prep in **HBlank** ping-pong. PRG stays on game logic.

Multi-chip board. That's CPU + AVRs + PLDs + 74xx glue (~17 ICs on main PCB), not an FPGA soft console and not pure TTL discrete logic. See the programmable-vs-fixed table in `hardware.md`.

## NES-sharp pixels, SNES-true parallax

Art stays in the crisp, color-reduced NES lane (tight palettes, readable 8x8 tiles). Depth is real SNES-style hardware parallax: two BG planes with independent scroll (BG1 over BG0 show-through), not a fake layered blit in software.

## Single 32 KB PRG region

One flat **32 KB PRG** window on the cart. No PRG banking. That is the same PRG size class as classic NES NROM games (Excitebike, Balloon Fight, Ice Climber), but those 32 KB go a lot farther on Retr01.

**Faster CPU.** The W65C02 runs at **8 MHz**, not the NES NTSC ~1.79 MHz. More cycles per frame means more game logic, AI, and physics in the same PRG budget.

**More RAM.** System RAM is about **32 KB** (`$0000-$7EFF`), not the NES **2 KB**. Live entity state, buffers, and helpers can sit in RAM instead of burning PRG on clever packing and constant reloads.

**NES games burned VBlank on the picture.** A large share of many NES titles (often **40-50%** of VBlank time) went into making the picture appear and move: OAM updates, scroll writes, nametable streaming, timed mid-frame tricks. That work ate the same CPU and PRG that also had to run the game.

**Retr01 pushes video off PRG.** Work the NES piled onto the 6502 (OAM, scroll, nametable traffic) is handled here by PLDs, glue, and the helper AVRs: hardware scroll, cart MAP/nametable streaming, and sprites filled in **VBlank** by MCU-S1. On top of that, Retr01 adds dual BG planes with BG0 show-through and **HBlank** BG0 line prep, which the NES never had. Authors spend the same 32 KB mostly on play and behavior, not on fighting the display.

## Entity system with clear budgets

Authors think in **entities** (up to 4 states x 4 frames x 4 sprites), not raw sprites. Definitions are cart data. Behavior is C/ASM in PRG.

Studio-friendly hard cap: **16** entity types **per world**. Each world blob carries its own catalog and SPR CHR. Packed defs use offset tables so PRG can seek state S / frame F (see `software-api.md`).

## Console programs its own carts

One DIY tool: **Adafruit's UPDI Friend**, clipped onto one shared set of motherboard male pins (no USB on console/cart/pads). A **4-pos DIP** picks MCU-M / S1 / S2 / cart. Default **all OFF** so nothing is armed. Scope is **AVRs + cart only**. PLDs, color PROM, and pad MCUs: buy **pre-programmed**, or DIY with a separate tool (Arduino Nano/Uno GAL programmers, TL866-class, Arduino-as-ISP). See `hardware.md`.

## Through-hole DIY friendly layout

Test points for bring-up, full-size THT status LEDs, initial **2-layer** PCBs (mobo / cart / pads), and the usual decoupling / short-clock / keep-off-the-edge rules. A **4-layer** mobo stays an option later only if bring-up or a commercial SMD spin needs it. See `hardware.md`.

## Three helper AVRs

MCU-M (soft I/O, saves, SPI), MCU-S1 (sprites + BG0 HBlank), MCU-S2 (pads + audio). Clear ownership, room to grow firmware without starving PRG.

## Light-gun ready hit detect (later)

The beam path is sketched so a light gun can land later without redesigning the console. Hit detect paints a black anti-spoof frame, then **all target hitboxes as white rectangles in one frame**, then reads the gun timer (`0x5A` / proposed `$7F80`-`$7F81`). That is a roadmap feature, not a v1 ship requirement. See `hardware.md`.

## Closing note

The NES already had memorable games on plain NROM. Retr01 is not trying to erase that bar. The goal is better plumbing for developers (less time fighting the picture, more time on play) while keeping an NES + SNES feel: crisp color-reduced pixels and true dual-plane parallax. Plus other QoL features here and there.

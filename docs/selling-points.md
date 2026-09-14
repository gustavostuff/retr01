# Selling points

Bells and whistles of the Retr01. Add new items here as they land.

## One PCB for console and arcade

A single motherboard footprint set covers **home console** and **arcade** installs. Populate TRS pad jacks, arcade microswitch headers, or both. Shell and BOM choose the path. No separate A/C SKUs.

## Dual sync RGB + composite

One RGB header supports **CSYNC** or **H/V sync** (jumper/cable mode) into a frozen **AD724** composite path. Cabinets and RGBHV upscalers share the same connector family.

## Hardware does the picture

True dual BG planes (BG1 over BG0 show-through), hardware scroll, cart MAP/nametable streaming, sprites filled in **VBlank**, BG0 line prep in **HBlank** ping-pong. PRG stays on game logic.

Multi-chip board (CPU + AVRs + PLDs + 74xx glue), not an FPGA soft console and not pure TTL discrete logic. See the programmable-vs-fixed table in `hardware.md`.

## Single 32 KB PRG region

One flat **32 KB PRG** window on the cart. No PRG banking.

That is the same PRG size class as classic NES NROM games (Excitebike, Balloon Fight, Ice Climber), but Retr01 can do more with it. Hardware and helpers handle most of the video work that NES games often burn vblank on. The CPU is faster and system RAM is larger, so more of those 32 KB stay on game logic.

| Topic | NES (NROM-class) | Retr01 |
| --- | --- | --- |
| PRG seen by the CPU | Up to 32 KB, flat | 32 KB, flat (no PRG banks) |
| Typical CPU clock | ~1.79 MHz (NTSC) | 8 MHz W65C02 |
| System RAM | 2 KB | ~32 KB (`$0000-$7EFF`) |
| Who drives most of the picture | CPU in vblank and timed code | Hardware scroll, cart MAP streaming, video glue, AVR helpers |
| Sprite / entity heavy lifting | CPU builds and feeds OAM | CPU entities + OAM. MCU-S1 fills sprite field in vblank |
| What 32 KB of PRG is for | Game logic **plus** a lot of video housekeeping | Mostly game logic and behavior |

NES already shipped complete, polished games on NROM with those limits. That bar matters. Retr01 turns many of those limits into polished plumbing so authors spend the same PRG budget on play instead of fighting the display.

## Entity system with clear budgets

Authors think in **entities** (up to 4 states x 4 frames x 4 sprites), not raw sprites. Definitions are cart data. Behavior is C/ASM in PRG.

Studio-friendly hard cap: **128** entity types in one **global** catalog. Worlds share that catalog (place A/B/C in world 1 and C/D/E in world 2). Packed defs use offset tables so PRG can seek state S / frame F (see `software-api.md`).

## Console programs its own carts

One DIY tool: **Adafruit's UPDI Friend**, clipped onto one shared set of motherboard male pins (no USB on console/cart/pads). A **4-pos DIP** picks MCU-M / S1 / S2 / cart. Default **all OFF** so nothing is armed. Builders may also flash AVRs on a breadboard before soldering. No separate flasher PCB. Header pin numbers and protocol TBD.

## Through-hole DIY friendly layout

Test points for bring-up, full-size THT status LEDs, motherboard **4-layer** ground planes, cart/pad **2-layer**, and the usual decoupling / short-clock / keep-off-the-edge rules. See `hardware.md`.

## Three helper AVRs

MCU-M (soft I/O, saves, SPI), MCU-S1 (sprites + BG0 HBlank), MCU-S2 (pads + audio). Clear ownership, room to grow firmware without starving PRG.

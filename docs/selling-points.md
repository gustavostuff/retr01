# Selling points

Bells and whistles of the Retr01. Add new items here as they land.

## Single 32 KB PRG region

One flat **32 KB PRG** window on the cart. No PRG banking.

That is the same PRG size class as classic NES NROM games (Excitebike, Balloon Fight, Ice Climber), but Retr01 can do more with it. Hardware and helpers handle most of the video work that NES games often burn vblank on. The CPU is faster and system RAM is larger, so more of those 32 KB stay on game logic.

| Topic | NES (NROM-class) | Retr01 |
| --- | --- | --- |
| PRG seen by the CPU | Up to 32 KB, flat | 32 KB, flat (no PRG banks) |
| Typical CPU clock | ~1.79 MHz (NTSC) | 8 MHz W65C02 |
| System RAM | 2 KB | ~32 KB (`$0000-$7EFF`) |
| Who drives most of the picture | CPU in vblank and timed code | Hardware scroll, cart MAP/nametable streaming, video glue |
| Sprite / entity heavy lifting | CPU builds and feeds OAM | CPU entities + OAM. MCU-S1 fills sprite field in vblank |
| What 32 KB of PRG is for | Game logic **plus** a lot of video housekeeping | Mostly game logic and data tables |

NES already shipped complete, polished games on NROM with those limits. That bar matters. Retr01 turns many of those limits into polished plumbing so authors spend the same PRG budget on play instead of fighting the display.

## Entity catalog headroom

No hard limit on how many different entity **types** you put on a cart. On a max-filled world/screen/CHR cart there is still enough flash for **more than 100** distinct entities with space to spare. Per-world sprite CHR without tile reuse tops out around **16** fully unique maxed entities. Definitions are world-scoped data. Behavior is PRG code in C/ASM. Details in `memory.md` and `software-api.md`.

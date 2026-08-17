# Retr01-C — Console Edition

Living-room variant; same core as Retr01-A (CPU, interleaved 32 KB VRAM, 32 KB system RAM, CHR from cart, 2bpp, memory map). Differences are home I/O and video.

**Status:** After Retr01-A proves the core.

## Board

Through-hole / socketed DIP for early revisions; 2–4 layer PCB; mini-ITX or custom shell.

## Video

Internal 256×240 2bpp. Nearest-neighbor upscale toward HDMI/DVI; legacy analog paths as needed.

## Controllers

DB-9 and/or USB — **primary port TBD**. Latched each frame (`$7F60`-class I/O).

## Power

Barrel jack and/or USB-C PD → 5V / 3.3V rails.

## Shared

[02_graphics_and_cartridge.md](02_graphics_and_cartridge.md), [08_memory_map.md](08_memory_map.md), NES-style APU protocol.

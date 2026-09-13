# Retr01 overview

Retr01 is a home console and arcade board in one. GPIO drives arcade microswitches. Two players can also use controllers over 3.5mm jacks (three cables per pad).

## Inspirations

1. NES (tile and sprite feel, 2bpp, NES-style color limits).
2. SNES (true parallax via two BG planes).
3. [GameTank](https://gametank.zone/) (similar screen resolution target).

## Design goals

- Hardware does most video work so PRG can stay on game logic.
- Hardware scroll and MAP/nametable streaming from the cart.
- Start small on software features. Grow the API later without locking the silicon early where we can avoid it.

## Doc map

Hardware details live in `hardware.md` (3x AVR128DB28, PLDs, BOM). CPU map and `.retr01` image in `memory.md`. Video and palettes in `video-graphics.md`. World layout and scrolling in `world-scrolling.md`. Cart physical notes in `cartridge.md`. Entity API and modes in `software-api.md`. Open decisions in `open-questions.md`.

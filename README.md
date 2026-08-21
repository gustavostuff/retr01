<img src="docs/img/logo_v1.png" alt="Logo V1" />

**A modern discrete-logic 8-bit hardware family, built to be understood, hacked, and shipped.**

Retr01 is a family of purpose-built retro game hardware. Same creative rules, same feel, three form factors over time:

| Device/PCB | Description |
|---|---|
| **Retr01-A** | Arcade motherboard, the first build. Uses THT components and doesn't worry too much about board/PCB size  |
| **Retr01-C** | Home console. We'll have to mind the board size for this one, have different control ports, among other things |
| **Retr01-H** | Handheld. This is the most challenging task. Will use SMD components and probably more than one board |

We're starting with the arcade board: something you can drop into a cabinet, wire to real controls, and run games that look and play like classic 8-bit tile/sprite games - with a world model and CPU budget built for larger designs.

For the full product pitch and **NES comparison tables**, see [`docs/07_pitch.md`](docs/07_pitch.md).

## Why it exists

Most retro projects either emulate the past exactly or leave the aesthetic behind. Retr01 keeps the 8-bit look (8x8 tiles, 2bpp art, cartridge games) and redesigns the plumbing for easily navigable, multi-world games, multi-screen worlds, smoother scrolling and simple parallax integration, without VBlank-only nametable updates.

Details and fair NES comparisons: [`docs/07_pitch.md`](docs/07_pitch.md).

## What you get (in plain terms)

- **A real arcade-first board:** sticks, buttons, coin/start, analog RGBS/S-Video/composite pads for cabinet monitors. RGBS pads can also feed an off-the-shelf analog-to-HDMI converter if you want a modern TV
- **A bold but readable look:** limited color per tile/sprite on purpose, with clarity over mush
- **Big cartridge worlds:** multiple "worlds," dozens of screens each, packed into a standard-size cart
- **Chunky 128x120** playfields (**16x15** tiles) with board **2x** into a **256x240** RGBS raster (fills CRT; no letterbox)
- **Smooth multi-screen scrolling:** cameras that cross screen borders using a 2x2 live nametable window (see pitch doc)
- **Retr01 Studio:** visual tool to author worlds. Later phases compile `.retr01` cart images

Later editions (console and handheld) share the same soul: one architecture, different shells.

## Project status

Retr01 is in **architecture & documentation** phase. The living spec lives in [`docs/`](docs/). **Retr01 Studio** - the visual authoring and compile tool - is the next coding focus. A low-level hardware emulator is planned later, not in active development yet.

---

Built for people who want to *make* 8-bit games, not only play them.

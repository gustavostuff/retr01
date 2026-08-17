# Retr01

**A modern discrete-logic 8-bit arcade platform, built to be understood, hacked, and shipped.**

Retr01 is a family of purpose-built retro game hardware. Same creative rules, same feel, three form factors over time:

| Model | Description |
|---|---|
| **Retr01-A** | Arcade motherboard, the first build. Uses THT components and doesn't worry too much about board/PCB size  |
| **Retr01-C** | Home console. We'll have to mind the board size for this one, have different control ports, among other things |
| **Retr01-H** | Handheld. This is the most challenging task. Will use SMD components and probably more than one board |

We're starting with the arcade board: something you can drop into a cabinet, wire to real controls, and run games that look and play like classic 8-bit, without the usual "wait for blanking or your graphics explode" pain.

## Why it exists

Most retro projects either emulate the past or trap you in its worst limits. Retr01 keeps the charm: crisp tile art, tight sprites, that unmistakable low-bit look. It redesigns the plumbing underneath so developers get a full frame of breathing room for game logic, scrolling, and streaming levels from the cartridge.

Think: **NES-era aesthetics with room to actually build ambitious games.**

## What you get (in plain terms)

- **A real arcade-first board:** sticks, buttons, coin/start, analog RGB for cabinet monitors
- **A bold but readable look:** limited color per tile/sprite on purpose, with clarity over mush
- **Big cartridge worlds:** multiple "worlds," dozens of screens each, packed into a standard-size cart
- **Smooth multi-screen scrolling:** cameras that cross screen borders without the classic flicker tax
- **A path from software to silicon:** prove a game in a strict low-level emulator, then flash it to hardware

Later editions (console and handheld) share the same soul: one architecture, different shells.

## Project status

Retr01 is in **architecture & documentation** phase. The living spec lives in [`markdown_v_01/`](markdown_v_01/). Hardware bring-up and the low-level emulator come next. Polished toolchains and sample games follow once the core is solid.

## Name note

You may still see the older working title **GameNerd** in paths or early notes. The project name is **Retr01**.

---

Built for people who want to *make* 8-bit games, not only play them.

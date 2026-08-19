# Retr01 Studio

Retr01 Studio is the **only software tool in active development** for now.

It is a visual authoring application for creating and compiling Retr01 games. It produces cartridge-ready output: **PRG + CHR + MAP** in a `.retr01` cart image, aligned with the architecture docs in this folder.

## Role

Retr01 Studio is the path from art and level design to a playable cart image:

- paint screens and sprite tiles
- arrange worlds and screens on the virtual grid
- assign CHR banks, palette banks, and palette rows
- export MAP data with the locked RLE and directory formats
- build / compile game projects into `.retr01`

It is **not** the hardware emulator. It is the tool developers use first.

## Phased delivery

Studio will be built in phases. Later phases depend on earlier ones, but each phase should produce something usable on its own.

| Phase | Focus | Outcome |
|-------|-------|---------|
| **0 — Core formats** | Shared library for palette, screen, MAP, RLE, cart I/O | Correct encode/decode of Retr01 data formats |
| **1 — Screen authoring** | Screen painter, CHR packing, palette editing | One screen can be authored and exported |
| **2 — World authoring** | World grid, screen directory, sparse atlas export | Multi-screen worlds export to MAP-ROM |
| **3 — Build / compile** | Project format, PRG template or cc65 integration, cart assembly | Studio builds a full `.retr01` cart |
| **4 — Preview and polish** | In-tool preview, validation, workflow UX | Edit → build → verify loop inside Studio |

Exact UI layout and file formats for the Studio project file are still open. The **hardware contract** (MAP layout, palette banks, CHR banks, memory map) is defined in the other docs and Studio must follow it.

## What Studio owns vs what it does not

**Studio owns:**

- authoring UX
- project files
- export and build pipeline
- validation against Retr01 caps (worlds, screens, banks, palette rows)

**Studio does not own (for now):**

- motherboard schematic capture
- gate-level simulation
- low-level hardware emulator

## Future software (planned, not current work)

A **low-level Retr01 emulator** is planned later. It is **not** the current coding target.

When it exists, it should be hardware-faithful:

- cycle-oriented CPU stepping
- real `$0000-$FFFF` decode and `$FExx` register behavior
- interleaved VRAM phase rules
- cart PRG / CHR / MAP loading as on real hardware

That emulator will consume the same `.retr01` carts that Retr01 Studio produces. Studio docs and format code should stay compatible with that future goal, but emulator implementation waits until after Studio reaches a useful build/export path.

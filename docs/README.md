# Retr01 Documentation (`markdown_v_01`)

Canonical rewrite of the scattered Gemini drafts under `gemini_docs/`. This folder is the **source of truth** until a later version supersedes it.

**Project name:** Retr01 (formerly GameNerd / Retr02).  
**Initial hardware target:** Retr01-A (arcade board).  
**Near-term software focus:** architecture docs, then **Digital (HNeemann) bring-up** ([16](16_simulation_and_bringup_plan.md)), then rewrite the low-level C emulator. Schematic AIs wait.

## Terminology (canonical)

Use these words consistently across all docs in this folder.

| Term | Meaning | Not the same as |
|------|---------|-----------------|
| **World** | One cart chapter: MAP atlas + **4 BG banks + 4 sprite banks** in CHR. Up to **8** worlds (0–7). `$FE30` world select. | A screen, a nametable slot, or a grid position |
| **Screen** | One stored **32×30** nametable (+ attrs) in MAP-ROM. Has a **(col, row)** **grid position** on the virtual grid. May be **playfield** or **parallax**. | A nametable slot, a BG bank, or a sprite bank |
| **Virtual grid** | Up to **16×16** atlas coordinates. At most **64** grid positions hold a stored screen; the rest are holes. | The live VRAM camera field |
| **Grid position** | One `(col, row)` on the virtual grid. | A **BG bank**, **sprite bank**, or **nametable slot** |
| **Camera nametable slot** | Live VRAM slots **0–3**. Up to **four** decompressed playfield screens in a 2×2 field for pixel scrolling. | A MAP screen, a BG bank, or plane slots 4–5 |
| **Plane nametable slot** | Live VRAM slots **4–5** only. Optional **parallax** band (raster split). Not part of the 4-screen camera. | Camera slots 0–3 |
| **BG bank** | **256** BG tiles (**16×16** tile grid, 8×8 px each → **4 KB**) in CHR for the current world. Index **0–3**. Each screen names a bank in MAP metadata; loaders copy it into that slot's **BG bank latch**. | A sprite bank, grid position, or nametable slot |
| **Sprite bank** | **256** sprite tiles (**16×16** grid, **4 KB**) in CHR for the current world. Index **0–3**. One **global sprite-bank latch**; game code switches it independently of BG banks. | A BG bank or OAM entry |
| **BG bank latch** | Per **nametable slot** register: which **BG bank** CHR fetch uses for that slot. Six latches (camera 0–3 + plane 4–5). | The sprite-bank latch |
| **2×2 attr quadrant** | Four tiles sharing one palette byte in the 240-byte attr plane. | A BG bank, sprite bank, grid position, or nametable slot |

**Why six nametable slots but “four screens”?** The **camera** uses slots **0–3** only. Slots **4–5** are an optional parallax **plane**, not a fifth/sixth playfield screen in the 2×2 scroll field. See [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) section 8.

**Other uses of “bank”:** **PRG banking** (`$FE80`) and sprite **line-buffer ping-pong halves** (SRAM hardware). NES “CHR bank” comparisons: [18_nes_same_vs_different.md](18_nes_same_vs_different.md).

## Document map

| File | Purpose |
|------|---------|
| [01_system_overview.md](01_system_overview.md) | Vision, principles, onboard memory |
| [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) | Timing, CHR banks, patterns, palettes, ROM budget |
| [03_hardware_variants.md](03_hardware_variants.md) | Retr01-A / C / H form factors |
| [04_worlds_and_screens.md](04_worlds_and_screens.md) | Sparse world atlas, layouts, `load_screen`, MAP directory |
| [05_how_the_machine_works.md](05_how_the_machine_works.md) | Big picture: buses, who R/W whom, interleave, each block |
| [06_hardware_for_software_engineers.md](06_hardware_for_software_engineers.md) | Discrete-logic architecture for software people |
| [07_emulator_specification.md](07_emulator_specification.md) | Low-level C emulator design |
| [08_memory_map.md](08_memory_map.md) | CPU + VRAM map **and** GAL / bus decode |
| [10_hardware_simulators.md](10_hardware_simulators.md) | Gate-level digital simulation options |
| [11_pitch_draft.md](11_pitch_draft.md) | Product pitch (cabinet / developer) |
| [12_part_prices_and_cost.md](12_part_prices_and_cost.md) | Retr01-A motherboard + cart BOM prices |
| [13_pcb_schematic_brief.md](13_pcb_schematic_brief.md) | Older schematic prompt (discrete sprites) — **superseded by 15** |
| [14_reduced_number_of_chips.md](14_reduced_number_of_chips.md) | Sprite/input AVR coprocessor, 8-bit pads, 49-chip v0 BOM |
| [15_schematic_prompt_coprocessor.txt](15_schematic_prompt_coprocessor.txt) | Prompt for a schematic AI later (Retr01-A v0, 49-chip) |
| [16_simulation_and_bringup_plan.md](16_simulation_and_bringup_plan.md) | Step-by-step Digital / simavr / passives / KiCad — **current build path** |
| [17_protoboard_test_plan.md](17_protoboard_test_plan.md) | Physical proto-board islands (power, CPU, VRAM, PPU, 1284, APU) |
| [18_nes_same_vs_different.md](18_nes_same_vs_different.md) | NES compatibility comparison (same mental model vs real differences) |
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | Living decision log (locked + still open) |

## Locked in this revision

1. Family name **Retr01**. Ship path starts at **Retr01-A**.
2. Cart: **8 worlds**, sparse grid up to **16 x 16**, **64 screens max** each. Details: [04_worlds_and_screens.md](04_worlds_and_screens.md).
3. Each world has **4 BG banks + 4 sprite banks** (**256** tiles each, **16×16** tile grid per bank). BG banks are chosen per **nametable slot**; **sprite bank** is a separate global latch. Mid-frame changes via **raster IRQ** (not sprite-0).
4. **Three** 32 KB SRAMs: system RAM at `$0000-$7FFF`, interleaved VRAM, sprite line buffer. OAM is in the 1284. CHR from cart. I/O at `$FExx`.
5. **Per-tile** BG palettes packed in **240 bytes/screen** (1 byte per **2×2 attr quadrant**). **8 palettes**, shared BG color 0, **64-entry** master palette in [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt).
6. Scroll X/Y = one byte each (0-255 wrap) over 1/2/4 live screens. Software streams the next screen at a **2-tile** seam cue. `$FE30` world select is a chapter, not the camera.
7. NES-style APU on a **328P**. Sprites + pads on a **1284P**. CPU **8.000 MHz**. Dot **5.369318 MHz** (341x262, ~60.1 Hz). MAP via `$FE90`. PRG bank via `$FE80` only. Pads: **one byte per player**.
8. Near-term focus: **Digital bring-up** ([16](16_simulation_and_bringup_plan.md)), then proto-board islands ([17](17_protoboard_test_plan.md)), then rewrite the C emulator. Do not generate a PCB from Celus/Protoflow.

# Retr01 Documentation (`markdown_v_01`)

Canonical rewrite of the scattered Gemini drafts under `gemini_docs/`. This folder is the **source of truth** until a later version supersedes it.

**Project name:** Retr01 (formerly GameNerd / Retr02).  
**Initial hardware target:** Retr01-A (arcade board).  
**Near-term software focus:** architecture docs, then **Digital (HNeemann) bring-up** ([16](16_simulation_and_bringup_plan.md)), then rewrite the low-level C emulator. Schematic AIs wait.

## Terminology (canonical)

Use these words consistently across all docs in this folder.

| Term | Meaning | Not the same as |
|------|---------|-----------------|
| **World** | One cart chapter: MAP atlas + **4 BG cells + 4 sprite cells** in CHR. Up to **8** worlds (0–7). `$FE30` world select. | A screen, a nametable slot, or a grid position |
| **Screen** | One stored **32×30** nametable (+ attrs) in MAP-ROM. Has a **(col, row)** on the world's virtual grid. May be **playfield** or **parallax**. | A nametable slot, a BG cell, or a sprite cell |
| **Virtual grid** | Up to **16×16** atlas coordinates. At most **64** grid positions hold a stored screen; the rest are holes. | The live VRAM camera field |
| **Grid position** | One `(col, row)` address on the virtual grid. | A **BG cell**, a **sprite cell**, or a **nametable slot** |
| **Nametable slot** | One live **2 KB** region in VRAM: slots **0–3** (camera) or **4–5** (parallax plane). Holds decompressed tile/attr bytes. | A screen in MAP (source) or a CHR cell (tile art) |
| **BG cell** | **256** BG tile patterns (**4 KB**) in CHR for the current world. Index **0–3**. Chosen per screen in MAP metadata; copied into the destination slot's **BG cell latch** at load time. | A sprite cell, a nametable slot, or a grid position |
| **Sprite cell** | **256** sprite tile patterns (**4 KB**) in CHR for the current world. Index **0–3**. Selected by a **global sprite-cell latch**; game code may switch it at any time. Independent of BG cells and nametable slots. | A BG cell or OAM entry |
| **BG cell latch** | Hardware register remembering which **BG cell** a given nametable slot uses for CHR fetch. Six latches: slots 0–5. | The sprite-cell latch |

| **2×2 attr quadrant** | Four tiles sharing one palette byte in the 240-byte attr plane. | A BG cell, sprite cell, grid position, or nametable slot |

**When “bank” appears in Retr01 docs:** only for **PRG banking** (`$FE80`), the sprite **line-buffer ping-pong** (two 256-byte SRAM regions), or comparisons to NES terminology in [18_nes_same_vs_different.md](18_nes_same_vs_different.md). CHR tile art is **not** called a bank; use **BG cell** / **sprite cell**.

## Document map

| File | Purpose |
|------|---------|
| [01_system_overview.md](01_system_overview.md) | Vision, principles, onboard memory |
| [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) | Timing, CHR cells, patterns, palettes, ROM budget |
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
3. Each world has **4 BG cells + 4 sprite cells**, **256 patterns** each. BG cells are chosen per **nametable slot**; **sprite cell** is a separate global latch. Mid-frame changes via **raster IRQ** (not sprite-0).
4. **Three** 32 KB SRAMs: system RAM at `$0000-$7FFF`, interleaved VRAM, sprite line buffer. OAM is in the 1284. CHR from cart. I/O at `$FExx`.
5. **Per-tile** BG palettes packed in **240 bytes/screen** (1 byte per 2x2 cell). **8 palettes**, shared BG color 0, **64-entry** master palette in [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt).
6. Scroll X/Y = one byte each (0-255 wrap) over 1/2/4 live screens. Software streams the next screen at a **2-tile** seam cue. `$FE30` world select is a chapter, not the camera.
7. NES-style APU on a **328P**. Sprites + pads on a **1284P**. CPU **8.000 MHz**. Dot **5.369318 MHz** (341x262, ~60.1 Hz). MAP via `$FE90`. PRG bank via `$FE80` only. Pads: **one byte per player**.
8. Near-term focus: **Digital bring-up** ([16](16_simulation_and_bringup_plan.md)), then proto-board islands ([17](17_protoboard_test_plan.md)), then rewrite the C emulator. Do not generate a PCB from Celus/Protoflow.

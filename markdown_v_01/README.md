# Retr01 Documentation (`markdown_v_01`)

Canonical rewrite of the scattered Gemini drafts under `gemini_docs/`. This folder is the **source of truth** until a later version supersedes it.

**Project name:** Retr01 (formerly GameNerd / Retr02).  
**Initial hardware target:** Retr01-A (arcade board).  
**Near-term software focus:** hardware-faithful **low-level C emulator** (not asset/game toolchains yet).

## Document map

| File | Purpose |
|------|---------|
| [01_system_overview.md](01_system_overview.md) | Vision, principles, onboard memory |
| [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) | Worlds, screens, banks, patterns, ROM budget |
| [03_hardware_variants.md](03_hardware_variants.md) | Retr01-A / C / H form factors |
| [06_hardware_for_software_engineers.md](06_hardware_for_software_engineers.md) | Discrete-logic architecture for software people |
| [07_emulator_specification.md](07_emulator_specification.md) | Low-level C emulator design |
| [08_memory_map.md](08_memory_map.md) | CPU + VRAM map **and** GAL / bus decode |
| [10_hardware_simulators.md](10_hardware_simulators.md) | Gate-level digital simulation options |
| [11_pitch_draft.md](11_pitch_draft.md) | Marketing-oriented pitch |
| [12_part_prices_and_cost.md](12_part_prices_and_cost.md) | Retr01-A motherboard + cart BOM prices |
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | What is still undecided |

## Locked in this revision

1. Family name **Retr01**. Ship path starts at **Retr01-A**.
2. Cart: **8 worlds**. Each world has a sparse grid up to **64 x 64** and at most **64** stored screens. A screen is a **32x30** nametable **authored** against one BG bank.
3. Bank = **BG page + sprite page** -> **512 patterns**. Runtime BG/sprite banks may differ. Mid-frame changes are OK.
4. **Two** 32 KB SRAMs: full system RAM at `$0000-$7FFF`, interleaved VRAM, CHR from cart, I/O at `$FExx`.
5. **Per-tile** BG palettes packed in **240 bytes/screen** (1 byte per 2x2 cell). **8 palettes**, shared BG color 0, master palette 32-64 TBD.
6. Scroll X/Y = one byte each (0-255 wrap) over 1/2/4 live screens. Software streams the next screen at a **2-tile** seam cue. `$FE30` world select is a chapter, not the camera.
7. NES-style APU. CPU **8.000 MHz**. Dot **5.369318 MHz** (341x262, ~60.1 Hz). MAP via `$FE90`. PRG bank via `$FE80` only.
8. Near-term focus: **low-level C emulator**.

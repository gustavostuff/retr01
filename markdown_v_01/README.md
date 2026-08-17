# Retr01 Documentation - markdown_v_01

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
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | What is still undecided |

## Locked in this revision

1. Family name **Retr01**; ship path starts at **Retr01-A**.
2. Cart: **8 worlds x 64 screens**; screen = **32x30** nametable from **one of 4 banks per world**.
3. Bank = **BG page + sprite page** -> **512 patterns** (256 + 256).
4. **Two** 32 KB SRAMs: CPU-only system RAM + **interleaved VRAM**; **CHR fetched from cartridge**.
5. Per-tile BG palettes; BG and sprite pattern banks may differ; bank changes allowed mid-frame.
6. NES-style APU on ATmega (2 pulse + triangle + noise + DMC).
7. Near-term engineering focus: **low-level C emulator**, not cc65/PPUX productization.

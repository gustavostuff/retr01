# Retr01 Documentation — markdown_v_01

Canonical rewrite of the scattered Gemini drafts under `gemini_docs/`. This folder is the **source of truth** until a later version supersedes it.

**Project name:** Retr01 (formerly GameNerd / Retr02).  
**Initial hardware target:** Retr01-A (arcade board).  
**Near-term software focus:** hardware-faithful **low-level C emulator** (not asset/game toolchains yet).

## Document map

| File | Purpose |
|------|---------|
| [01_system_overview.md](01_system_overview.md) | Vision, principles, variants, onboard memory |
| [02_graphics_and_cartridge.md](02_graphics_and_cartridge.md) | Worlds, screens, banks, patterns, ROM budget |
| [03_retr01_a_arcade.md](03_retr01_a_arcade.md) | Arcade motherboard (current focus) |
| [04_retr01_c_console.md](04_retr01_c_console.md) | Home console variant |
| [05_retr01_h_handheld.md](05_retr01_h_handheld.md) | Handheld / SMD variant |
| [06_hardware_for_software_engineers.md](06_hardware_for_software_engineers.md) | Discrete-logic architecture explained for software people |
| [07_emulator_specification.md](07_emulator_specification.md) | Low-level C emulator goals and components |
| [08_memory_map.md](08_memory_map.md) | **Canonical CPU + VRAM map** (easy to remember) |
| [09_address_decoding.md](09_address_decoding.md) | Virtual GAL / bus routing for the emulator |
| [10_hardware_simulators.md](10_hardware_simulators.md) | Gate-level digital simulation options |
| [11_pitch_draft.md](11_pitch_draft.md) | Marketing-oriented pitch |
| [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md) | What is still undecided |

## Locked in this revision

1. Family name **Retr01**; ship path starts at **Retr01-A**.
2. Cart: **8 worlds × 64 screens**; screen = **32×30** nametable from **one of 4 banks per world**.
3. Bank = **BG page + sprite page** → **512 patterns** (256 + 256).
4. **Two** 32 KB SRAMs: CPU-only system RAM + **interleaved VRAM**; **CHR fetched from cartridge** (no CHR-RAM required).
5. Per-tile BG palettes; BG and sprite pattern banks may differ; bank changes allowed mid-frame.
6. NES-style APU on ATmega (2 pulse + triangle + noise + DMC).
7. Near-term engineering focus: **low-level C emulator**, not cc65/PPUX productization.

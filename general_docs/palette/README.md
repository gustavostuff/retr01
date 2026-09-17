# Palette kit exports

Locked **64-color** motherboard kit used by Studio and Emu preview RGB, and by PROM burn (packed R3G3B2).

## Source of truth

C table: [`apps/common/r01_kit_palette.c`](../../apps/common/r01_kit_palette.c) (`R01_KIT_RGB`, markers `R01_KIT_RGB_BEGIN` / `R01_KIT_RGB_END`).

Header: [`apps/common/r01_kit_palette.h`](../../apps/common/r01_kit_palette.h) (`r01_kit_rgb`).

Both apps link `r01_play_common`, which compiles that file. A second RGB list in Studio or Emu sources must not exist.

## Generated files

| File | Role |
|------|------|
| `global_system_palette.md` | Index + RGB **0..255** + hex preview |
| `retr01_global_system.gpl` | GIMP palette (Palette dialog / import) |
| `retr01_global_system.pal` | JASC-PAL for Aseprite (**Palette -> Load Palette**) |

## Regenerate

From the repo root:

```text
python3 general_docs/palette/gen_kit_palette.py
```

Or from this folder:

```text
python3 gen_kit_palette.py
```

After changing the C table, rebuild the apps and re-run the script so the markdown and tool palettes stay aligned.

Studio **Aseprite entity import** maps PNG pixels by exact kit RGB onto sprite pal **0** (indices **1-3**). Aseprite **Palette -> Load Palette** with `retr01_global_system.pal` keeps source RGB identical to the kit. Nearest-color quantize is not used on that path.

Hardware and cart index rules stay in [`../video-graphics.md`](../video-graphics.md) and [`../memory.md`](../memory.md).

# 09 — Master palette (v0.1)

Canonical RGB master palette for Retr01 World Studio, emulator, and hardware planning.

**Source file:** [`../retr01_palette_v_01.txt`](../retr01_palette_v_01.txt)

## Summary

| Property | Value |
|----------|--------|
| Version | v0.1 (`retr01_palette_v_01`) |
| Entry count | **64** (indices 0–63) |
| Layout | 4 rows × 16 columns in the source file |
| Format | Tab-separated `#RRGGBB` hex (one row per line) |
| Backdrop (BG index 0) | Master index **0** = `#000000` |
| Sprite transparent | Pattern color 0 (not a master index) |

This closes planning item **B1** for World Studio v1: 64 entries with a fixed RGB table. Hardware spec and emulator should load the same file (or a generated `.inc` / header derived from it).

## File format (`retr01_palette_v_01.txt`)

```
#RRGGBB<TAB>#RRGGBB<TAB>...   (16 colors per line)
```

- Lines starting with `#` at column 0 that are **only** color tokens are data rows (the file has no comment lines).
- Parser: split on tabs, strip `#`, parse 6 hex digits → `r`, `g`, `b` (0–255).
- Row 0 → master indices 0–15, row 1 → 16–31, row 2 → 32–47, row 3 → 48–63.

**Loader (`core/palette_io.c` planning):**

```c
int retr01_palette_load_v01(const char *path, retr01_master_palette_t *out);
/* Returns 64 on success; -1 on parse error */
```

New projects: copy parsed RGB into `.r01proj` → `master_palette.entries`. User edits in the Palette panel write back to the project; re-export can diff against v01 for “reset to default”.

## Index map (visual)

Rows are **dark → light** down the file. Columns walk hue across each row.

| Row | Indices | Role (typical) |
|-----|---------|----------------|
| 1 | 0–15 | Shadows, deep BG, backdrop ramp |
| 2 | 16–31 | Mid tones, terrain, wood, foliage |
| 3 | 32–47 | Saturated mids, character fills |
| 4 | 48–63 | Highlights, pastels, UI accents |

## Full table

| Idx | Hex | Idx | Hex | Idx | Hex | Idx | Hex |
|-----|-----|-----|-----|-----|-----|-----|-----|
| 0 | `#000000` | 16 | `#363636` | 32 | `#949494` | 48 | `#FFFFFF` |
| 1 | `#290514` | 17 | `#740A40` | 33 | `#C04A7A` | 49 | `#F1A2BB` |
| 2 | `#2A0507` | 18 | `#77091A` | 34 | `#C54A4D` | 50 | `#F1A6A1` |
| 3 | `#230F06` | 19 | `#693512` | 35 | `#B8601B` | 51 | `#F1A983` |
| 4 | `#1E1306` | 20 | `#5D3F0E` | 36 | `#A27326` | 52 | `#EEAC44` |
| 5 | `#1A1605` | 21 | `#514617` | 37 | `#8F7E2F` | 53 | `#D4BA33` |
| 6 | `#141807` | 22 | `#424C19` | 38 | `#77872D` | 54 | `#B0C841` |
| 7 | `#061A07` | 23 | `#13511A` | 39 | `#209030` | 55 | `#73D275` |
| 8 | `#051A13` | 24 | `#16503F` | 40 | `#2E8E72` | 56 | `#22D0A6` |
| 9 | `#071918` | 25 | `#114E4D` | 41 | `#318B89` | 57 | `#3BCDC9` |
| 10 | `#08181C` | 26 | `#164D58` | 42 | `#1F889C` | 58 | `#48C9E4` |
| 11 | `#071722` | 27 | `#164A66` | 43 | `#2483B5` | 59 | `#88C4ED` |
| 12 | `#030B3D` | 28 | `#163794` | 44 | `#4D77D7` | 60 | `#A4BDEF` |
| 13 | `#16033A` | 29 | `#472990` | 45 | `#7E6AD3` | 61 | `#BBB5F1` |
| 14 | `#20052D` | 30 | `#5F167D` | 46 | `#9D5DBF` | 62 | `#D5A9EF` |
| 15 | `#260420` | 31 | `#6C115F` | 47 | `#B352A0` | 63 | `#F09BDD` |

## Default 8-palette assignment (new `.r01proj`)

World Studio seeds **4 BG + 4 sprite** palettes from the master ramp. Each palette holds 4 **master indices** (2bpp → pattern bits 00, 01, 10, 11).

```json
{
  "backdrop_index": 0,
  "bg_palettes": [
    [0, 1, 2, 3],
    [4, 5, 6, 7],
    [8, 9, 10, 11],
    [12, 13, 14, 15]
  ],
  "sprite_palettes": [
    [0, 32, 33, 34],
    [0, 35, 36, 37],
    [0, 38, 39, 40],
    [0, 41, 42, 43]
  ]
}
```

Rules:

- Every BG palette **must** use master index `0` at slot 0 (shared backdrop `#000000`).
- Sprite palettes use master index `0` at slot 0 for tooling consistency; hardware treats pattern color 0 as **transparent**, not this RGB.
- Artists may remap slots in the Palette panel; fade tables interpolate in **master index** space at build time.

## Integration points

| Component | Behavior |
|-----------|----------|
| **Screen Painter** | Quantize painted pixels to nearest master index (or active palette subset) |
| **CHR pack** | 2bpp indices 0–3 map through active BG/sprite palette → master → RGB preview |
| **`.r01proj`** | Stores full 64-entry RGB copy + 8 palette index tables |
| **Build / fade** | Lerp between master RGB entries when baking fade tables |
| **Emulator** | `master_palette[64]` loaded from project or v01 file on cart load |
| **Hardware** | DAC table TBD; digital RGB values come from this ramp |

## Versioning

- File name suffix `_v_01` bumps when colors change (breaking for old projects).
- `.r01proj` should record `"master_palette_source": "retr01_palette_v_01"` when using defaults.
- Migration: if source tag mismatches, offer “upgrade palette” or keep embedded RGB.

## Related docs

- Project JSON: [03_project_format.md](03_project_format.md)
- Canvas quantize: [06_data_formats.md](06_data_formats.md)
- UI Palette panel: [02_ui.md](02_ui.md)
- Hardware palettes: [`markdown_v_01/02_graphics_and_cartridge.md`](../../markdown_v_01/02_graphics_and_cartridge.md)

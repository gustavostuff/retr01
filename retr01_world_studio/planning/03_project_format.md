# 03 — Project format (`.r01proj`)

The `.r01proj` file is JSON — the **single source of truth** the build pipeline reads. The editor loads and saves it; `core/` validates it; ASM and cart assembly consume it.

## File naming

- Project: `my_game.r01proj`
- Build output: `build/my_game.retr01` (name from project `title` or filename stem)

## Top-level schema

```json
{
  "format_version": 1,
  "title": "My Game",
  "author": "",
  "created": "2026-08-17T00:00:00Z",
  "modified": "2026-08-17T00:00:00Z",

  "master_palette": { ... },
  "world_mode": { ... },
  "worlds": [ ... ],
  "screens": [ ... ],
  "chr": { ... },

  "build": {
    "start_world": 0,
    "start_col": 0,
    "start_row": 0,
    "output_name": "my_game"
  },

  "ui_layout": null
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `format_version` | yes | Integer; bump on breaking schema changes |
| `title` | yes | Display name; default cart output stem |
| `master_palette` | yes | RGB master table + BG/sprite palette assignments |
| `world_mode` | yes | Pack id + axis overrides |
| `worlds` | yes | 1–8 world definitions |
| `screens` | yes | All nametable payloads (logical, may reference CHR by id) |
| `chr` | yes | Pattern data per world/bank |
| `build` | yes | Boot position and output naming |
| `ui_layout` | no | Serialized ImGui dock state |

## `master_palette`

```json
{
  "entries": [
    { "r": 0, "g": 0, "b": 0 },
    { "r": 255, "g": 255, "b": 255 }
  ],
  "bg_palettes": [
    [0, 1, 2, 3],
    [0, 4, 5, 6],
    [0, 7, 8, 9],
    [0, 10, 11, 12]
  ],
  "sprite_palettes": [
    [0, 13, 14, 15],
    [0, 16, 17, 18],
    [0, 19, 20, 21],
    [0, 22, 23, 24]
  ],
  "backdrop_index": 0
}
```

- `entries`: 32–64 RGB triples (B1 final count TBD; v1 uses 32 from default asset)
- Index 0 in every BG palette must equal `backdrop_index` (shared backdrop)
- Sprite index 0 = transparent at draw time

## `world_mode`

```json
{
  "pack_id": "side_platformer",
  "overrides": {
    "movement": "DIR8",
    "transition": "SMOOTH",
    "physics": "PLATFORM",
    "camera": "CAM_H"
  },
  "player": {
    "sprite_tile": 0,
    "sprite_palette": 0,
    "width_px": 8,
    "height_px": 8
  },
  "links": [
    {
      "from": { "world": 0, "col": 0, "row": 0, "edge": "E" },
      "to": { "world": 0, "col": 1, "row": 0 },
      "transition": "SMOOTH"
    }
  ]
}
```

### Axis enum values

| Key | Values |
|-----|--------|
| `movement` | `DIR2`, `DIR4`, `DIR8` |
| `transition` | `SMOOTH`, `INSTANT`, `FADE` |
| `physics` | `PLATFORM`, `TOPDOWN`, `FIXED` |
| `camera` | `CAM_H`, `CAM_V`, `CAM_BOTH` |

Pack linter validates overrides against pack defaults (see [04_world_mode_packs.md](04_world_mode_packs.md)).

### `links` (optional)

Door/warp metadata for INSTANT/FADE transitions. Smooth scrolling uses grid adjacency + seam fill; links add explicit warps (doors, stairs).

## `worlds[]`

```json
{
  "id": 0,
  "label": "Overworld",
  "grid_w": 14,
  "grid_h": 1,
  "empty_template": null,
  "parallax": [
    {
      "col": 0,
      "row": -1,
      "axis": "PARALLAX_H",
      "drive": "PARALLAX_CAMERA",
      "factor": 4,
      "span": 2,
      "span_partner_col": 1,
      "height_scanlines": 80,
      "screen_ids": ["sky_a", "sky_b"]
    }
  ]
}
```

| Field | Description |
|-------|-------------|
| `id` | 0–7 |
| `grid_w`, `grid_h` | Virtual grid dimensions (1–64) |
| `empty_template` | `null` = solid black; or `screen_id` for optional empty nametable |
| `parallax` | Runtime `set_parallax` args + editor screen references |

Parallax cells must have `flags=1` in directory export. Row/col may reference cells outside playfield grid (e.g. sky row above y=0) if within 0–63 MAP coords.

## `screens[]`

Each screen is one stored nametable:

```json
{
  "id": "hub",
  "world": 0,
  "col": 0,
  "row": 0,
  "flags": 0,
  "authored_bank": 0,
  "tiles": [0, 0, 1, 1, ...],
  "attrs": [0, 0, 0, ...],
  "collision": null
}
```

| Field | Size | Description |
|-------|------|-------------|
| `tiles` | 960 bytes (JSON array of 0–255) | Tile indices |
| `attrs` | 240 bytes | Packed attr plane (see [06_data_formats.md](06_data_formats.md)) |
| `flags` | 0 or 1 | 0 = playfield, 1 = parallax |
| `authored_bank` | 0–3 | BG bank used when painting; `load_screen` default |
| `collision` | optional 32×30 bitmask | PRG AABB grid (future / v1 optional) |

Alternative storage (large projects): external `.screen.json` per id with `"external": "screens/hub.screen.json"` — v2 optimization; v1 inline arrays OK.

## `chr`

```json
{
  "worlds": [
    {
      "world_id": 0,
      "banks": [
        {
          "bank_id": 0,
          "bg_tiles": ["base64..."],
          "sprite_tiles": ["base64..."]
        }
      ]
    }
  ]
}
```

Each tile = 16 bytes (2bpp 8×8). Base64 keeps JSON readable; binary `.r01proj` bundle (zip) is v2.

Tile count: 256 BG + 256 sprite per bank × 4 banks × 8 worlds max — project usually sparse.

## Validation rules (on load and pre-build)

1. `worlds.length` ≤ 8
2. Stored screens per world ≤ 64; total ≤ 512
3. Every screen `(col, row)` unique within world; within `grid_w × grid_h` unless parallax exception documented
4. `flags=1` screens not referenced as playfield start
5. Parallax `span=2` requires two adjacent parallax cells with matching metadata
6. CHR: each referenced tile index valid in authored bank
7. `world_mode.pack_id` exists under `runtime/packs/`

## Version migration

`format_version` increment triggers migration helpers in `core/project_io.c`:

- v1 → v2 (example): add collision layer default null, split CHR to external files

Unknown version: refuse load with clear error.

## Generated artifacts (not in `.r01proj`)

Build writes to `build/` (gitignored):

| Path | Producer |
|------|----------|
| `build/intermediate/world0/*.bin` | MAP screen payloads |
| `build/intermediate/map.bin` | Full MAP-ROM image |
| `build/intermediate/chr.bin` | CHR-ROM image |
| `build/generated/world_init.s` | ASM generator |
| `build/generated/world_config.inc` | Pack config struct |
| `build/<name>.retr01` | Final cart |

See [05_cart_assembly.md](05_cart_assembly.md) and [07_build_pipeline.md](07_build_pipeline.md).

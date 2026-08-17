# 06 — Data formats (shared `core/` library)

The `core/` module implements Retr01 authoring formats shared by the editor, build pipeline, and (ideally) `retr01_emu` MAP loader. This doc is the studio-side specification; hardware truth remains in `markdown_v_01/`.

## Screen layout (1200 bytes uncompressed)

Retr01 **does not** use NES 1024-byte nametables.

| Region | Offset | Size | Content |
|--------|--------|------|---------|
| Tile plane | 0 | 960 | 32×30 tile indices (0–255) |
| Attr plane | 960 | 240 | Packed palette IDs (2 bits per tile) |
| **Total** | | **1200** | Per stored screen |

VRAM slot is 2 KB (`NT_SLOT_BYTES = 0x800`): 1200 bytes used + pad — see [08_memory_map.md](../markdown_v_01/08_memory_map.md).

### Attr packing (240 bytes)

One byte = one **2×2 cell** with **four** 2-bit palette fields (one per tile):

```
bits [1:0]   = top-left tile palette (0–3)
bits [3:2]   = top-right
bits [5:4]   = bottom-left
bits [7:6]   = bottom-right
```

Byte index for tile `(tx, ty)`:

```
attr_index = (ty / 2) * 16 + (tx / 2)
shift      = ((ty & 1) * 2 + (tx & 1)) * 2
palette_id = (attrs[attr_index] >> shift) & 3
```

C API (planning):

```c
typedef struct {
    uint8_t tiles[960];
    uint8_t attrs[240];
    uint8_t col, row, flags;
    uint8_t authored_bank;
} retr01_screen_t;

void retr01_attr_set(uint8_t *attrs, int tx, int ty, uint8_t pal);
uint8_t retr01_attr_get(const uint8_t *attrs, int tx, int ty);
```

## MAP screen payload (on disk / in MAP-ROM)

Self-describing blob (optional header + compressed body):

```
[optional] col, row, flags   ; 3 bytes if embedded in payload
RLE tile data                ; decodes to 960 bytes
raw attr plane               ; 240 bytes literal (no RLE v1)
```

Directory `data_off` points at payload start. Directory already has `col`, `row`, `flags` — payload header is redundant but aids debugging when extracted as `.bin`.

## RLE codec (close B9)

**Status:** Open in [OPEN_QUESTIONS.md](../markdown_v_01/OPEN_QUESTIONS.md); studio Phase 0 **implements and documents** this as canonical.

### Recommended format: byte RLE

Control bytes:

| Lead byte | Meaning |
|-----------|---------|
| `0x00` | Run: next byte = `len` (1–255), next byte = `val` → emit `len` copies of `val` |
| `0x01`–`0x7F` | Literal: copy next `(lead)` bytes literally |
| `0x80`–`0xFF` | Reserved / escape (unused v1) |

Encode order for one screen:

1. RLE compress `tiles[960]`
2. Append `attrs[240]` uncompressed

Decoder in emulator MAP loader and studio share `core/rle.c`.

### Round-trip requirements

- Unit tests: random tiles → encode → decode → memcmp
- Worst case expansion handled (literal path if run not profitable)
- Empty screen (all tile 0): should compress well

## MAP-ROM builder structures

```c
typedef struct {
    uint8_t col, row, flags;
    uint32_t data_off;  /* 24-bit stored on wire */
} retr01_dir_entry_t;

typedef struct {
    uint8_t grid_w, grid_h;
    uint8_t screen_count;
    uint32_t empty_off;
    retr01_dir_entry_t directory[64];
} retr01_world_hdr_t;

typedef struct {
    uint8_t magic[4];      /* 'M', 'A', 'P', 0x01 */
    uint8_t version;
    uint8_t world_count;
    uint32_t world_base[8];  /* 24-bit each, stored in uint32 */
} retr01_map_hdr_t;
```

Builder assigns `data_off` sequentially after directory; updates `world_base[N]` in cart MAP header.

## CHR tile (16 bytes)

Standard 2bpp 8×8 layout (planar bitplanes):

```
bytes 0–7:   bitplane 0
bytes 8–15:  bitplane 1
```

Per bank: 256 BG tiles + 256 sprite tiles = 8192 bytes/bank.

`pack/chr_io.c`: read/write tile, import from 16×16 PNG sheet.

## Canvas → CHR (`pack/` module)

Adapt PPUX sketch algorithm:

1. Input: 256×240 RGBA canvas + master palette
2. Quantize pixels to 2bpp indices (3 colors + transparent for sprites)
3. Slice into 8×8 tiles
4. Dedupe identical tiles (hash 16 bytes)
5. Assign indices 0–255; error if > 256 unique BG tiles
6. Generate attr plane from per-pixel palette choices

**Tolerance** (optional): merge near-identical tiles like PPUX — config in project settings.

## Palette conversion

Master palette → hardware format TBD (B1). v1:

- Store RGB in `.r01proj`
- Export phase converts to emulator PPU palette entries via shared table from `imgs/retr01_palette_bank_0.png`

## `.r01proj` I/O

`core/project_io.c`:

- `retr01_project_load(path, retr01_project_t *out)`
- `retr01_project_save(path, const retr01_project_t *in)`
- `retr01_project_validate(const retr01_project_t *p, retr01_error_list_t *err)`

JSON via `cJSON` or similar; validate on load.

## Collision grid (optional v1)

Parallel to tiles: 32×30 bytes bitmask or 2 bits/cell — exported to PRG for AABB in platform packs. Not in MAP-ROM v1; lives in generated ASM or PRG data section.

## `.retr01` cart header (first bytes)

```c
#define RETR01_MAGIC "RETR01"   /* 6 bytes: 0x52 0x45 0x54 0x52 0x30 0x31 */

typedef struct {
    uint8_t magic[6];      /* "RETR01" */
    uint16_t format_version;
    uint8_t flags;
    uint8_t world_count;
    uint32_t prg_size;
    uint32_t chr_size;
    uint32_t map_size;
    uint32_t prg_offset;   /* TBD with linker */
    uint8_t reserved[22];
} retr01_cart_hdr_t;       /* 0x30 bytes total before PRG data */
```

Full layout: [05_cart_assembly.md](05_cart_assembly.md).

## File extension summary

| Extension | Format |
|-----------|--------|
| `.r01proj` | JSON project |
| `.bin` | MAP screen payload (intermediate) |
| `.retr01` | Full cart container |

## Tests (Phase 0)

| Test | Module |
|------|--------|
| attr round-trip | `core/screen.c` |
| RLE round-trip | `core/rle.c` |
| MAP build 1 world 3 screens | `core/map_builder.c` |
| CHR dedupe cap | `pack/chr_pack.c` |
| project JSON load/save | `core/project_io.c` |

## Related docs

- MAP directory semantics: [04_worlds_and_screens.md](../markdown_v_01/04_worlds_and_screens.md)
- Cart container: [05_cart_assembly.md](05_cart_assembly.md)
- Project schema: [03_project_format.md](03_project_format.md)

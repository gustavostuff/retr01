# Retr01 Low-Level Emulator Specification

## 1. Objective

A **hardware-restricted emulator in C** whose job is architectural validation, not max host FPS. Behavior that passes here should match Retr01-A silicon for: memory decode, interleaved VRAM phases, sprite drop rules, bank timing, and NES-style APU register traffic.

**In scope now:** emulator core + host display/audio glue (e.g. SDL2).  
**Out of scope now:** PPUX, cc65, and other asset/game authoring toolchains.

Canonical map + bus decode: [08_memory_map.md](08_memory_map.md).

---

## 2. Suggested module layout

Keep hardware boundaries as C modules so the "virtual GAL" stays obvious:

```text
retr01_emu/
  cpu/          /* W65C02S core: regs, step one cycle / one instruction */
  bus/          /* system_bus_read / write, only entry to memory */
  mem/          /* system_ram[], vram[], io_page state */
  cart/         /* prg[], chr[], map[] + mapper regs */
  ppu/          /* beam, nametable fetch, OAM eval, framebuffer */
  apu/          /* NES-style channels -> host PCM */
  host/         /* SDL window, audio callback, input -> $FE6x */
  main.c        /* load ROM, reset, run loop */
```

Rule of thumb: **the CPU never indexes `system_ram` or `vram` directly.** Every load/store goes through `bus`.

---

## 3. Core data: arrays that *are* the chips

Hardware chips map cleanly to fixed buffers. There is no `malloc` in the hot path.

```c
uint8_t system_ram[0x8000];   /* $0000-$7FFF full 32 KB */
uint8_t vram[0x8000];         /* 32 KB video SRAM */
uint8_t io_regs[0x100];       /* $FE00-$FEFF shadow / decode aids */

/* Cartridge (sizes = planning ceilings. Load what the image provides) */
uint8_t *prg;   size_t prg_size;   /* <= 512 KB */
uint8_t *chr;   size_t chr_size;   /* <= 256 KB */
uint8_t *map;   size_t map_size;   /* compressed screens */
```

| Hardware | Emulator representation |
|----------|-------------------------|
| AS6C62256 system | `system_ram[]` |
| AS6C62256 VRAM | `vram[]` |
| GAL decode | `if`/`switch` in `bus` (not a separate array) |
| Cart flash regions | `prg[]` / `chr[]` / `map[]` |
| Latches (scroll, banks) | fields in a `PpuState` / `MapperState` struct, updated when `io_regs` written |
| 74HC161 beam counters | `uint16_t dot`, `uint16_t scanline` (or x/y) in `PpuState` |

Optional: keep `io_regs[256]` as the raw MMIO image **and** mirror important bits into typed structs after each write (easier PPU code, still inspectable from a debugger).

---

## 4. CPU core

### 4.1 Registers (not a "stack chip")

```c
typedef struct {
    uint16_t pc;
    uint8_t  a, x, y;
    uint8_t  sp;      /* stack pointer, indexes page $0100 in system_ram */
    uint8_t  p;       /* status flags */
    /* cycle / phase bookkeeping */
    uint64_t cycles;
    int      phase;   /* 0 = PPU owns VRAM, 1 = CPU owns VRAM (name to match schematic) */
} Cpu;
```

### 4.2 Hardware stack -> byte array

The 6502 stack is **not** a C `stack<>`. It is bytes in system RAM:

```text
Push -> system_ram[0x0100 + sp], then sp--
Pull -> sp++, then read system_ram[0x0100 + sp]
```

Implement `push`/`pull` as bus writes/reads to `$0100|$sp` so stack traffic still goes through decode (and so a future watchpoint on RAM works).

### 4.3 Stepping

Prefer **cycle-accurate** stepping (or instruction step that advances N cycles and runs PPU/APU for each):

```c
void emu_tick(Emu *e) {
    /* One half-cycle or one full CPU cycle. Pick a convention and stick to it */
    e->cpu.phase = /* derive from e->cpu.cycles */;
    ppu_dot(e);          /* may read vram[] / chr[] on PPU phase */
    cpu_cycle(e);        /* may bus_read/write on CPU phase */
    apu_tick(e);
    e->cpu.cycles++;
}
```

NMI: when PPU enters VBlank (scanline 240), set the NMI pin. Raster: when `scanline == raster_y` at dot 0, set `raster_hit` and optionally IRQ. CPU samples both pins like silicon. Handlers are guest code.

---

## 5. Bus / virtual GAL

```c
uint8_t bus_read(Emu *e, uint16_t addr) {
    if (addr < 0x8000)
        return e->system_ram[addr];
    if (addr >= 0xFE00 && addr <= 0xFEFF)
        return io_read(e, (uint8_t)addr);
    return cart_prg_read(e, addr); /* $8000-$FDFF and $FF00-$FFFF */
}

void bus_write(Emu *e, uint16_t addr, uint8_t data) {
    if (addr < 0x8000) {
        e->system_ram[addr] = data;
        return;
    }
    if (addr >= 0xFE00 && addr <= 0xFEFF) {
        io_write(e, (uint8_t)addr, data);
        return;
    }
    /* PRG window: ignore. Banking only via $FE80 */
}
```

`io_write` dispatches on `(addr & 0xFF) >> 4` (the 16-byte blocks in the memory map).

### VRAM port (interleave)

```c
void vram_data_write(Emu *e, uint8_t data) {
    if (!cpu_owns_vram(e)) {
        /* debug builds */
        abort_or_log("VRAM write on PPU phase");
        return;
    }
    e->vram[e->ppu.vram_addr & 0x7FFF] = data;
    e->ppu.vram_addr += e->ppu.vram_increment; /* 1 or 32, etc. */
}
```

Same gate on reads. PPU fetch paths call `vram_ppu_read(e, addr)` only when `!cpu_owns_vram(e)` (or the opposite polarity, match the board).

---

## 6. PPU implementation sketch

### 6.1 State

```c
typedef struct {
    uint16_t vram_addr;
    uint8_t  vram_addr_hi_next;  /* two-write latch */
    uint8_t  scroll_x, scroll_y; /* 0-255 wrap, fine scroll over 1/2/4 NT field */
    uint8_t  nt_arrange;         /* mirroring / which slots are distinct */
    uint8_t  bg_bank;            /* 0-3 within world */
    uint8_t  spr_bank;
    uint8_t  world;
    uint8_t  raster_y;           /* 0-255, compare at start of scanline */
    uint8_t  raster_hit;
    uint8_t  raster_irq_enable;

    int      scanline;           /* -1 pre-render ... 239 visible ... VBlank */
    int      dot;                /* 0 ... dots_per_line-1 */

    uint8_t  oam[256];           /* 64 sprites x 4 bytes (NES-like), NOT in vram[] */
    uint8_t  oam_addr;

    /* Per-scanline sprite pipeline: 16 sprites x 4 bytes */
    uint8_t  secondary_oam[64];
    int      sprites_on_line;

    uint32_t framebuffer[256 * 240]; /* host RGB for SDL */
} Ppu;
```

### 6.2 Nametable as arrays inside `vram[]`

Do not allocate four separate C arrays unless you want aliases:

```c
/* Slot s: tiles at base, attrs immediately after (planning layout) */
enum { NT_SLOT_BYTES = 0x800 }; /* 2 KB: 960 tiles + 240 packed attrs + pad */

static uint8_t *nt_tiles(Emu *e, int slot) {
    return &e->vram[slot * NT_SLOT_BYTES];
}
static uint8_t *nt_attrs(Emu *e, int slot) {
    /* +0x3C0 = after 960 tile bytes, 240 packed attr bytes (2x2 cell, 2 bits/tile) */
    return &e->vram[slot * NT_SLOT_BYTES + 0x3C0];
}
```

Scrolling: `scroll_x`/`scroll_y` wrap 0-255. Combine with `nt_arrange` to sample the correct slot(s) so the viewport shows portions of **1, 2, or 4** screens. Index `nt_tiles[ty * 32 + tx]`. BG palette from packed attrs: byte `nt_attrs[(ty / 2) * 16 + (tx / 2)]`, then `((byte >> (((ty & 1) * 2 + (tx & 1)) * 2)) & 3)`.

### 6.3 CHR fetch (cart, not VRAM)

```c
uint8_t chr_read_bg(Emu *e, uint8_t tile, uint8_t row /*0-7*/, int bitplane) {
    size_t bank = e->ppu.world * 4 + e->ppu.bg_bank;
    size_t page = 0; /* BG page first in bank */
    size_t off = bank * 0x2000 + page * 0x1000 + tile * 16 + row + bitplane * 8;
    return e->chr[off % e->chr_size]; /* or hard fault if OOB */
}
```

Sprites use `spr_bank` and page `1`. Mid-frame bank writes just mutate `ppu.bg_bank` / `spr_bank`. The next fetch sees the new value (accurate and simple). Raster IRQ: when `scanline` becomes `raster_y` at dot 0, set `raster_hit`. If `raster_irq_enable`, assert CPU IRQ until guest acks. Do **not** emulate NES sprite-0 hit.

### 2bpp -> color index

For each pixel, combine two bitplanes into `0..3`. Same NES rule:
- **Sprites:** index `0` = transparent (do not draw, show BG).
- **BG:** index `0` = shared **backdrop** color (opaque). Indices 1-3 come from the BG palette selected by **that tile's** 2-bit field in the packed attr byte.  
Map through palette regs + master palette (`uint32_t master_palette[64]` — see [`retr01_world_studio/retr01_palette_v_01.txt`](../retr01_world_studio/retr01_palette_v_01.txt)) into `framebuffer[y * 256 + x]`.

PPU timing: advance `dot`/`scanline` on the **5.369318 MHz** domain (341x262). Run CPU ticks on the **8 MHz** domain. Host presents one framebuffer per VBlank (~60.1 Hz).

### 6.4 Sprite evaluation (scan -> secondary buffer)

During HBlank (or the dots reserved for eval):

1. Clear `secondary_oam` / `sprites_on_line = 0`.
2. Walk primary `oam[0..255]` in steps of 4 (Y at `oam[i]`).
3. If sprite Y hits this scanline and `sprites_on_line < 16`, copy the 4 bytes into secondary storage and increment.
4. If more would qualify, **drop** them (do not draw). That matches hardware accuracy.

During the visible line, for each x, scan the <=16 active sprites and pick the front-most non-transparent pixel, honoring the OAM **priority** bit (behind opaque BG). A small `uint8_t line_spr_idx[256]` / color buffer is a fine software stand-in for hardware shift registers.

### 6.5 Framebuffer

`uint32_t framebuffer[256*240]` (or `uint8_t[256*240]` color indices + palette expand in the host). Present once per frame to SDL. No need to emulate DAC analog levels.

---

## 7. OAM DMA

When guest writes the DMA trigger in `$FE2x`:

```c
uint16_t src = (uint16_t)page << 8; /* page in system RAM */
for (int i = 0; i < 256; i++)
    e->ppu.oam[i] = bus_read(e, src + i);
/* Advance CPU cycles by the real DMA cost so timing stays honest */
```

DMA reads system RAM (always legal). It must still burn cycles so games cannot pretend DMA is free.

---

## 8. Cartridge & mapper

```c
typedef struct {
    uint8_t prg_bank;     /* which slice at $8000-$FDFF / $FF00-$FFFF, set only via $FE80 */
} Mapper;

uint8_t cart_prg_read(Emu *e, uint16_t addr) {
    /* Map CPU addr into banked PRG. Skip the $FExx hole (never called for I/O) */
    size_t off = (size_t)e->mapper.prg_bank * 0x8000 + (addr & 0x7FFF);
    return e->prg[off % e->prg_size];
}
```

MAP decompression: guest (or host helper during bring-up) reads bytes through **`$FE90` MAP port** and writes nametables through **`$FE1x`**. Do not bank MAP over system RAM.

---

## 9. APU

Model channels as structs + a ring buffer of PCM for SDL:

```c
typedef struct {
    /* pulse x2, triangle, noise, dmc: timers, volume, length counters, ... */
    Pulse  pulse1, pulse2;
    Tri    triangle;
    Noise  noise;
    Dmc    dmc;
    float  sample_accum;
    int16_t pcm[APU_RING];
    size_t  pcm_w, pcm_r;
} Apu;
```

- `io_write` to `$FE40-$FE5F` updates channel regs (NES-like layout recommended).
- Each `emu_tick` (or every N ticks) runs channel timers and pushes a mixed sample into `pcm[]`.
- SDL audio callback only **pops** from the ring. No PPU work on the audio thread.

---

## 10. Input & board I/O

Host keyboard/joystick maps into **four bytes** `$FE60-$FE63` (P1 stick+btns, P1 extra, P2, P2 extra). Retr01-A: latch once per frame from parallel bits. Retr01-C later: clock a 3-wire pad into the same regs. EEPROM (`$FE7x`) can be a small `uint8_t eeprom[size]` file-backed array.

---

## 11. Main loop (host)

```c
load_cart("game.retr01");
cpu_reset(&emu);          /* PC from $FFFC vector via bus_read */

while (running) {
    while (!frame_complete)
        emu_tick(&emu);

    sdl_present(emu.ppu.framebuffer);
    sdl_poll_input_into_io(&emu);
    frame_complete = false;
}
```

For debugging: run a fixed number of ticks, or break when `scanline == Y && dot == X`.

---

## 12. Bring-up order (practical)

1. Bus + `system_ram` + PRG window + CPU smoke test (NOP loop in hand-assembled ROM).
2. I/O page writes that only set struct fields (scroll, banks).
3. VRAM port + phase checks (write nametable, read back).
4. BG renderer -> framebuffer (no sprites).
5. OAM + 16-sprite cap.
6. NMI metronome + simple guest "wait for frame" loop.
7. APU pulse tone.
8. Mapper / multi-bank CHR / four NT scroll.
9. Raster Y compare + IRQ, mid-frame bank/scroll split.

---

## 13. Non-goals

- Analog DAC / encoder simulation.
- Gate-level GAL fuse simulation ([10_hardware_simulators.md](10_hardware_simulators.md)).
- Full game/asset toolchain integration in this phase.
- Dynamic allocation in the tick path.

---

## 14. Quick reference: hardware to C

| Silicon idea | C idea |
|--------------|--------|
| SRAM chip | `uint8_t buf[size]` |
| 6502 stack | bytes at `$0100+sp` in `system_ram` |
| Latch | `uint8_t` / `uint16_t` field updated on MMIO write |
| Raster IRQ | `raster_y` vs `scanline` at dot 0, optional IRQ pin |
| Counter (beam) | `int scanline`, `int dot` |
| GAL | `bus_read` / `bus_write` branching |
| Mux / phase | `cpu.phase` gate around VRAM |
| OAM | `uint8_t oam[256]` |
| Line sprite limit | secondary array capped at 16 |
| CHR-ROM | `chr[]` indexed by world/bank/tile/row |
| Frame | `framebuffer[256*240]` |
| APU | channel structs + PCM ring buffer |
| Cart mapper | bank index + modular offset into `prg[]` |

# Retr01 KiCad labs (32-IC)

Step-by-step schematic labs for the **32-IC** motherboard. Written for a software engineer who is new to board design. Each lab adds a small island, names the nets, and ends with a clear "pass" check.

You do **not** need to read the old monolithic guide first.

**KiCad:** 8.x recommended.  
**BOM / architecture:** [`../hardware.md`](../hardware.md).  
**Sim islands (visual netlist):** [`../../retr01/sim/README.md`](../../retr01/sim/README.md).  
**Per-chip pin notes:** [`../../hw/md/`](../../hw/md/).

## How to use these labs

1. Finish one lab before starting the next.
2. Keep the same KiCad project the whole way.
3. Use the net names the lab gives you. Searchability matters later.
4. When a lab says **temporary**, that wiring will be replaced in a later lab. That is normal.
5. Silicon target vs what Sim/Emu do today: see [`../hardware.md`](../hardware.md#runners-today-vs-silicon-target).
6. Do not advance until the lab **Success metrics** section passes. KiCad checks are required. Hardware / Sim checks are optional until you have silicon or use Sim.

## Success metrics pattern (every lab)

Each lab ends with:

| Block | Meaning |
|-------|---------|
| **In KiCad (required)** | Highlight nets, ERC, what you must see on the sheet |
| **Optional later** | Breadboard, programmed PLD, or Sim island smoke |

## Lab index

| Lab | Goal | Status |
|-----|------|--------|
| [01 CPU and system RAM](01_cpu_and_system_ram.md) | 6502 can read/write 32 KB RAM | Ready |
| [02 Cart PRG read](02_cart_prg_read.md) | 6502 can fetch bytes from cart flash | Ready |
| [03 Decode PLD and CPU bus transceiver](03_decode_pld_and_bus.md) | Replace temporary chip-selects with ATF22V10 + HC245 | Ready |
| [04 `$FExx` latches](04_fexx_latches.md) | CPU writes stick in HC573s for later video/IO | Ready |
| 05+ | VRAM interleave, beam, video, 1284, APU, ports | Later |

## Mental model (software analogy)

| Hardware idea | Rough software analogy |
|---------------|------------------------|
| Net name (`CPU_D0`) | Global variable shared by modules |
| Chip-select (`RAM_CE#`) | `if (addr in range) enable device` |
| Active-low `#` | Signal is "true" when the wire is **0** |
| Hierarchical sheet | Source file in a multi-file project |
| ERC | Compiler warnings for disconnected power / floating inputs |

## Project layout to create once

```text
hw/kicad/retr01_mobo/
  retr01_mobo.kicad_pro
  retr01_mobo.kicad_sch          (root)
  sheets/
    01_power_cpu_ram.kicad_sch
    02_cart.kicad_sch
    ...
  symbols/                       (optional Retr01 library)
```

Lab 01 walks you through creating the project and the first sheet.

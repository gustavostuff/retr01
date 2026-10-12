#!/usr/bin/env python3
"""Tier A burn images.

PROM bytes are the locked kit, packed R3G3B2 the same way as the sim.
Beam PLDs are a 341 by 262 raster that fits in two ATF22V10s. The sim
shell also drives six index pins from an internal counter. Those pins
do not fit beside a 9-bit counter, so this burn is sync only.
"""

from __future__ import annotations

import re
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
PALETTE_C = REPO / "apps" / "common" / "r01_kit_palette.c"
BEGIN = "R01_KIT_RGB_BEGIN"
END = "R01_KIT_RGB_END"
HEX_TRIPLE = re.compile(
    r"\{\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*,\s*0x([0-9A-Fa-f]{2})\s*\}"
)

# Sum-of-products room on a GAL22V10, pin 14 through pin 23.
# The enable row is already subtracted.
PT_LIMIT = {
    14: 8,
    15: 10,
    16: 12,
    17: 14,
    18: 16,
    19: 16,
    20: 14,
    21: 12,
    22: 10,
    23: 8,
}

DOTS_X = 341
DOTS_Y = 262
HSYNC_LO = 264
HSYNC_HI = 289  # exclusive, matches the Tier A sim
VSYNC_LO = 243
VSYNC_HI = 246


def parse_kit(text: str) -> list[tuple[int, int, int]]:
    begin = text.find(BEGIN)
    end = text.find(END)
    if begin < 0 or end < 0 or end <= begin:
        raise SystemExit("kit markers missing")
    colors = [
        (int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16))
        for m in HEX_TRIPLE.finditer(text[begin:end])
    ]
    if len(colors) != 64:
        raise SystemExit(f"expected 64 kit colors, found {len(colors)}")
    return colors


def pack_r3g3b2(r: int, g: int, b: int) -> int:
    rr = (r * 7 + 127) // 255
    gg = (g * 7 + 127) // 255
    bb = (b * 3 + 127) // 255
    return (rr << 5) | (gg << 2) | bb


def write_prom(colors: list[tuple[int, int, int]]) -> None:
    data = bytes(pack_r3g3b2(r, g, b) for r, g, b in colors)
    (HERE / "at27c256r_kit.bin").write_bytes(data)


def primes(onset: set[int], dc: set[int], nvars: int) -> list[tuple[int, int]]:
    """Quine-McCluskey. Each prime is (value, mask). Mask bit 1 is a dash."""
    universe = onset | dc
    current: dict[tuple[int, int], set[int]] = {}
    for m in universe:
        current[(m, 0)] = {m}
    primes_out: list[tuple[tuple[int, int], set[int]]] = []
    while current:
        nxt: dict[tuple[int, int], set[int]] = {}
        keys = list(current)
        combined: set[tuple[int, int]] = set()
        for i, a in enumerate(keys):
            va, ma = a
            for b in keys[i + 1 :]:
                vb, mb = b
                if ma != mb:
                    continue
                diff = (va ^ vb) & ((1 << nvars) - 1)
                if diff == 0 or diff & ma or diff & (diff - 1):
                    continue
                mask = ma | diff
                value = va & ~mask
                key = (value, mask)
                bucket = nxt.setdefault(key, set())
                bucket.update(current[a])
                bucket.update(current[b])
                combined.add(a)
                combined.add(b)
        for key in keys:
            if key not in combined:
                covered = current[key] & onset
                if covered:
                    primes_out.append((key, covered))
        current = nxt
    uncovered = set(onset)
    chosen: list[tuple[int, int]] = []
    pool = primes_out
    while uncovered:
        best_i = -1
        best_n = 0
        for i, (_imp, cov) in enumerate(pool):
            n = len(cov & uncovered)
            if n > best_n:
                best_n = n
                best_i = i
        if best_i < 0:
            raise SystemExit("minimizer left onset bits uncovered")
        imp, cov = pool.pop(best_i)
        chosen.append(imp)
        uncovered -= cov
    return chosen


def onset_for_bit(mod: int, bit: int, nvars: int = 9) -> tuple[set[int], set[int]]:
    onset: set[int] = set()
    dc = set(range(mod, 1 << nvars))
    for cur in range(mod):
        nxt = 0 if cur == mod - 1 else cur + 1
        if nxt & (1 << bit):
            onset.add(cur)
    return onset, dc


def onset_for_pulse(mod: int, lo: int, hi: int, nvars: int = 9) -> tuple[set[int], set[int]]:
    """Registered pulse, high while the count is in [lo, hi)."""
    onset: set[int] = set()
    dc = set(range(mod, 1 << nvars))
    for cur in range(mod):
        nxt = 0 if cur == mod - 1 else cur + 1
        if lo <= nxt < hi:
            onset.add(cur)
    return onset, dc


def term(value: int, mask: int, names: list[str]) -> str:
    parts: list[str] = []
    for i, name in enumerate(names):
        if mask & (1 << i):
            continue
        parts.append(name if value & (1 << i) else "/" + name)
    if not parts:
        raise SystemExit("constant-true term is not used")
    return " * ".join(parts)


def sop(implicants: list[tuple[int, int]], names: list[str]) -> str:
    if not implicants:
        return "GND"
    return "\n     + ".join(term(v, m, names) for v, m in implicants)


def eval_sop(implicants: list[tuple[int, int]], state: int) -> int:
    if not implicants:
        return 0
    for value, mask in implicants:
        if (state & ~mask) == (value & ~mask):
            return 1
    return 0


def check_counter(mod: int, bits: list[list[tuple[int, int]]], pulse: list[tuple[int, int]], lo: int, hi: int) -> None:
    for cur in range(mod):
        nxt = 0
        for bit, imps in enumerate(bits):
            if eval_sop(imps, cur):
                nxt |= 1 << bit
        expect = 0 if cur == mod - 1 else cur + 1
        if nxt != expect:
            raise SystemExit(f"count {cur} went to {nxt}, expected {expect}")
        got = eval_sop(pulse, cur)
        want = 1 if lo <= expect < hi else 0
        if got != want:
            raise SystemExit(f"pulse at {cur} (next {expect}) is {got}, expected {want}")


def assign_pins(functions: list[tuple[str, list[tuple[int, int]]]]) -> dict[str, int]:
    """Widest equations land on the pins with the most product terms."""
    order = sorted(functions, key=lambda item: len(item[1]), reverse=True)
    pins = sorted(PT_LIMIT, key=lambda pin: PT_LIMIT[pin], reverse=True)
    if len(order) != 10:
        raise SystemExit(f"expected 10 macrocells, got {len(order)}")
    placed: dict[str, int] = {}
    for (name, imps), pin in zip(order, pins):
        if len(imps) > PT_LIMIT[pin]:
            raise SystemExit(f"{name} needs {len(imps)} terms, pin {pin} holds {PT_LIMIT[pin]}")
        placed[name] = pin
    return placed


def write_pld(path: Path, signature: str, placed: dict[str, int], equations: dict[str, str], blurb: str) -> None:
    names = ["NC"] * 24
    names[0] = "CLK"
    names[1] = "RES"
    names[11] = "GND"
    names[23] = "VCC"
    for name, pin in placed.items():
        names[pin - 1] = name
    lines = [
        "GAL22V10",
        signature,
        " ".join(names[:12]),
        " ".join(names[12:]),
        "",
        "AR = /RES",
        "",
    ]
    for name in sorted(placed, key=lambda n: placed[n]):
        lines.append(f"{name}.R = {equations[name]}")
        lines.append("")
    lines.append("DESCRIPTION")
    lines.append(blurb.rstrip())
    lines.append("")
    path.write_text("\n".join(lines), encoding="utf-8")


def build_beam(mod: int, pulse_name: str, lo: int, hi: int) -> tuple[dict[str, int], dict[str, str], list[list[tuple[int, int]]], list[tuple[int, int]]]:
    names = [f"Q{i}" for i in range(9)]
    bit_imps: list[list[tuple[int, int]]] = []
    equations: dict[str, str] = {}
    functions: list[tuple[str, list[tuple[int, int]]]] = []
    for bit in range(9):
        onset, dc = onset_for_bit(mod, bit)
        imps = primes(onset, dc, 9)
        bit_imps.append(imps)
        equations[names[bit]] = sop(imps, names)
        functions.append((names[bit], imps))
    p_onset, p_dc = onset_for_pulse(mod, lo, hi)
    pulse = primes(p_onset, p_dc, 9)
    equations[pulse_name] = sop(pulse, names)
    functions.append((pulse_name, pulse))
    check_counter(mod, bit_imps, pulse, lo, hi)
    placed = assign_pins(functions)
    return placed, equations, bit_imps, pulse


def main() -> None:
    colors = parse_kit(PALETTE_C.read_text(encoding="utf-8"))
    write_prom(colors)
    x_placed, x_eq, _, _ = build_beam(DOTS_X, "HSYNC", HSYNC_LO, HSYNC_HI)
    y_placed, y_eq, _, _ = build_beam(DOTS_Y, "VSYNC", VSYNC_LO, VSYNC_HI)
    write_pld(
        HERE / "beam_x.pld",
        "BeamX",
        x_placed,
        x_eq,
        "Beam X. DOT on CLK, rising edge. Count 0..340 then 0. "
        "HSYNC high on counts 264..288. RES low clears. "
        "HSYNC also clocks Beam Y.",
    )
    write_pld(
        HERE / "beam_y.pld",
        "BeamY",
        y_placed,
        y_eq,
        "Beam Y. CLK is Beam X HSYNC, rising edge, once per line. "
        "Count 0..261 then 0. VSYNC high on counts 243..245. RES low clears.",
    )
    print("prom", (HERE / "at27c256r_kit.bin").stat().st_size, "bytes")
    print("beam x pins", {n: x_placed[n] for n in sorted(x_placed, key=lambda n: x_placed[n])})
    print("beam y pins", {n: y_placed[n] for n in sorted(y_placed, key=lambda n: y_placed[n])})


if __name__ == "__main__":
    main()

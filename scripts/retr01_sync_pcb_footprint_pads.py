#!/usr/bin/env python3
"""Push Retr01_Lib footprint geometry from .pretty into embedded PCB copies.

KiCad caches libraries in memory; \"Update footprints from library\" often applies a
stale in-RAM copy (or old silk while pads were fixed by script). Use this with KiCad
**closed**, or reload the PCB from disk after running.

Usage:
  python3 scripts/retr01_sync_pcb_footprint_pads.py PATH/to/v_01.kicad_pcb
  python3 scripts/retr01_sync_pcb_footprint_pads.py PATH/to/v_01.kicad_pcb --full
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DEFAULT_LIB = REPO / "apps/sim/tier-h/skidl/library/Retr01_Lib.pretty"

FP_HEAD = re.compile(r'^\t+\(footprint "Retr01_Lib:([^"]+)"')
PAD_AT = re.compile(
    r'(\(pad "(\d+)" thru_hole oval\s*\n\s+\(at )([-\d.]+)\s+([-\d.]+)(?:\s+([-\d.]+))?\)',
    re.MULTILINE,
)
DESCR_LINE = re.compile(r'(\t+\(descr ")([^"]*)("\))')
TAGS_LINE = re.compile(r'(\t+\(tags ")([^"]*)("\))')
PAD_NET = re.compile(r'\(net "([^"]*)"\)')
REGION_MARK = "(duplicate_pad_numbers_are_jumpers no)"
REGION_END = "(embedded_fonts no)"
FP_PLACEMENT = re.compile(
    r'\(footprint "Retr01_Lib:[^"]+"\s*\n'
    r'(?:\t+\(layer[^\n]*\n)?'
    r'(?:\t+\(uuid[^\n]*\n)?'
    r'\t+\(at [-\d.]+ [-\d.]+ ([-\d.]+)\)',
    re.MULTILINE,
)


def footprint_placement_rotation(block: str) -> float:
    """Footprint (at X Y R) on the board — KiCad copies R onto each oval pad."""
    m = FP_PLACEMENT.search(block)
    if not m:
        return 0.0
    return float(m.group(1))


def stamp_oval_pad_rotation(text: str, rot: float) -> str:
    """Match Pcbnew after *Update from library* on a rotated footprint."""

    def repl(m: re.Match[str]) -> str:
        x, y = m.group(3), m.group(4)
        if abs(rot) < 1e-9:
            return f"{m.group(1)}{x} {y})"
        return f"{m.group(1)}{x} {y} {rot:g})"

    return PAD_AT.sub(repl, text)


def parse_pad_xy(mod_text: str) -> dict[str, tuple[float, float]]:
    out: dict[str, tuple[float, float]] = {}
    for m in PAD_AT.finditer(mod_text):
        out[m.group(2)] = (float(m.group(3)), float(m.group(4)))
    return out


def load_descr(mod_text: str) -> str | None:
    m = DESCR_LINE.search(mod_text)
    return m.group(2) if m else None


def pad_nets(block: str) -> dict[str, str | None]:
    """Pad number -> net name (None if no net line)."""
    out: dict[str, str | None] = {}
    for m in re.finditer(
        r'\(pad "(\d+)" thru_hole oval.*?(?=\n\t\t\(pad |\n\t\t\(embedded_fonts|\n\t\t\(model |\Z)',
        block,
        re.S,
    ):
        num = m.group(1)
        nm = PAD_NET.search(m.group(0))
        out[num] = nm.group(1) if nm else None
    return out


def inject_pad_nets(pad_section: str, nets: dict[str, str | None]) -> str:
    def repl(m: re.Match[str]) -> str:
        num = m.group(1)
        body = m.group(0)
        if num not in nets or nets[num] is None:
            return re.sub(r"\n\t\t\t\(net \"[^\"]*\"\)\n", "\n", body)
        net = nets[num]
        if PAD_NET.search(body):
            return PAD_NET.sub(f'(net "{net}")', body, count=1)
        # Insert after (layers ...) block
        ins = re.search(r'(\n\t\t\t\(layers[^\n]+\)\n)', body)
        if ins:
            pos = ins.end(1)
            return body[:pos] + f'\t\t\t(net "{net}")\n' + body[pos:]
        return body

    return re.sub(
        r'\(pad "(\d+)" thru_hole oval.*?(?=\n\t\t\(pad |\n\t\t\(embedded_fonts|\n\t\t\(model |\Z)',
        repl,
        pad_section,
        flags=re.S,
    )


def extract_region(text: str) -> str | None:
    start = text.find(REGION_MARK)
    end = text.find(REGION_END)
    if start < 0 or end < 0 or end <= start:
        return None
    return text[start:end]


def sync_block_pads_only(
    block: str,
    pads: dict[str, tuple[float, float]],
    descr: str | None,
    *,
    fp_rot: float = 0.0,
) -> tuple[str, int]:
    changed = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal changed
        num = m.group(2)
        if num not in pads:
            return m.group(0)
        nx, ny = pads[num]
        ox, oy = float(m.group(3)), float(m.group(4))
        old_rot = m.group(5)
        if abs(ox - nx) > 1e-6 or abs(oy - ny) > 1e-6:
            changed += 1
        if fp_rot:
            want_rot = f"{fp_rot:g}"
            if old_rot != want_rot:
                changed += 1
            return f"{m.group(1)}{nx:g} {ny:g} {want_rot})"
        rot_s = f" {old_rot}" if old_rot is not None else ""
        return f"{m.group(1)}{nx:g} {ny:g}{rot_s})"

    new_block = PAD_AT.sub(repl, block)
    if descr is not None:

        def descr_repl(m: re.Match[str]) -> str:
            nonlocal changed
            if m.group(2) == descr:
                return m.group(0)
            changed += 1
            return f"{m.group(1)}{descr}{m.group(3)}"

        new_block = DESCR_LINE.sub(descr_repl, new_block, count=1)
    return new_block, changed


def sync_block_full(block: str, lib_mod: str) -> tuple[str, int]:
    lib_region = extract_region(lib_mod)
    if lib_region is None:
        return block, 0
    old_region = extract_region(block)
    if old_region is None:
        return block, 0
    nets = pad_nets(block)
    fp_rot = footprint_placement_rotation(block)
    new_region = stamp_oval_pad_rotation(inject_pad_nets(lib_region, nets), fp_rot)
    if new_region.strip() == old_region.strip():
        return block, 0
    new_block = block.replace(old_region, new_region, 1)
    # descr/tags from library
    descr = load_descr(lib_mod)
    if descr:
        new_block = DESCR_LINE.sub(
            lambda m: f'{m.group(1)}{descr}{m.group(3)}',
            new_block,
            count=1,
        )
    tm = TAGS_LINE.search(lib_mod)
    if tm:
        new_block = TAGS_LINE.sub(
            lambda m: f'{m.group(1)}{tm.group(2)}{m.group(3)}',
            new_block,
            count=1,
        )
    return new_block, 1


def sync_pcb(
    text: str,
    lib_dir: Path,
    footprint_names: set[str],
    *,
    full: bool = False,
) -> tuple[str, int]:
    lib_cache: dict[str, str] = {}
    pad_cache: dict[str, tuple[dict[str, tuple[float, float]], str | None]] = {}
    for name in footprint_names:
        path = lib_dir / f"{name}.kicad_mod"
        if not path.is_file():
            print(f"skip missing library footprint: {path}", file=sys.stderr)
            continue
        mod = path.read_text(encoding="utf-8")
        lib_cache[name] = mod
        pad_cache[name] = (parse_pad_xy(mod), load_descr(mod))

    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    total = 0

    while i < len(lines):
        m = FP_HEAD.match(lines[i].rstrip("\n"))
        if not m or m.group(1) not in lib_cache:
            out.append(lines[i])
            i += 1
            continue

        fp_name = m.group(1)
        start = i
        depth = lines[i].count("(") - lines[i].count(")")
        i += 1
        while i < len(lines) and depth > 0:
            depth += lines[i].count("(") - lines[i].count(")")
            i += 1
        block = "".join(lines[start:i])
        fp_rot = footprint_placement_rotation(block)
        if full:
            new_block, n = sync_block_full(block, lib_cache[fp_name])
        else:
            pads, descr = pad_cache[fp_name]
            new_block, n = sync_block_pads_only(block, pads, descr, fp_rot=fp_rot)
        if n == 0 and fp_rot:
            stamped = stamp_oval_pad_rotation(block, fp_rot)
            if stamped != block:
                new_block = stamped
                n = 1
        total += n
        out.append(new_block)

    return "".join(out), total


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pcb", type=Path, help="path to .kicad_pcb")
    ap.add_argument(
        "--library",
        type=Path,
        default=DEFAULT_LIB,
        help="Retr01_Lib.pretty with canonical footprints",
    )
    ap.add_argument(
        "--footprint",
        action="append",
        default=[],
        help="footprint basename (repeatable); default: Switchcraft TRS jack",
    )
    ap.add_argument(
        "--full",
        action="store_true",
        help="replace silk/fab/courtyard/pads/model from library (keeps pad nets)",
    )
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    names = set(args.footprint) or {"Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical"}
    lib = args.library.resolve()
    pcb = args.pcb.resolve()
    if not pcb.is_file():
        raise SystemExit(f"missing PCB: {pcb}")
    if not lib.is_dir():
        raise SystemExit(f"missing library: {lib}")

    text = pcb.read_text(encoding="utf-8")
    new_text, n = sync_pcb(text, lib, names, full=args.full)
    if n == 0:
        print("embedded footprints already match library")
        return
    mode = "full geometry" if args.full else "pad/descr"
    print(f"updated {n} footprint(s) ({mode}) in {pcb.name}")
    if args.dry_run:
        return
    pcb.write_text(new_text, encoding="utf-8")


if __name__ == "__main__":
    main()

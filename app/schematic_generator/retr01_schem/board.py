"""Top-level motherboard and cartridge assemblies."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional

from .bom import BoardId, CART_IC_COUNT, IC_COUNT_TARGET, SYSTEM_IC_COUNT, silicon_ic_entries, _C0603, _R0603
from .pinmap import power_pin_nums
from .manifest import build_manifest, manifest_gaps
from .cart_manifest import build_cart_manifest, j36_pin_to_net
from .parts import instantiate_bom, make_passive, skidl_available


def export_manifest_json(path: Path, *, board: BoardId = BoardId.MOBO) -> None:
    if board == BoardId.CART:
        manifest = build_cart_manifest()
        gaps: List[str] = []
    else:
        manifest = build_manifest()
        gaps = manifest_gaps()
    payload = [
        {
            "net": c.net,
            "a": {"refdes": c.a_refdes, "pin": c.a_pin},
            "b": {"refdes": c.b_refdes, "pin": c.b_pin},
            "source": c.source,
        }
        for c in manifest
    ]
    path.write_text(json.dumps({"board": board.value, "connections": payload, "gaps": gaps}, indent=2) + "\n")


def add_decoupling(
    parts: Dict[str, object],
    nets: Dict[str, object],
    board: BoardId = BoardId.MOBO,
) -> Dict[str, object]:
    """100nF across VCC/GND for every silicon IC on this board."""
    if not skidl_available():
        return nets

    from skidl import Net

    vcc = nets.get("+5V") or Net("+5V")
    gnd = nets.get("GND") or Net("GND")
    nets["+5V"] = vcc
    nets["GND"] = gnd

    for n, entry in enumerate(silicon_ic_entries(board), start=1):
        pins = power_pin_nums(entry.mpn)
        if pins is None:
            continue
        vcc_name, gnd_name = pins
        part = parts.get(entry.refdes)
        if part is None:
            continue
        cap = make_passive("C_100N", f"CD{n}", _C0603)
        parts[f"CD{n}"] = cap
        try:
            vp = part[vcc_name]
            gp = part[gnd_name]
            vp += vcc
            gp += gnd
            cap["1"] += vcc
            cap["2"] += gnd
        except Exception:
            continue
    return nets


def add_r2r_passives(parts: Dict[str, object]) -> None:
    """Instantiate video weighted DAC + audio R-2R + termination passives."""
    if not skidl_available():
        return
    # Video: binary-weighted into each gun (MSB=1k, mid=2k, LSB=4k). Studio packing
    # (rr<<5)|(gg<<2)|bb → PROM D7..D5=R, D4..D2=G, D1..D0=B (docs/passive_rf_etc.md).
    for ref, mpn in (
        ("RR0", "R_4K"),
        ("RR1", "R_2K"),
        ("RR2", "R_1K"),
        ("RG0", "R_4K"),
        ("RG1", "R_2K"),
        ("RG2", "R_1K"),
        ("RB0", "R_2K"),
        ("RB1", "R_1K"),
    ):
        parts[ref] = make_passive(mpn, ref, _R0603)
    for ref in ("R75R", "R75G", "R75B"):
        parts[ref] = make_passive("R_75", ref, _R0603)
    # Audio: classic 8-bit R-2R (R=10k, 2R=20k). AUD0=LSB .. AUD7=MSB.
    for i in range(8):
        parts[f"Ra2r{i}"] = make_passive("R_20K", f"Ra2r{i}", _R0603)
    for i in range(7):
        parts[f"Rar{i}"] = make_passive("R_10K", f"Rar{i}", _R0603)
    parts["Raterm"] = make_passive("R_20K", "Raterm", _R0603)
    parts["Raud"] = make_passive("R_1K", "Raud", _R0603)
    parts["Caud"] = make_passive("C_10U_AUD", "Caud", _C0603)


def build_board(include_sim_only: bool = False) -> Dict[str, Any]:
    """Instantiate motherboard BOM and apply mobo manifest (no cart silicon)."""
    from .connect import apply_connections
    from .islands import apu, beam, cart_socket, cpu, io_latch, mcu_linebuf, power_clk, video, vram

    manifest = build_manifest()
    parts = instantiate_bom(include_sim_only=include_sim_only, board=BoardId.MOBO)
    add_r2r_passives(parts)
    nets: Dict[str, object] = {}
    for island in (power_clk, cpu, io_latch, vram, beam, cart_socket, apu, mcu_linebuf, video):
        nets.update(island.wire(parts, island.LETTER))
    nets.update(apply_connections(parts, manifest))
    nets = add_decoupling(parts, nets, BoardId.MOBO)
    return {"parts": parts, "nets": nets, "manifest": manifest}


def build_cart() -> Dict[str, Any]:
    """Instantiate cart BOM (J36 + U40 + U50) and apply cart manifest."""
    from .connect import apply_connections

    manifest = build_cart_manifest()
    parts = instantiate_bom(board=BoardId.CART)
    nets: Dict[str, object] = {}
    nets.update(apply_connections(parts, manifest))
    nets = add_decoupling(parts, nets, BoardId.CART)
    return {"parts": parts, "nets": nets, "manifest": manifest}


def connect_unused_pins_to_nc() -> None:
    """Give every unused pin its own dummy net so KiCad sees the pad.

    SKiDL's NCNet is omitted from KiCad netlists, which produces
    'No net found ... (no pin N in symbol)' on import. Unique Net()
    names keep pads in the netlist without shorting unused pins together.
    """
    import builtins

    from skidl import Net

    circuit = builtins.default_circuit
    for part in circuit.parts:
        ref = getattr(part, "ref", "U")
        for pin in part.pins:
            if pin.nets:
                continue
            dummy = Net(f"NC_{ref}_{pin.num}")
            pin += dummy


def generate_netlist(out_dir: Path, basename: str = "retr01_mobo") -> Path:
    if not skidl_available():
        raise RuntimeError("skidl is not installed; pip install -r requirements.txt")

    from skidl import ERC, generate_netlist, reset

    reset()
    if basename.endswith("_cart") or basename == "retr01_cart":
        build_cart()
    else:
        build_board(include_sim_only=False)
    connect_unused_pins_to_nc()
    out_dir.mkdir(parents=True, exist_ok=True)
    net_path = out_dir / f"{basename}.net"
    generate_netlist(file=str(net_path))
    ERC()
    return net_path


def generate_both_netlists(out_dir: Path) -> Dict[str, Path]:
    """Write retr01_mobo.net and retr01_cart.net."""
    mobo = generate_netlist(out_dir, "retr01_mobo")
    cart = generate_netlist(out_dir, "retr01_cart")
    return {"mobo": mobo, "cart": cart}


def validate_j36_contract() -> List[str]:
    """Every J36 pin net on the motherboard must match the cart edge."""
    errors: List[str] = []
    try:
        mobo = j36_pin_to_net(build_manifest())
        cart = j36_pin_to_net(build_cart_manifest())
    except ValueError as e:
        return [str(e)]
    mobo_pins = set(mobo)
    cart_pins = set(cart)
    for pin in sorted(mobo_pins | cart_pins, key=lambda p: int(p) if p.isdigit() else p):
        if pin not in mobo:
            errors.append(f"J36 pin {pin} on cart missing on mobo")
        elif pin not in cart:
            errors.append(f"J36 pin {pin} on mobo missing on cart")
        elif mobo[pin] != cart[pin]:
            errors.append(f"J36 pin {pin}: mobo {mobo[pin]!r} != cart {cart[pin]!r}")
    return errors


def validate_bom_counts() -> List[str]:
    errors: List[str] = []
    mobo_ics = silicon_ic_entries(BoardId.MOBO)
    cart_ics = silicon_ic_entries(BoardId.CART)
    system_ics = silicon_ic_entries(None)
    if len(mobo_ics) != IC_COUNT_TARGET:
        errors.append(f"expected {IC_COUNT_TARGET} motherboard silicon ICs, got {len(mobo_ics)}")
    if len(cart_ics) != CART_IC_COUNT:
        errors.append(f"expected {CART_IC_COUNT} cart silicon ICs, got {len(cart_ics)}")
    if len(system_ics) != SYSTEM_IC_COUNT:
        errors.append(f"expected {SYSTEM_IC_COUNT} system silicon ICs, got {len(system_ics)}")
    for board in (BoardId.MOBO, BoardId.CART):
        seen = set()
        for e in silicon_ic_entries(board):
            if e.refdes in seen:
                errors.append(f"duplicate refdes {e.refdes} on {board.value}")
            seen.add(e.refdes)
    errors.extend(validate_j36_contract())
    return errors

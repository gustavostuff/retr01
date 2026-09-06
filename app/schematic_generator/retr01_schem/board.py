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


def _rail_name(net) -> str:
    return str(getattr(net, "name", "") or "")


def _pick_supply_rail(pin, vcc, analog):
    """Prefer an existing named rail on the pin (e.g. +5V_ANALOG for U24)."""
    for n in list(getattr(pin, "nets", None) or []):
        name = _rail_name(n)
        if name == "+5V_ANALOG":
            return analog
        if name == "+5V":
            return vcc
    return vcc


def add_decoupling(
    parts: Dict[str, object],
    nets: Dict[str, object],
    board: BoardId = BoardId.MOBO,
) -> Dict[str, object]:
    """100nF bypass per silicon IC with Quilter-friendly exclusive VCC nets.

    Quilter assigns bypass parents with high confidence when the cap shares a
    net with **one** IC power pin (explicit local net), not a shared +5V label.
    Pattern per IC::

        +5V --[RD*]-- +5V_<refdes> -- IC.VCC
                           |
                          CD*
                           |
                          GND

    RD* is a populated 0 ohm (power path). Cap pin 1 lives only on +5V_<refdes>
    with that IC VCC and the 0R, so Circuit Comprehension can parent CD* to the IC.
    """
    if not skidl_available():
        return nets

    from skidl import Net

    from .connect import rail_net

    vcc = nets.get("+5V") or rail_net("+5V")
    gnd = nets.get("GND") or rail_net("GND")
    analog = nets.get("+5V_ANALOG") or rail_net("+5V_ANALOG")
    # Keep canonical names if SKiDL tried to rename after merges.
    vcc.name = "+5V"
    gnd.name = "GND"
    analog.name = "+5V_ANALOG"
    nets["+5V"] = vcc
    nets["GND"] = gnd
    nets["+5V_ANALOG"] = analog

    for n, entry in enumerate(silicon_ic_entries(board), start=1):
        pins = power_pin_nums(entry.mpn)
        if pins is None:
            continue
        vcc_name, gnd_name = pins
        part = parts.get(entry.refdes)
        if part is None:
            continue
        cap_ref = f"CD{n}"
        bridge_ref = f"RD{n}"
        local_name = f"+5V_{entry.refdes}"
        try:
            vp = part[vcc_name]
            gp = part[gnd_name]
            supply = _pick_supply_rail(vp, vcc, analog)
            # Drop any prior direct rail tie (manifest may have put VCC on +5V).
            vp.disconnect()
            local = Net(local_name)
            local.name = local_name
            nets[local_name] = local

            cap = make_passive("C_100N", cap_ref, _C0603)
            bridge = make_passive("R_0", bridge_ref, _R0603)
            try:
                bridge.value = "0"
            except Exception:
                pass
            parts[cap_ref] = cap
            parts[bridge_ref] = bridge

            vp += local
            cap["1"] += local
            bridge["1"] += local
            bridge["2"] += supply
            gp += gnd
            cap["2"] += gnd
        except Exception:
            continue
    return nets


def add_ad725_decoupling(parts: Dict[str, object], nets: Dict[str, object]) -> Dict[str, object]:
    """Exclusive VCC+GND locals for AD725 APOS/DPOS bypass (Quilter parents).

    Cap pin1 and the power pin share one net. Cap pin2 and the matching ground
    pin share another. 0R bridges back to +5V_ANALOG / GND. That pin-to-pin
    pair is what Quilter treats as high-confidence (shared GND with AGND+DGND
    was leaving Cd725a at medium confidence).
    """
    if not skidl_available():
        return nets

    from skidl import Net

    from .connect import rail_net
    from .pinmap import AD725_AGND, AD725_APOS, AD725_DGND, AD725_DPOS

    u725 = parts.get("U725")
    if u725 is None:
        return nets

    analog = nets.get("+5V_ANALOG") or rail_net("+5V_ANALOG")
    gnd = nets.get("GND") or rail_net("GND")
    analog.name = "+5V_ANALOG"
    gnd.name = "GND"
    nets["+5V_ANALOG"] = analog
    nets["GND"] = gnd

    def _pair(
        vpin: str,
        gpin: str,
        vlocal: str,
        glocal: str,
        cap_ref: str,
        rdv: str,
        rdg: str,
    ) -> None:
        vp = u725[vpin]
        gp = u725[gpin]
        vp.disconnect()
        gp.disconnect()
        vn = Net(vlocal)
        gn = Net(glocal)
        vn.name = vlocal
        gn.name = glocal
        nets[vlocal] = vn
        nets[glocal] = gn
        bv = make_passive("R_0", rdv, _R0603)
        bg = make_passive("R_0", rdg, _R0603)
        try:
            bv.value = "0R"
            bg.value = "0R"
        except Exception:
            pass
        parts[rdv] = bv
        parts[rdg] = bg
        vp += vn
        bv["1"] += vn
        bv["2"] += analog
        gp += gn
        bg["1"] += gn
        bg["2"] += gnd
        cap = parts.get(cap_ref)
        if cap is None:
            return
        try:
            cap["1"].disconnect()
            cap["2"].disconnect()
        except Exception:
            pass
        cap["1"] += vn
        cap["2"] += gn

    _pair(
        AD725_APOS,
        AD725_AGND,
        "+5V_U725_APOS",
        "GND_U725_AGND",
        "Cd725a",
        "RD725a",
        "RD725ag",
    )
    _pair(
        AD725_DPOS,
        AD725_DGND,
        "+5V_U725_DPOS",
        "GND_U725_DGND",
        "Cd725d",
        "RD725d",
        "RD725dg",
    )
    return nets


def isolate_non_bypass_caps(parts: Dict[str, object], nets: Dict[str, object]) -> Dict[str, object]:
    """Keep Cbulk / Cytrap from looking like IC bypass to Quilter.

    Quilter flags any C that sits on a power rail + GND shared with IC pins.
    Cbulk is entry bulk (not U2 bypass). Cytrap is the YTRAP tank C (not an L
    bypass). Pull them onto private nets with no IC power pins.
    """
    if not skidl_available():
        return nets

    from skidl import Net

    from .connect import rail_net

    vcc = nets.get("+5V") or rail_net("+5V")
    gnd = nets.get("GND") or rail_net("GND")
    vcc.name = "+5V"
    gnd.name = "GND"
    nets["+5V"] = vcc
    nets["GND"] = gnd

    # Cbulk: FB1 output stub, then 0R into the main +5V rail.
    cbulk = parts.get("Cbulk")
    fb1 = parts.get("FB1")
    if cbulk is not None and fb1 is not None:
        try:
            fb1["2"].disconnect()
            cbulk["1"].disconnect()
            cbulk["2"].disconnect()
        except Exception:
            pass
        bulk = Net("+5V_BULK")
        bulk.name = "+5V_BULK"
        nets["+5V_BULK"] = bulk
        bridge = make_passive("R_0", "RDbulk", _R0603)
        try:
            bridge.value = "0R"
        except Exception:
            pass
        parts["RDbulk"] = bridge
        fb1["2"] += bulk
        cbulk["1"] += bulk
        bridge["1"] += bulk
        bridge["2"] += vcc
        cbulk["2"] += gnd
        try:
            cbulk.value = "220uF"
        except Exception:
            pass

    # Cytrap: keep series L-C, but return through YTRAP_RET (not a GND-named net)
    # so Quilter does not classify it as bypass-to-ground.
    cytrap = parts.get("Cytrap")
    if cytrap is not None:
        try:
            cytrap["2"].disconnect()
        except Exception:
            pass
        ret = Net("YTRAP_RET")
        ret.name = "YTRAP_RET"
        nets["YTRAP_RET"] = ret
        bridge = make_passive("R_0", "RDytrap", _R0603)
        try:
            bridge.value = "0R"
        except Exception:
            pass
        parts["RDytrap"] = bridge
        cytrap["2"] += ret
        bridge["1"] += ret
        bridge["2"] += gnd

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
    from .islands import apu, beam, cart_socket, cpu, soft_fe, mcu_linebuf, power_clk, video, vram

    manifest = build_manifest()
    parts = instantiate_bom(include_sim_only=include_sim_only, board=BoardId.MOBO)
    add_r2r_passives(parts)
    nets: Dict[str, object] = {}
    for island in (power_clk, cpu, soft_fe, vram, beam, cart_socket, apu, mcu_linebuf, video):
        nets.update(island.wire(parts, island.LETTER))
    nets.update(apply_connections(parts, manifest))
    nets = add_decoupling(parts, nets, BoardId.MOBO)
    nets = add_ad725_decoupling(parts, nets)
    nets = isolate_non_bypass_caps(parts, nets)
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


def _force_rail_net_names() -> None:
    """Ensure power rails keep stable KiCad names after island merges."""
    from .connect import rail_net

    for name in ("GND", "+5V", "+5V_ANALOG"):
        try:
            n = rail_net(name)
            n.name = name
        except Exception:
            continue


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
    _force_rail_net_names()
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

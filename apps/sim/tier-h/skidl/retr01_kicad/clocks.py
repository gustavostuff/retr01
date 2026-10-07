"""Motherboard discrete clock generation networks (Y1/Y2/Y3, 74HCU04, 74HC74).

Implements the discrete Pierce crystal oscillators and flip-flop divider
documented in docs/general/hardware.md and docs/passive_bom.md:
- CPU Clock (8.000 MHz): Y1 crystal + 74HCU04 inverters (Gates 3 & 4) + R31 (1M) + C25/C26
- Dot Clock (5.369318 MHz): Y2 crystal (21.47727 MHz) + 74HCU04 inverters (Gates 1 & 2)
  + R32 (1M) + C21/C22 + SN74HC74 dual D flip-flop divider (Stage 1 divide-by-2 to 10.738 MHz,
  Stage 2 divide-by-2 to 5.369 MHz DOT clock)
- FSC Clock (3.579545 MHz): Y3 crystal + C23/C24 directly to AD724 on-chip oscillator pins
"""

from __future__ import annotations

from typing import Callable, Dict

CLOCK_REFDES = (
    "U04",
    "U74",
    "Y1",
    "Y2",
    "Y3",
    "R31",
    "R32",
    "C21",
    "C22",
    "C23",
    "C24",
    "C25",
    "C26",
)


def ensure_clock_parts(
    parts: Dict[str, object],
    part_for_refdes: Callable[[str], object],
) -> None:
    for ref in CLOCK_REFDES:
        if ref not in parts:
            parts[ref] = part_for_refdes(ref)


def _ensure_net(nets_map: dict, name: str):
    from skidl import Net

    if name not in nets_map:
        nets_map[name] = Net(name)
    return nets_map[name]


def wire_clocks(parts: dict, nets_map: dict, pin_connect: Callable) -> None:
    """Wire discrete Pierce oscillators and 74HC74 divider per docs/general/hardware.md."""
    gnd = _ensure_net(nets_map, "GND")
    v5 = _ensure_net(nets_map, "+5V")
    nc = _ensure_net(nets_map, "NC")

    phi2 = _ensure_net(nets_map, "PHI2")
    dot = _ensure_net(nets_map, "DOT")

    # Power rails for clock ICs
    if "U04" in parts:
        pin_connect(parts["U04"], "", "14", v5)
        pin_connect(parts["U04"], "", "7", gnd)
    if "U74" in parts:
        pin_connect(parts["U74"], "", "14", v5)
        pin_connect(parts["U74"], "", "7", gnd)

    # -----------------------------------------------------------------------
    # 1. Dot clock (21.47727 MHz master crystal -> 74HCU04 -> 74HC74 -> 5.369 MHz DOT)
    # -----------------------------------------------------------------------
    xtal_dot_in = _ensure_net(nets_map, "XTAL_DOT_IN")
    xtal_dot_out = _ensure_net(nets_map, "XTAL_DOT_OUT")
    clk_21m = _ensure_net(nets_map, "CLK_21M")

    # Gate 1: Pierce oscillator amplifier (pins 1A=1, 1Y=2)
    # Gate 2: Buffer stage (pins 2A=3, 2Y=4)
    if "U04" in parts:
        pin_connect(parts["U04"], "", "1", xtal_dot_in)
        pin_connect(parts["U04"], "", "2", xtal_dot_out)
        pin_connect(parts["U04"], "", "3", xtal_dot_out)
        pin_connect(parts["U04"], "", "4", clk_21m)
    if "Y2" in parts:
        pin_connect(parts["Y2"], "", "1", xtal_dot_in)
        pin_connect(parts["Y2"], "", "2", xtal_dot_out)
    if "R32" in parts:
        pin_connect(parts["R32"], "", "1", xtal_dot_in)
        pin_connect(parts["R32"], "", "2", xtal_dot_out)
    if "C21" in parts:
        pin_connect(parts["C21"], "", "1", xtal_dot_in)
        pin_connect(parts["C21"], "", "2", gnd)
    if "C22" in parts:
        pin_connect(parts["C22"], "", "1", xtal_dot_out)
        pin_connect(parts["C22"], "", "2", gnd)

    # 74HC74 Stage 1: divide 21.477 MHz by 2 -> 10.738 MHz
    clk_10m = _ensure_net(nets_map, "CLK_10M")
    clk_10m_inv = _ensure_net(nets_map, "CLK_10M_INV")
    if "U74" in parts:
        pin_connect(parts["U74"], "", "1", v5)           # 1CLR#
        pin_connect(parts["U74"], "", "4", v5)           # 1PRE#
        pin_connect(parts["U74"], "", "3", clk_21m)      # 1CLK
        pin_connect(parts["U74"], "", "2", clk_10m_inv)  # 1D
        pin_connect(parts["U74"], "", "6", clk_10m_inv)  # 1/Q
        pin_connect(parts["U74"], "", "5", clk_10m)      # 1Q

        # 74HC74 Stage 2: divide 10.738 MHz by 2 -> 5.369 MHz DOT clock
        dot_inv = _ensure_net(nets_map, "DOT_INV")
        pin_connect(parts["U74"], "", "10", v5)          # 2PRE#
        pin_connect(parts["U74"], "", "13", v5)          # 2CLR#
        pin_connect(parts["U74"], "", "11", clk_10m)     # 2CLK
        pin_connect(parts["U74"], "", "12", dot_inv)     # 2D
        pin_connect(parts["U74"], "", "8", dot_inv)      # 2/Q
        pin_connect(parts["U74"], "", "9", dot)          # 2Q (DOT clock output)

    # -----------------------------------------------------------------------
    # 2. CPU clock (8.000 MHz crystal -> 74HCU04 -> PHI2)
    # -----------------------------------------------------------------------
    xtal_cpu_in = _ensure_net(nets_map, "XTAL_CPU_IN")
    xtal_cpu_out = _ensure_net(nets_map, "XTAL_CPU_OUT")

    # Gate 3: Pierce oscillator amplifier (pins 3A=5, 3Y=6)
    # Gate 4: Buffer stage (pins 4A=9, 4Y=8)
    if "U04" in parts:
        pin_connect(parts["U04"], "", "5", xtal_cpu_in)
        pin_connect(parts["U04"], "", "6", xtal_cpu_out)
        pin_connect(parts["U04"], "", "9", xtal_cpu_out)
        pin_connect(parts["U04"], "", "8", phi2)
        # Spare gates 5 and 6: tie inputs inactive (GND), outputs NC
        pin_connect(parts["U04"], "", "11", gnd)         # 5A
        pin_connect(parts["U04"], "", "10", nc)          # 5Y
        pin_connect(parts["U04"], "", "13", gnd)         # 6A
        pin_connect(parts["U04"], "", "12", nc)          # 6Y
    if "Y1" in parts:
        pin_connect(parts["Y1"], "", "1", xtal_cpu_in)
        pin_connect(parts["Y1"], "", "2", xtal_cpu_out)
    if "R31" in parts:
        pin_connect(parts["R31"], "", "1", xtal_cpu_in)
        pin_connect(parts["R31"], "", "2", xtal_cpu_out)
    if "C25" in parts:
        pin_connect(parts["C25"], "", "1", xtal_cpu_in)
        pin_connect(parts["C25"], "", "2", gnd)
    if "C26" in parts:
        pin_connect(parts["C26"], "", "1", xtal_cpu_out)
        pin_connect(parts["C26"], "", "2", gnd)

    # -----------------------------------------------------------------------
    # 3. FSC subcarrier crystal Y3 (3.579545 MHz to AD724 FIN and FSC_XTAL)
    # -----------------------------------------------------------------------
    fsc_xtal = _ensure_net(nets_map, "FSC_XTAL")
    fin = _ensure_net(nets_map, "FSC_FIN")
    if "Y3" in parts:
        pin_connect(parts["Y3"], "", "1", fin)
        pin_connect(parts["Y3"], "", "2", fsc_xtal)
    if "C23" in parts:
        pin_connect(parts["C23"], "", "1", fin)
        pin_connect(parts["C23"], "", "2", gnd)
    if "C24" in parts:
        pin_connect(parts["C24"], "", "1", fsc_xtal)
        pin_connect(parts["C24"], "", "2", gnd)
    if "U725" in parts:
        pin_connect(parts["U725"], "", "3", fin)

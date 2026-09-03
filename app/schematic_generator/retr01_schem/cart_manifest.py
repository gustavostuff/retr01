"""Cartridge PCB wiring — SST39SF040 + 24C64 behind the shared J36 edge pinout."""

from __future__ import annotations

from typing import Dict, List

from . import pinmap as P
from .manifest import Connection


def build_cart_manifest() -> List[Connection]:
    """Cart-side connections: J36 fingers ↔ flash / EEPROM (docs/cart.md)."""
    m: List[Connection] = []
    src = "docs/cart.md cart PCB"

    for pin in (P.cart_a(1), P.cart_a(18), P.cart_b(1)):
        m.append(Connection("GND", "J36", pin, "GND", "GND", src))
    for pin in (P.cart_a(2), P.cart_b(2)):
        m.append(Connection("+5V", "J36", pin, "+5V", "+5V", src))

    m += [
        Connection("I2C_SDA", "J36", P.cart_a(3), "U50", P.EE_SDA, src),
        Connection("I2C_SCL", "J36", P.cart_b(3), "U50", P.EE_SCL, src),
        Connection("GND", "U50", P.EE_A0, "GND", "GND", src),
        Connection("GND", "U50", P.EE_A1, "GND", "GND", src),
        Connection("GND", "U50", P.EE_A2, "GND", "GND", src),
        Connection("GND", "U50", P.EE_WP, "GND", "GND", src),
        Connection("+5V", "U50", P.EE_VCC, "+5V", "+5V", src),
        Connection("GND", "U50", P.EE_GND, "GND", "GND", src),
    ]

    for i in range(14):
        edge = P.cart_a(i + 4)
        m.append(Connection(f"CART_A{i}", "J36", edge, "U40", P.FLASH_A[i], src))
    for i, edge_n in enumerate(range(13, 18)):
        bit = 14 + i
        m.append(Connection(f"CART_A{bit}", "J36", P.cart_b(edge_n), "U40", P.FLASH_A[bit], src))
    for i in range(8):
        m.append(Connection(f"CART_D{i}", "J36", P.cart_b(i + 4), "U40", P.FLASH_D[i], src))

    m += [
        Connection("CART_OE_N", "J36", P.cart_b(12), "U40", P.FLASH_OE, src),
        Connection("CART_WE_N", "J36", P.cart_b(18), "U40", P.FLASH_WE, src),
        Connection("GND", "U40", P.FLASH_CE, "GND", "GND", src),
        Connection("+5V", "U40", P.FLASH_VCC, "+5V", "+5V", src),
        Connection("GND", "U40", P.FLASH_GND, "GND", "GND", src),
    ]
    return m


def j36_pin_to_net(connections: List[Connection]) -> Dict[str, str]:
    """Map J36 pin number → net name (one net per edge contact)."""
    out: Dict[str, str] = {}
    for c in connections:
        for ref, pin in ((c.a_refdes, c.a_pin), (c.b_refdes, c.b_pin)):
            if ref != "J36":
                continue
            prev = out.get(pin)
            if prev is not None and prev != c.net:
                raise ValueError(f"J36 pin {pin} has conflicting nets {prev!r} vs {c.net!r}")
            out[pin] = c.net
    return out

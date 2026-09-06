"""8-bit beam-Y vs raster-Y inequality (cascaded NElo/NEhi -> EQ)."""

from __future__ import annotations


def xor8(a: int, b: int) -> int:
    return (a ^ b) & 0xFF


def ne_lo(p: int, q: int) -> bool:
    x = xor8(p, q)
    return bool(x & 0x0F)


def ne_hi(p: int, q: int) -> bool:
    x = xor8(p, q)
    return bool(x & 0xF0)


def eq_high_when_ne(p: int, q: int) -> bool:
    """EQ output in upldy_eq8.pld: high when P != Q."""
    return ne_lo(p, q) or ne_hi(p, q)


def eq_active_low_when_equal(p: int, q: int) -> bool:
    """Silicon IRQ sense: EQ# low (False here means active) when equal.

    Modeled as not(EQ_high_when_ne).
    """
    return not eq_high_when_ne(p, q)

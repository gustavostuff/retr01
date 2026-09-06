"""Clock-enable register bank (scroll X/Y, raster Q, MAP A14-18)."""

from __future__ import annotations


class CeReg:
    """GAL-style: Q := LE ? D : Q on each clock edge."""

    def __init__(self, width: int) -> None:
        self.width = width
        self.mask = (1 << width) - 1
        self.q = 0

    def clock(self, d: int, le: bool) -> int:
        if le:
            self.q = d & self.mask
        return self.q

"""Island-oriented wiring helpers (mirrors app/sim island canvas)."""

from __future__ import annotations

from typing import List

from retr01_schem.bom import BOM_BY_REFDES, IslandId
from retr01_schem.manifest import Connection, build_manifest


def connections_for_island(island_letter: str) -> List[Connection]:
    """Filter manifest entries whose refdes maps to an island letter."""
    letter_map = {i.value: i for i in IslandId}
    target = letter_map.get(island_letter.upper())
    if target is None:
        return []

    island_refdes = {r for r, e in BOM_BY_REFDES.items() if e.island == target and not e.sim_only}
    out: List[Connection] = []
    for c in build_manifest():
        if c.a_refdes in island_refdes or c.b_refdes in island_refdes:
            out.append(c)
    return out

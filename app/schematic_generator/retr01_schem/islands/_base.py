"""Shared island module pattern."""

from __future__ import annotations

from typing import Dict, List

from retr01_schem.bom import BOM_BY_REFDES, BomEntry, IslandId
from retr01_schem.connect import apply_connections
from retr01_schem.islands import connections_for_island
from retr01_schem.manifest import Connection


def island_entries(island: IslandId) -> List[BomEntry]:
    return [e for e in BOM_BY_REFDES.values() if e.island == island and not e.sim_only]


def island_connections(letter: str) -> List[Connection]:
    return connections_for_island(letter)


def wire_island(parts: Dict[str, object], letter: str) -> Dict[str, object]:
    return apply_connections(parts, connections_for_island(letter))

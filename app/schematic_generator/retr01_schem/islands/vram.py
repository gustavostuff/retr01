from retr01_schem.bom import IslandId
from retr01_schem.islands._base import island_connections, island_entries, wire_island

ISLAND = IslandId.VRAM
LETTER = ISLAND.value

refdes = lambda: [e.refdes for e in island_entries(ISLAND)]
connections = lambda: island_connections(LETTER)
wire = wire_island

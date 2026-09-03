"""Cart socket island — 36-pin edge (docs/cart.md) + HC245 U20C."""

from retr01_schem.bom import IslandId
from retr01_schem.islands._base import island_connections, island_entries, wire_island

ISLAND = IslandId.CART_SOCKET
LETTER = ISLAND.value

# Motherboard-side parts: J36 edge + U20C. Cart flash/EEPROM live on island N
# but share CART_* / I2C nets through the edge pinout in manifest.py.

refdes = lambda: [e.refdes for e in island_entries(ISLAND)]
connections = lambda: island_connections(LETTER)
wire = wire_island

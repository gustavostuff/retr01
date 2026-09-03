"""32-IC BOM and refdes map (authority: bom32.h + docs/hardware.md + graphics.md)."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Dict, List, Optional, Tuple


class IslandId(str, Enum):
    POWER_CLK = "A"
    CPU = "C"
    IO_LATCH = "D"
    VRAM = "G"
    BEAM = "H"
    CART_SOCKET = "J"
    APU = "K"
    MCU_LINEBUF = "L"
    VIDEO = "O"
    CART_MODULE = "N"


class BoardId(str, Enum):
    MOBO = "mobo"
    CART = "cart"


@dataclass(frozen=True)
class BomEntry:
    refdes: str
    mpn: str
    role: str
    island: IslandId
    dip_pins: int
    footprint: str
    in_ic_count: bool = True
    sim_only: bool = False
    board: BoardId = BoardId.MOBO
    # Power pin names for automated decoupling (None = skip).
    vcc_pin: Optional[str] = "VCC"
    gnd_pin: Optional[str] = "GND"


# KiCad footprints (Package_DIP library). Tune when Retr01_Lib symbols land.
_DIP28 = "Package_DIP:DIP-28_W15.24mm"
_DIP32 = "Package_DIP:DIP-32_W15.24mm"
_DIP40 = "Package_DIP:DIP-40_W15.24mm"
_DIP24 = "Package_DIP:DIP-24_W15.24mm"
_DIP20 = "Package_DIP:DIP-20_W7.62mm"
_DIP16 = "Package_DIP:DIP-16_W7.62mm"
_DIP14 = "Package_DIP:DIP-14_W7.62mm"
_DIP8 = "Package_DIP:DIP-8_W7.62mm"
_C0603 = "Capacitor_SMD:C_0603_1608Metric"
_R0603 = "Resistor_SMD:R_0603_1608Metric"
# Placeholder 2x18 header until a real card-edge .kicad_mod is added.
_EDGE36 = "Connector_PinHeader_2.54mm:PinHeader_2x18_P2.54mm_Vertical"
_BARREL = "Connector_BarrelJack:BarrelJack_Horizontal"
_RGBS = "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical"
# KiCad 10 stock name includes _Horizontal (35RAPC not in fp-lib-table).
_TRS = "Connector_Audio:Jack_3.5mm_CUI_SJ1-3533NG_Horizontal"
_ARCADE10 = "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical"
_HDR4 = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
_HDR2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"
# Stock coaxial (BNC) placeholder for RCA — KiCad has no Cinch/RCA .kicad_mod.
_RCA = "Connector_Coaxial:BNC_PanelMountable_Vertical"

# Silicon target HC573 map (docs/graphics.md#hc573-latch-map-9-chips).
HC573_REFDES = ("U5A", "U5B", "U5C", "U5D", "U5E", "U5F", "U5G", "U5H", "U5I")
HC573_PORTS = (
    "FE00 PPUCTRL",
    "FE02 BG1 scroll X",
    "FE03 BG1 scroll Y",
    "FE04 raster Y",
    "FE05 raster control",
    "FE08 palette addr",
    "FE90 MAP seek lo",
    "FE91 MAP seek mid",
    "FE92 MAP seek hi",
)
# Port hex suffix for decode SEL_FExx nets (graphics.md table order).
HC573_PORT_HEX = ("00", "02", "03", "04", "05", "08", "90", "91", "92")

HC157_REFDES = ("U7A", "U7B", "U7C", "U7D", "U7E", "U7F")
HC157_ROLES = (
    "VRAM interleave A3-0",
    "VRAM interleave A7-4",
    "VRAM interleave A11-8",
    "linebuf mux A3-0",
    "linebuf mux A7-4",
    "linebuf mux A11-8",
)

PLD_REFDES = ("UPLDA", "UPLDB", "UPLDX", "UPLDY", "UPLDV")
PLD_ROLES = (
    "decode $FExx",
    "VRAM glue",
    "beam X / dot",
    "beam Y compare",
    "compositor priority",
)

HC245_REFDES = ("U20A", "U20B", "U20C")
HC245_ROLES = ("CPU data bus", "video peek", "cart flash data")


def _hc573_entries() -> List[BomEntry]:
    return [
        BomEntry(r, "SN74HC573", f"HC573 {port}", IslandId.IO_LATCH, 20, _DIP20)
        for r, port in zip(HC573_REFDES, HC573_PORTS)
    ]


def _hc157_entries() -> List[BomEntry]:
    return [
        BomEntry(r, "SN74HC157", f"HC157 {role}", IslandId.VRAM if i < 3 else IslandId.MCU_LINEBUF, 16, _DIP16)
        for i, (r, role) in enumerate(zip(HC157_REFDES, HC157_ROLES))
    ]


def _pld_entries() -> List[BomEntry]:
    island_map = {
        0: IslandId.CPU,
        1: IslandId.VRAM,
        2: IslandId.BEAM,
        3: IslandId.BEAM,
        4: IslandId.VIDEO,
    }
    return [
        BomEntry(r, "ATF22V10", f"PLD {role}", island_map[i], 24, _DIP24)
        for i, (r, role) in enumerate(zip(PLD_REFDES, PLD_ROLES))
    ]


def _hc245_entries() -> List[BomEntry]:
    island_map = (IslandId.CPU, IslandId.VIDEO, IslandId.CART_SOCKET)
    return [
        BomEntry(r, "SN74HC245", f"HC245 {role}", island_map[i], 20, _DIP20)
        for i, (r, role) in enumerate(zip(HC245_REFDES, HC245_ROLES))
    ]


def _decoupling_caps() -> List[BomEntry]:
    """Placeholder BOM slots; board.add_decoupling() instantiates by silicon IC."""
    return []


BOM: List[BomEntry] = [
    BomEntry("U1", "W65C02S", "game CPU 8 MHz", IslandId.CPU, 40, _DIP40, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U3", "AS6C62256", "system RAM", IslandId.CPU, 28, _DIP28, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U1284", "ATmega1284P", "OAM sprites pads EEPROM", IslandId.MCU_LINEBUF, 40, _DIP40),
    BomEntry("U328", "ATmega328P", "APU", IslandId.APU, 28, _DIP28),
    BomEntry("U6", "AS6C62256", "interleaved VRAM", IslandId.VRAM, 28, _DIP28, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U41", "AS6C62256", "sprite line buffer", IslandId.MCU_LINEBUF, 28, _DIP28, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U24", "AT27C256R", "color PROM", IslandId.VIDEO, 28, _DIP28, vcc_pin="VCC", gnd_pin="VSS"),
    # Cart PCB only (still counted in system 32-IC goal; not on motherboard netlist).
    BomEntry(
        "U40",
        "SST39SF040",
        "cart flash 512 KB",
        IslandId.CART_MODULE,
        32,
        _DIP32,
        board=BoardId.CART,
        vcc_pin="VDD",
        gnd_pin="VSS",
    ),
    BomEntry(
        "U50",
        "24C64",
        "cart save EEPROM",
        IslandId.CART_MODULE,
        8,
        _DIP8,
        board=BoardId.CART,
    ),
    *_pld_entries(),
    *_hc573_entries(),
    *_hc157_entries(),
    *_hc245_entries(),
    # Support (outside 32-IC count)
    BomEntry("U2", "SN74HC14", "reset / PHI2 conditioning", IslandId.POWER_CLK, 14, _DIP14, in_ic_count=False),
    BomEntry("Y1", "OSC8M", "PHI2 8 MHz", IslandId.POWER_CLK, 8, _DIP8, in_ic_count=False, vcc_pin="VDD", gnd_pin="GND"),
    BomEntry("Y2", "OSC_DOT", "dot clock ~5.37 MHz", IslandId.BEAM, 8, _DIP8, in_ic_count=False, vcc_pin="VDD", gnd_pin="GND"),
    # Cart edge on both boards (same electrical pinout / net names). Cart copy is CART_EDGE.
    BomEntry(
        "J36",
        "CART_EDGE_36",
        "36-pin cart edge (mobo receptacle)",
        IslandId.CART_SOCKET,
        36,
        _EDGE36,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "J36",
        "CART_EDGE_36",
        "36-pin cart edge (cart fingers)",
        IslandId.CART_MODULE,
        36,
        _EDGE36,
        in_ic_count=False,
        board=BoardId.CART,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry("J1", "BARREL_5V", "5 V barrel jack", IslandId.POWER_CLK, 3, _BARREL, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J2", "RGBS_HDR", "RGBS video out", IslandId.VIDEO, 5, _RGBS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J3", "TRS_P1", "Switchcraft 35RAPC P1", IslandId.MCU_LINEBUF, 3, _TRS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J4", "TRS_P2", "Switchcraft 35RAPC P2", IslandId.MCU_LINEBUF, 3, _TRS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J5", "ARCADE_P1", "arcade P1 1x10", IslandId.MCU_LINEBUF, 10, _ARCADE10, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J6", "ARCADE_P2", "arcade P2 1x10", IslandId.MCU_LINEBUF, 10, _ARCADE10, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J7", "CAB_PWR_RST", "cabinet +5V/GND/RESET 1x4", IslandId.POWER_CLK, 4, _HDR4, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("SW1", "SCALE_SW", "SCALE 1x/2x (open=2x)", IslandId.VIDEO, 2, _HDR2, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rscale", "R_10K", "SCALE_1X pull-down", IslandId.VIDEO, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry(
        "J8",
        "AUDIO_OUT",
        "mono audio RCA (BNC placeholder)",
        IslandId.APU,
        2,
        _RCA,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "J9",
        "COMPOSITE_OUT",
        "composite video RCA (BNC placeholder)",
        IslandId.VIDEO,
        2,
        _RCA,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    # Power-entry / rail passives (docs/passive_rf_etc.md)
    BomEntry("F1", "PPTC", "VIN PPTC", IslandId.POWER_CLK, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("FB1", "FERRITE", "5 V input ferrite", IslandId.POWER_CLK, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("D1", "SCHOTTKY", "reverse polarity", IslandId.POWER_CLK, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cbulk", "C_BULK", "100-470uF entry bulk", IslandId.POWER_CLK, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("FB2", "FERRITE", "analog video ferrite", IslandId.VIDEO, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cva", "C_10U", "10uF analog spur", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("F2", "PPTC", "TRS P1 VCC PPTC", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("F3", "PPTC", "TRS P2 VCC PPTC", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rpu1", "R_4K7", "pad DATA pull-up", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    # Sim / bench only
    BomEntry("U4", "PRG_ROM", "breadboard PRG fallback", IslandId.CPU, 28, _DIP28, in_ic_count=False, sim_only=True, vcc_pin=None, gnd_pin=None),
    BomEntry("SCR1", "SCREEN_SINK", "LCD sim sink", IslandId.VIDEO, 0, "", in_ic_count=False, sim_only=True, vcc_pin=None, gnd_pin=None),
]

BOM_BY_REFDES: Dict[str, BomEntry] = {
    e.refdes: e for e in BOM if e.board == BoardId.MOBO
}

# System silicon goal (mobo + cart) matches bom32.h / docs/hardware.md.
SYSTEM_IC_COUNT = 32
IC_COUNT_TARGET = 30  # motherboard silicon only (U40/U50 live on cart)
CART_IC_COUNT = 2


def entries_for_board(board: BoardId, *, include_sim_only: bool = False) -> List[BomEntry]:
    return [
        e
        for e in BOM
        if e.board == board and (include_sim_only or not e.sim_only)
    ]


def silicon_ic_entries(board: Optional[BoardId] = BoardId.MOBO) -> List[BomEntry]:
    """Silicon ICs for a board. Pass board=None for the full system (mobo+cart)."""
    out: List[BomEntry] = []
    for e in BOM:
        if not e.in_ic_count or e.sim_only:
            continue
        if board is not None and e.board != board:
            continue
        out.append(e)
    return out


def refdes_list(board: BoardId = BoardId.MOBO) -> List[str]:
    return [e.refdes for e in entries_for_board(board)]


def power_pins(entry: BomEntry) -> Optional[Tuple[str, str]]:
    if entry.vcc_pin is None or entry.gnd_pin is None:
        return None
    return entry.vcc_pin, entry.gnd_pin
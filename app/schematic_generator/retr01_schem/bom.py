"""23-IC BOM and refdes map (authority: docs/hardware.md + graphics.md).

Scroll/raster in ATF22V10. Soft $FExx on 1284. CART_A14-A18 on UPLDV.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Dict, List, Optional, Tuple


class IslandId(str, Enum):
    POWER_CLK = "A"
    CPU = "C"
    SOFT_FE = "D"  # soft $FExx canvas letter (no discrete packages)
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


class PassiveProfile(str, Enum):
    """Board passive population.

    BRINGUP (default generate): vertical axials, no cart/TRS TVS, no arcade series R.
    FULL: production ESD clamps + arcade 47 ohm series.
    """

    BRINGUP = "bringup"
    FULL = "full"


_PASSIVE_PROFILE = PassiveProfile.BRINGUP


def get_passive_profile() -> PassiveProfile:
    return _PASSIVE_PROFILE


def set_passive_profile(profile: PassiveProfile | str) -> PassiveProfile:
    global _PASSIVE_PROFILE
    _PASSIVE_PROFILE = PassiveProfile(profile)
    return _PASSIVE_PROFILE


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
    # Omit from BRINGUP netlist (still listed in BOM for FULL / docs).
    bringup_omit: bool = False


# KiCad footprints (Package_DIP library). Tune when Retr01_Lib symbols land.
# Width: W7.62mm = 0.300" skinny, W15.24mm = 0.600" wide (JEDEC memory / 40-pin).
_DIP28 = "Package_DIP:DIP-28_W15.24mm"  # AS6C62256, AT27C256R (600 mil)
_DIP28N = "Package_DIP:DIP-28_W7.62mm"  # ATmega328P-PU (300 mil skinny)
_DIP32 = "Package_DIP:DIP-32_W15.24mm"
_DIP40 = "Package_DIP:DIP-40_W15.24mm"
_DIP24 = "Package_DIP:DIP-24_W7.62mm"  # ATF22V10CQZ-20PU (300 mil skinny, not 600)
_DIP20 = "Package_DIP:DIP-20_W7.62mm"
_DIP16 = "Package_DIP:DIP-16_W7.62mm"
_DIP14 = "Package_DIP:DIP-14_W7.62mm"
_DIP8 = "Package_DIP:DIP-8_W7.62mm"
# Arcade/console first spin is fully THT. Only AD725 silicon is SOIC (on PA0006 DIP adapter).
# Vertical axials (P2.54) for board density. KiCad 9+ needs _Horizontal/_Vertical suffix.
_C_CER = "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm"  # 22pF / 100nF class
_C_ELEC = "Capacitor_THT:CP_Radial_D8.0mm_P3.50mm"  # 10uF / 220uF class
_R_AX = "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical"  # 1/4 W axial standing
_TVS = "Diode_THT:D_DO-35_SOD27_P2.54mm_Vertical_CathodeUp"  # axial ESD stand-in (real PESD is SOD-323)
_SCHOTTKY = "Diode_THT:D_DO-41_SOD81_P2.54mm_Vertical_CathodeUp"  # reverse-polarity rectifier (1N581x class)
_L_AX = "Inductor_THT:L_Axial_L11.0mm_D4.5mm_P5.08mm_Vertical_Fastron_MECC"  # ~68uH YTRAP
# PPTC / ferrite: no locked THT MPN yet. DIN0207 vertical is a hole-pattern stand-in only.
# board.py still imports _C0603 / _R0603 names for decoupling / R-2R helpers.
_C0603 = _C_CER
_R0603 = _R_AX
# Abracon ACH half-size DIP-8 can (pins 1/4/5/8). KiCad: Oscillator:Oscillator_DIP-8.
_OSC8 = "Oscillator:Oscillator_DIP-8"
# Locked EDAC straight (vertical insert) 2x18 for arcade / bring-up.
# Retr01-C console later: right-angle EDAC 395-036-559-212 + Horizontal stand-in.
# Mobo: pin-socket hole stand-in until EDAC manufacturer CAD.
# Cart: gold fingers in Retr01_Lib (mate to EDAC card-edge).
_EDGE36_MOBO = "Connector_PinSocket_2.54mm:PinSocket_2x18_P2.54mm_Vertical"
_EDGE36_CART = "Retr01_Lib:Cart_Edge_2x18_P2.54mm"
# CUI PJ-063AH 2.1 mm ID. Stock KiCad footprint (pads 1=tip, 2=sleeve, MP).
_BARREL = "Connector_BarrelJack:BarrelJack_CUI_PJ-063AH_Horizontal"
_RGBS = "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical"
# Switchcraft VN4 CD 5-hole pattern (see docs/passive_rf_etc.md). Locked SKU 35RAPC2BVN4.
_TRS = "Retr01_Lib:Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical"
_ARCADE10 = "Connector_PinHeader_2.54mm:PinHeader_1x10_P2.54mm_Vertical"
_HDR4 = "Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical"
_HDR2 = "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical"
# CUI RCJ-01x PCB RCA (color variants share holes). Custom .kicad_mod until upstream Cinch lands.
_RCA = "Retr01_Lib:CUI_RCJ-01x_Vertical"
_XTAL = "Crystal:Crystal_HC49-U_Vertical"

# $FExx ownership: docs/graphics.md. Hard PLD: FE02/FE03/FE04. Soft 1284: FE00/05/08/90-92.
FE_HARD_PLD_HEX = ("02", "03", "04")
FE_SOFT_1284_HEX = ("00", "05", "08", "90", "91", "92")
# BG0 scroll already soft on 1284 (FE06/FE07).
FE_SOFT_1284_BG0_HEX = ("06", "07")

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
    "VRAM glue + scroll X reg",
    "beam X / dot + scroll Y reg",
    "beam Y compare + raster Y reg",
    "compositor + cart A14-18 MAP export",
)

HC245_REFDES = ("U20A", "U20B", "U20C")
HC245_ROLES = ("CPU data bus", "video peek", "cart flash data")


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
    BomEntry("U328", "ATmega328P", "APU", IslandId.APU, 28, _DIP28N),
    BomEntry("U6", "AS6C62256", "interleaved VRAM", IslandId.VRAM, 28, _DIP28, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U41", "AS6C62256", "sprite line buffer", IslandId.MCU_LINEBUF, 28, _DIP28, vcc_pin="VDD", gnd_pin="VSS"),
    BomEntry("U24", "AT27C256R", "color PROM", IslandId.VIDEO, 28, _DIP28, vcc_pin="VCC", gnd_pin="VSS"),
    # Cart PCB only (counted in system 23-IC goal; not on motherboard netlist).
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
    *_hc157_entries(),
    *_hc245_entries(),
    # Support (outside 23-IC count)
    BomEntry("U2", "SN74HC14", "reset / PHI2 conditioning", IslandId.POWER_CLK, 14, _DIP14, in_ic_count=False),
    # Board clocks: Abracon ACH half-size DIP-8, 5 V HCMOS, -EK (+/-30 ppm, -20..+70 C)
    BomEntry(
        "Y1",
        "OSC8M",
        "Abracon ACH-8.000MHZ-EK (PHI2)",
        IslandId.POWER_CLK,
        8,
        _OSC8,
        in_ic_count=False,
        vcc_pin="VDD",
        gnd_pin="GND",
    ),
    BomEntry(
        "Y2",
        "OSC_DOT",
        "Abracon ACH-5.369318MHZ-EK (dot; Abracon factory-order OK)",
        IslandId.BEAM,
        8,
        _OSC8,
        in_ic_count=False,
        vcc_pin="VDD",
        gnd_pin="GND",
    ),
    # Composite AV path (outside 23-IC logic BOM). AD725 silicon is wide SOIC-16.
    # Mobo footprint is DIP-16 for Proto Advantage PA0006 (first-spin fully THT).
    BomEntry(
        "U725",
        "AD725",
        "AD725ARZ RGB->NTSC on DIP-16 via Proto Advantage PA0006 (chip wide SOIC-16)",
        IslandId.VIDEO,
        16,
        _DIP16,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "Y3",
        "OSC_4FSC",
        "Abracon ACH-14.31818MHZ-EK (AD725 4FSC)",
        IslandId.VIDEO,
        8,
        _OSC8,
        in_ic_count=False,
        vcc_pin="VDD",
        gnd_pin="GND",
    ),
    # AVR crystals (HC-49/U) — not cans; XTAL pins on 1284 / 328P
    BomEntry(
        "Y4",
        "XTAL_20M",
        "Abracon ABLS7M-20.000MHZ-D2Y-T (1284)",
        IslandId.MCU_LINEBUF,
        2,
        _XTAL,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "Y5",
        "XTAL_16M",
        "Abracon ABLS7M-16.000MHZ-D2Y-T (328P)",
        IslandId.APU,
        2,
        _XTAL,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry("Cxtal4a", "C_22P", "Y4 load", IslandId.MCU_LINEBUF, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cxtal4b", "C_22P", "Y4 load", IslandId.MCU_LINEBUF, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cxtal5a", "C_22P", "Y5 load", IslandId.APU, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cxtal5b", "C_22P", "Y5 load", IslandId.APU, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("CinR", "C_100N", "AD725 RIN AC couple", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("CinG", "C_100N", "AD725 GIN AC couple", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("CinB", "C_100N", "AD725 BIN AC couple", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("R75C", "R_75", "composite reverse-term series", IslandId.VIDEO, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Lytrap", "L_YTRAP", "AD725 YTRAP ~68uH NTSC", IslandId.VIDEO, 2, _L_AX, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cytrap", "C_100N", "AD725 YTRAP resonate C", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cd725a", "C_100N", "AD725 APOS decouple", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cd725d", "C_100N", "AD725 DPOS decouple", IslandId.VIDEO, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    # Cart edge on both boards (same electrical pinout / net names).
    BomEntry(
        "J36",
        "CART_EDGE_36",
        "EDAC 395-036-520-201 straight vertical 2x18 (mobo)",
        IslandId.CART_SOCKET,
        36,
        _EDGE36_MOBO,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "J36",
        "CART_EDGE_36",
        "cart gold fingers (mate to EDAC 395-036-520-201)",
        IslandId.CART_MODULE,
        36,
        _EDGE36_CART,
        in_ic_count=False,
        board=BoardId.CART,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry(
        "J1",
        "BARREL_5V",
        "CUI PJ-063AH 2.1 mm barrel",
        IslandId.POWER_CLK,
        3,
        _BARREL,
        in_ic_count=False,
        vcc_pin=None,
        gnd_pin=None,
    ),
    BomEntry("J2", "RGBS_HDR", "RGBS video out", IslandId.VIDEO, 5, _RGBS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J3", "TRS_P1", "Switchcraft 35RAPC2BVN4 P1 (vertical)", IslandId.MCU_LINEBUF, 5, _TRS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J4", "TRS_P2", "Switchcraft 35RAPC2BVN4 P2 (vertical)", IslandId.MCU_LINEBUF, 5, _TRS, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J5", "ARCADE_P1", "arcade P1 1x10", IslandId.MCU_LINEBUF, 10, _ARCADE10, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J6", "ARCADE_P2", "arcade P2 1x10", IslandId.MCU_LINEBUF, 10, _ARCADE10, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("J7", "CAB_PWR_RST", "cabinet +5V/GND/RESET 1x4", IslandId.POWER_CLK, 4, _HDR4, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("SW1", "SCALE_SW", "SCALE 1x/2x (open=2x)", IslandId.VIDEO, 2, _HDR2, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rscale", "R_10K", "SCALE_1X pull-down", IslandId.VIDEO, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry(
        "J8",
        "AUDIO_OUT",
        "CUI RCJ-012 black RCA (audio)",
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
        "CUI RCJ-014 yellow RCA (composite)",
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
    BomEntry("D1", "SCHOTTKY", "reverse polarity", IslandId.POWER_CLK, 2, _SCHOTTKY, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cbulk", "C_BULK", "220uF entry bulk (locked)", IslandId.POWER_CLK, 2, _C_ELEC, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("FB2", "FERRITE", "analog video ferrite", IslandId.VIDEO, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cva", "C_10U", "10uF analog spur", IslandId.VIDEO, 2, _C_ELEC, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    # TRS aux pads (docs/passive_rf_etc.md + controllers.md): PPTC, 100nF, TVS, series-R, pull-up
    BomEntry("F2", "PPTC", "TRS P1 VCC PPTC", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("F3", "PPTC", "TRS P2 VCC PPTC", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cpad1", "C_100N", "TRS P1 VCC decouple after PPTC", IslandId.MCU_LINEBUF, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Cpad2", "C_100N", "TRS P2 VCC decouple after PPTC", IslandId.MCU_LINEBUF, 2, _C0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("TvsV1", "TVS_5V", "TRS P1 Tip ESD (PESD5V0-class)", IslandId.MCU_LINEBUF, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsV2", "TVS_5V", "TRS P2 Tip ESD", IslandId.MCU_LINEBUF, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsD1", "TVS_5V", "TRS P1 DATA ESD", IslandId.MCU_LINEBUF, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsD2", "TVS_5V", "TRS P2 DATA ESD", IslandId.MCU_LINEBUF, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("Rdata1", "R_47", "TRS P1 DATA series", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rdata2", "R_47", "TRS P2 DATA series", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rpu1", "R_4K7", "pad DATA pull-up (MCU side)", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    # Clock edge damping (docs/passive_rf_etc.md)
    BomEntry("Rphi", "R_33", "PHI2 series damp at Y1", IslandId.POWER_CLK, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rdot", "R_33", "DOT_CLK series damp at Y2", IslandId.BEAM, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    # Cart edge ESD: series 33Ω on D/OE/WE/I2C; TVS on those + all address (no series on A — timing)
    *[
        BomEntry(f"Rcd{i}", "R_33", f"cart D{i} series", IslandId.CART_SOCKET, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None)
        for i in range(8)
    ],
    BomEntry("Rcoe", "R_33", "cart OE# series", IslandId.CART_SOCKET, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rcwe", "R_33", "cart WE# series", IslandId.CART_SOCKET, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rcsda", "R_33", "cart SDA series", IslandId.CART_SOCKET, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    BomEntry("Rcscl", "R_33", "cart SCL series", IslandId.CART_SOCKET, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None),
    *[
        BomEntry(f"TvsCd{i}", "TVS_5V", f"cart D{i} ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True)
        for i in range(8)
    ],
    BomEntry("TvsOe", "TVS_5V", "cart OE# ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsWe", "TVS_5V", "cart WE# ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsSda", "TVS_5V", "cart SDA ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    BomEntry("TvsScl", "TVS_5V", "cart SCL ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True),
    *[
        BomEntry(f"TvsCa{i}", "TVS_5V", f"cart A{i} ESD", IslandId.CART_SOCKET, 2, _TVS, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True)
        for i in range(19)
    ],
    # Arcade headers: 47Ω series on each bitfield line (docs/controllers.md). BRINGUP wires direct.
    *[
        BomEntry(f"Rarc1_{i}", "R_47", f"arcade P1 bit {i} series", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True)
        for i in range(1, 9)
    ],
    *[
        BomEntry(f"Rarc2_{i}", "R_47", f"arcade P2 bit {i} series", IslandId.MCU_LINEBUF, 2, _R0603, in_ic_count=False, vcc_pin=None, gnd_pin=None, bringup_omit=True)
        for i in range(1, 9)
    ],
    # Sim / bench only
    BomEntry("U4", "PRG_ROM", "breadboard PRG fallback", IslandId.CPU, 28, _DIP28, in_ic_count=False, sim_only=True, vcc_pin=None, gnd_pin=None),
    BomEntry("SCR1", "SCREEN_SINK", "LCD sim sink", IslandId.VIDEO, 0, "", in_ic_count=False, sim_only=True, vcc_pin=None, gnd_pin=None),
]

BOM_BY_REFDES: Dict[str, BomEntry] = {
    e.refdes: e for e in BOM if e.board == BoardId.MOBO
}

# System silicon goal (mobo + cart).
SYSTEM_IC_COUNT = 23
IC_COUNT_TARGET = 21  # motherboard silicon only (U40/U50 live on cart)
CART_IC_COUNT = 2


def entries_for_board(board: BoardId, *, include_sim_only: bool = False) -> List[BomEntry]:
    profile = get_passive_profile()
    out: List[BomEntry] = []
    for e in BOM:
        if e.board != board:
            continue
        if e.sim_only and not include_sim_only:
            continue
        if profile == PassiveProfile.BRINGUP and e.bringup_omit:
            continue
        out.append(e)
    return out


def include_esd_tvs() -> bool:
    return get_passive_profile() == PassiveProfile.FULL


def include_arcade_series() -> bool:
    return get_passive_profile() == PassiveProfile.FULL


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
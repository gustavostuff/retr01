"""
Physical DIP pin numbers for Retr01 parts.

Authority for stock parts: KiCad 10 symbols under /usr/share/kicad/symbols/
(see kicad_pin_extract.json). HC573/HC157 use 74LS* symbols (identical DIP pinout;
KiCad has no discrete 74HC573/157). Custom silicon (W65C02S, ATF22V10, oscillators,
connectors) uses datasheet / hw/md pin numbers.

Manifest and connect.py address pins by these number strings ("1".."40").
"""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# KiCad 74xx (74LS573 / 74LS157 / 74HC245 / 74HC14)
# ---------------------------------------------------------------------------

# 74LS573 / SN74HC573 — KiCad names: OE, D0-D7, Load, Q0-Q7
HC573_OE = "1"
HC573_D = ("2", "3", "4", "5", "6", "7", "8", "9")  # D0..D7
HC573_GND = "10"
HC573_LE = "11"  # KiCad "Load"
HC573_Q = ("19", "18", "17", "16", "15", "14", "13", "12")  # Q0..Q7
HC573_VCC = "20"

# 74LS157 / SN74HC157 — S=select, I0*=A, I1*=B, Z*=Y, E=enable(~G)
HC157_S = "1"
HC157_I0 = ("2", "5", "11", "14")  # a,b,c,d  (= A inputs)
HC157_I1 = ("3", "6", "10", "13")  # a,b,c,d  (= B inputs)
HC157_Z = ("4", "7", "9", "12")  # a,b,c,d   (= Y outputs)
HC157_GND = "8"
HC157_E = "15"  # active-low enable (G#)
HC157_VCC = "16"

# 74HC245 — A->B=DIR, CE=~OE; B0 is pin 18
HC245_DIR = "1"
HC245_A = ("2", "3", "4", "5", "6", "7", "8", "9")  # A0..A7
HC245_GND = "10"
HC245_B = ("18", "17", "16", "15", "14", "13", "12", "11")  # B0..B7
HC245_OE = "19"  # KiCad "CE"
HC245_VCC = "20"

# 74HC14 — KiCad names are pin numbers for inverters
HC14_1A, HC14_1Y = "1", "2"
HC14_2A, HC14_2Y = "3", "4"
HC14_3A, HC14_3Y = "5", "6"
HC14_GND = "7"
HC14_4Y, HC14_4A = "8", "9"
HC14_5Y, HC14_5A = "10", "11"
HC14_6Y, HC14_6A = "12", "13"
HC14_VCC = "14"

# ---------------------------------------------------------------------------
# Memory — KM62256CLP (AS6C62256 JEDEC twin), 27C256, SST39SF040, 24LC64
# ---------------------------------------------------------------------------

# KM62256CLP / AS6C62256 DIP-28
SRAM_A = {
    0: "10",
    1: "9",
    2: "8",
    3: "7",
    4: "6",
    5: "5",
    6: "4",
    7: "3",
    8: "25",
    9: "24",
    10: "21",
    11: "23",
    12: "2",
    13: "26",
    14: "1",
}
SRAM_Q = ("11", "12", "13", "15", "16", "17", "18", "19")  # Q0..Q7
SRAM_GND = "14"
SRAM_CE = "20"  # ~{CS}
SRAM_OE = "22"
SRAM_WE = "27"
SRAM_VCC = "28"

# 27C256 / AT27C256R DIP-28 (KiCad Memory_EPROM:27C256)
PROM_A = {
    0: "10",
    1: "9",
    2: "8",
    3: "7",
    4: "6",
    5: "5",
    6: "4",
    7: "3",
    8: "25",
    9: "24",
    10: "21",
    11: "23",
    12: "2",
    13: "26",
    14: "27",
}
PROM_D = ("11", "12", "13", "15", "16", "17", "18", "19")  # D0..D7
PROM_VPP = "1"
PROM_GND = "14"
PROM_CE = "20"
PROM_OE = "22"
PROM_VCC = "28"

# SST39SF040 DIP-32 (KiCad Memory_Flash)
FLASH_A = {
    0: "12",
    1: "11",
    2: "10",
    3: "9",
    4: "8",
    5: "7",
    6: "6",
    7: "5",
    8: "27",
    9: "26",
    10: "23",
    11: "25",
    12: "4",
    13: "28",
    14: "29",
    15: "3",
    16: "2",
    17: "30",
    18: "1",
}
FLASH_D = ("13", "14", "15", "17", "18", "19", "20", "21")  # D0..D7
FLASH_GND = "16"
FLASH_CE = "22"
FLASH_OE = "24"
FLASH_WE = "31"  # KiCad "PGM"
FLASH_VCC = "32"

# 24LC64 / 24C64
EE_A0, EE_A1, EE_A2 = "1", "2", "3"
EE_GND = "4"
EE_SDA, EE_SCL = "5", "6"
EE_WP = "7"
EE_VCC = "8"

# ---------------------------------------------------------------------------
# W65C02S DIP-40 (hw/md/W65C02S.md) — no KiCad stock symbol
# ---------------------------------------------------------------------------

CPU_VPB, CPU_RDY, CPU_PHI1O, CPU_IRQB = "1", "2", "3", "4"
CPU_MLB, CPU_NMIB, CPU_SYNC, CPU_VDD = "5", "6", "7", "8"
CPU_A = {i: str(9 + i) for i in range(12)}  # A0..A11 = 9..20
CPU_A.update({12: "22", 13: "23", 14: "24", 15: "25"})
CPU_VSS = "21"
CPU_D = {0: "33", 1: "32", 2: "31", 3: "30", 4: "29", 5: "28", 6: "27", 7: "26"}
CPU_RWB, CPU_NC, CPU_BE, CPU_PHI2 = "34", "35", "36", "37"
CPU_SOB, CPU_PHI2O, CPU_RESB = "38", "39", "40"

# ---------------------------------------------------------------------------
# ATF22V10 DIP-24 (hw/md) — Retr01 function → physical pin (JEDEC later)
# ---------------------------------------------------------------------------

PLD_CLK, PLD_GND, PLD_VCC = "1", "12", "24"

# UPLDA decode: IN pins 2-11,13 ; I/O 14-23
UPLDA_A = {i: str(2 + i) for i in range(8)}  # A0-A7 on 2-9
UPLDA_BE, UPLDA_RWB, UPLDA_FE = "10", "11", "13"
UPLDA_SEL = {
    "00": "14", "02": "15", "03": "16", "04": "17", "05": "18",
    "08": "19", "90": "20", "91": "21", "92": "22",
    "10": "23", "11": "23", "06": "14", "07": "15", "12": "23", "93": "22",
}

# UPLDB VRAM glue + HC245/cart bus enables + scroll X register (HC573-zero).
# Never map signals onto PLD_GND (12) or PLD_VCC (24): that shorts the rail into a
# signal net and KiCad loses the literal name "GND" (merged net becomes e.g. PPU_VA10).
UPLDB_LD = {i: str(2 + i) for i in range(8)}  # CPU D load data (scroll X / FE1x)
UPLDB_LE_FE02 = "23"  # scroll X register clock enable (was SEL11 share. FE11 stays pin 22)
UPLDB_CPU_VA = {i: str(2 + i) for i in range(10)}  # bits 0-9 -> pins 2-11
UPLDB_CPU_VA.update({10: "13", 11: "14", 12: "15", 13: "16", 14: "17"})
UPLDB_VA = {i: str(2 + i) for i in range(10)}
UPLDB_VA.update({10: "13", 11: "14", 12: "15", 13: "16", 14: "17"})
UPLDB_DIR_CPU, UPLDB_OE_CPU = "14", "15"
UPLDB_DIR_VID, UPLDB_OE_VID = "16", "17"
UPLDB_DIR_CART, UPLDB_OE_CART = "18", "19"
UPLDB_CART_OE, UPLDB_CART_WE = "20", "21"
UPLDB_SEL10, UPLDB_SEL11 = "22", "22"  # FE10/FE11 share pin 22 after FE02 took 23

# UPLDX beam X + scroll Y register (HC573-zero).
UPLDX_DOT, UPLDX_RES, UPLDX_NMI = "1", "13", "14"
UPLDX_Y = {i: str(15 + i) for i in range(8)}
UPLDX_LD = {i: str(2 + i) for i in range(8)}  # CPU D -> scroll Y (pins 2-9)
UPLDX_LE_FE03 = "11"  # scroll Y load enable

# UPLDY beam Y: raster compare is internal (registered $FE04). No external Q hop.
UPLDY_P = {i: str(2 + i) for i in range(8)}
UPLDY_LD = {i: str(14 + i) for i in range(8)}  # CPU D -> raster Y regs (pins 14-21)
UPLDY_LE_FE04 = "23"  # raster Y load enable
UPLDY_EQ = "22"

# UPLDV compositor + CART_A14-A18 MAP high export (1284 GPIO budget exhausted).
UPLDV_IDX = {i: str(2 + i) for i in range(6)}
UPLDV_SCALE, UPLDV_EQ = "13", "14"
UPLDV_CART_A = {14 + i: str(15 + i) for i in range(5)}  # A14..A18 -> pins 15..19
UPLDV_LE_MAP = "20"  # load enable: SEL_FE91/FE92 for A14-A18
# MAP load data (5 bits). Pins 8-11,21 free of IDX/CART_A.
UPLDV_LD_MAP = {0: "8", 1: "9", 2: "10", 3: "11", 4: "21"}

# ---------------------------------------------------------------------------
# ATmega1284P-P / ATmega328P-P (KiCad MCU_Microchip_ATmega)
# ---------------------------------------------------------------------------

# Locked GPIO assignment for schematic (firmware must match)
M1284_P1 = ("40", "39", "38", "37", "36", "35", "34", "33")  # PA0..PA7 = RIGHT..START
M1284_P2 = ("1", "2", "3", "4", "5", "6", "7", "8")  # PB0..PB7
M1284_RESET, M1284_VCC, M1284_GND = "9", "10", "11"
M1284_SCL, M1284_SDA = "22", "23"  # PC0/PC1 TWI
M1284_PAD_DATA = "16"  # PD2
# Switchcraft 35RAPC2BVN4 (VN4 CD / 3BVN4 schematic family): Tip=4 Ring=2 Sleeve=1.
TRS_TIP, TRS_RING, TRS_SLEEVE = "4", "2", "1"
TRS_NC = ("3", "5")  # no switch contacts on 2BVN4; still plated for mechanical hold
M1284_HBLANK = "17"  # PD3
M1284_FE06, M1284_FE07 = "18", "19"  # PD4/PD5 (BG0 scroll). SEL_FE00 shares UPLDA pin with FE06.
# Soft $FExx (HC573-zero): palette + MAP seek strobes. Only PD0/PD1 free after DQ remap.
M1284_FE08 = "14"  # PD0 SEL_FE08 palette addr
M1284_FE90 = "15"  # PD1 SEL_FE90 MAP lo (FE91/FE92 also strobe via UPLDV_LE_MAP path)
M1284_XTAL2, M1284_XTAL1 = "12", "13"
# Prefer PC2-PC7 + PD6-PD7 for DQ to free PD2-5:
M1284_DQ = ("24", "25", "26", "27", "28", "29", "20", "21")  # PC2-7, PD6-7
M1284_LB_A = {i: str(1 + i) for i in range(8)}  # reuse PB for linebuf addr soft

M328_RESET, M328_VCC, M328_GND = "1", "7", "8"
M328_XTAL1, M328_XTAL2 = "9", "10"
M328_DQ = ("2", "3", "4", "5", "6", "11", "12", "13")  # PD0-4, PD5-7
M328_AUD = ("14", "15", "16", "17", "18", "19", "23", "24")  # PB0-5, PC0-1 = AUD0(LSB)..AUD7(MSB)

# AD725 RGB->NTSC/PAL encoder (wide SOIC-16 on DIP-16 via Proto Advantage PA0006).
AD725_STND, AD725_AGND, AD725_4FSC, AD725_APOS = "1", "2", "3", "4"
AD725_CE, AD725_RIN, AD725_GIN, AD725_BIN = "5", "6", "7", "8"
AD725_CRMA, AD725_COMP, AD725_LUMA, AD725_YTRAP = "9", "10", "11", "12"
AD725_DGND, AD725_DPOS, AD725_VSYNC, AD725_HSYNC = "13", "14", "15", "16"

# ---------------------------------------------------------------------------
# Passives / connectors / oscillators (KiCad Device R/C = pins 1,2)
# ---------------------------------------------------------------------------

R1, R2 = "1", "2"
C1, C2 = "1", "2"

# OSC cans — Abracon ACH half-size DIP-8 (KiCad Oscillator_DIP-8 pads 1/4/5/8)
OSC_OE, OSC_GND, OSC_OUT, OSC_VDD = "1", "4", "5", "8"

# Cart edge 36 — A1..A18 = 1..18, B1..B18 = 19..36
def cart_a(n: int) -> str:
    return str(n)  # A1=1 .. A18=18


def cart_b(n: int) -> str:
    return str(18 + n)  # B1=19 .. B18=36


# ---------------------------------------------------------------------------
# PIN_TEMPLATES: physical numbers only (guide requirement)
# ---------------------------------------------------------------------------

def _nums(n: int) -> List[str]:
    return [str(i) for i in range(1, n + 1)]


PIN_TEMPLATES: Dict[str, List[str]] = {
    "W65C02S": _nums(40),
    "AS6C62256": _nums(28),
    "SN74HC573": _nums(20),
    "SN74HC157": _nums(16),
    "SN74HC245": _nums(20),
    "ATF22V10": _nums(24),
    "ATmega1284P": _nums(40),
    "ATmega328P": _nums(28),
    "SST39SF040": _nums(32),
    "AT27C256R": _nums(28),
    "24C64": _nums(8),
    "SN74HC14": _nums(14),
    "OSC8M": ["1", "4", "5", "8"],  # Abracon ACH / KiCad Oscillator_DIP-8
    "OSC_DOT": ["1", "4", "5", "8"],
    "OSC_4FSC": ["1", "4", "5", "8"],
    "XTAL_20M": _nums(2),
    "XTAL_16M": _nums(2),
    "AD725": _nums(16),
    "CART_EDGE_36": _nums(36),
    "BARREL_5V": ["1", "2", "MP"],  # CUI PJ-063AH / KiCad Barrel_Jack_MountingPin
    "RGBS_HDR": _nums(5),
    # 35RAPC2BVN4: VN4 5-pad layout. Tip=4 Ring=2 Sleeve=1; 3+5 NC (no switch).
    "TRS_P1": _nums(5),
    "TRS_P2": _nums(5),
    "ARCADE_P1": _nums(10),
    "ARCADE_P2": _nums(10),
    "CAB_PWR_RST": _nums(4),
    "SCALE_SW": _nums(2),
    "AUDIO_OUT": _nums(2),  # RCJ: 1=center, 2=shell
    "COMPOSITE_OUT": _nums(2),
    "PPTC": _nums(2),
    "FERRITE": _nums(2),
    "SCHOTTKY": _nums(2),
    "C_BULK": _nums(2),
    "C_10U": _nums(2),
    "C_100N": _nums(2),
    "C_22P": _nums(2),
    "C_10U_AUD": _nums(2),
    "L_YTRAP": _nums(2),
    "TVS_5V": _nums(2),
    "R_4K7": _nums(2),
    "R_47": _nums(2),
    "R_33": _nums(2),
    "R_10K": _nums(2),
    "R_20K": _nums(2),
    "R_1K": _nums(2),
    "R_2K": _nums(2),
    "R_4K": _nums(2),
    "R_75": _nums(2),
    "R_R2R": _nums(2),
    # 0 ohm jumpers: bridge global rail <-> per-IC local VCC for Quilter bypass parent ID.
    "R_0": _nums(2),
}

# KiCad official pin name aliases (num -> kicad name) for documentation / Part aliases
KICAD_ALIASES: Dict[str, Dict[str, str]] = {
    "SN74HC573": {
        "1": "OE",
        "2": "D0",
        "3": "D1",
        "4": "D2",
        "5": "D3",
        "6": "D4",
        "7": "D5",
        "8": "D6",
        "9": "D7",
        "10": "GND",
        "11": "Load",
        "12": "Q7",
        "13": "Q6",
        "14": "Q5",
        "15": "Q4",
        "16": "Q3",
        "17": "Q2",
        "18": "Q1",
        "19": "Q0",
        "20": "VCC",
    },
    "SN74HC157": {
        "1": "S",
        "2": "I0a",
        "3": "I1a",
        "4": "Za",
        "5": "I0b",
        "6": "I1b",
        "7": "Zb",
        "8": "GND",
        "9": "Zc",
        "10": "I1c",
        "11": "I0c",
        "12": "Zd",
        "13": "I1d",
        "14": "I0d",
        "15": "E",
        "16": "VCC",
    },
    "SN74HC245": {
        "1": "A->B",
        "2": "A0",
        "3": "A1",
        "4": "A2",
        "5": "A3",
        "6": "A4",
        "7": "A5",
        "8": "A6",
        "9": "A7",
        "10": "GND",
        "11": "B7",
        "12": "B6",
        "13": "B5",
        "14": "B4",
        "15": "B3",
        "16": "B2",
        "17": "B1",
        "18": "B0",
        "19": "CE",
        "20": "VCC",
    },
    "SN74HC14": {**{str(i): str(i) for i in range(1, 15)}, "7": "GND", "14": "VCC"},
    "AS6C62256": {
        "1": "A14",
        "2": "A12",
        "3": "A7",
        "4": "A6",
        "5": "A5",
        "6": "A4",
        "7": "A3",
        "8": "A2",
        "9": "A1",
        "10": "A0",
        "11": "Q0",
        "12": "Q1",
        "13": "Q2",
        "14": "GND",
        "15": "Q3",
        "16": "Q4",
        "17": "Q5",
        "18": "Q6",
        "19": "Q7",
        "20": "~{CS}",
        "21": "A10",
        "22": "~{OE}",
        "23": "A11",
        "24": "A9",
        "25": "A8",
        "26": "A13",
        "27": "~{WE}",
        "28": "VCC",
    },
    "24C64": {"1": "A0", "2": "A1", "3": "A2", "4": "GND", "5": "SDA", "6": "SCL", "7": "WP", "8": "VCC"},
    "AT27C256R": {
        "1": "VPP",
        "2": "A12",
        "3": "A7",
        "4": "A6",
        "5": "A5",
        "6": "A4",
        "7": "A3",
        "8": "A2",
        "9": "A1",
        "10": "A0",
        "11": "D0",
        "12": "D1",
        "13": "D2",
        "14": "GND",
        "15": "D3",
        "16": "D4",
        "17": "D5",
        "18": "D6",
        "19": "D7",
        "20": "~{CE}",
        "21": "A10",
        "22": "~{OE}",
        "23": "A11",
        "24": "A9",
        "25": "A8",
        "26": "A13",
        "27": "A14",
        "28": "VCC",
    },
    "SST39SF040": {
        "1": "A18",
        "2": "A16",
        "3": "A15",
        "4": "A12",
        "5": "A7",
        "6": "A6",
        "7": "A5",
        "8": "A4",
        "9": "A3",
        "10": "A2",
        "11": "A1",
        "12": "A0",
        "13": "D0",
        "14": "D1",
        "15": "D2",
        "16": "GND",
        "17": "D3",
        "18": "D4",
        "19": "D5",
        "20": "D6",
        "21": "D7",
        "22": "CE",
        "23": "A10",
        "24": "OE",
        "25": "A11",
        "26": "A9",
        "27": "A8",
        "28": "A13",
        "29": "A14",
        "30": "A17",
        "31": "PGM",
        "32": "VCC",
    },
    "ATmega1284P": {
        "1": "PB0",
        "2": "PB1",
        "3": "PB2",
        "4": "PB3",
        "5": "PB4",
        "6": "PB5",
        "7": "PB6",
        "8": "PB7",
        "9": "~{RESET}",
        "10": "VCC",
        "11": "GND",
        "12": "XTAL2",
        "13": "XTAL1",
        "14": "PD0",
        "15": "PD1",
        "16": "PD2",
        "17": "PD3",
        "18": "PD4",
        "19": "PD5",
        "20": "PD6",
        "21": "PD7",
        "22": "PC0",
        "23": "PC1",
        "24": "PC2",
        "25": "PC3",
        "26": "PC4",
        "27": "PC5",
        "28": "PC6",
        "29": "PC7",
        "30": "AVCC",
        "31": "GND",
        "32": "AREF",
        "33": "PA7",
        "34": "PA6",
        "35": "PA5",
        "36": "PA4",
        "37": "PA3",
        "38": "PA2",
        "39": "PA1",
        "40": "PA0",
    },
    "ATmega328P": {
        "1": "~{RESET}/PC6",
        "2": "PD0",
        "3": "PD1",
        "4": "PD2",
        "5": "PD3",
        "6": "PD4",
        "7": "VCC",
        "8": "GND",
        "9": "XTAL1/PB6",
        "10": "XTAL2/PB7",
        "11": "PD5",
        "12": "PD6",
        "13": "PD7",
        "14": "PB0",
        "15": "PB1",
        "16": "PB2",
        "17": "PB3",
        "18": "PB4",
        "19": "PB5",
        "20": "AVCC",
        "21": "AREF",
        "22": "GND",
        "23": "PC0",
        "24": "PC1",
        "25": "PC2",
        "26": "PC3",
        "27": "PC4",
        "28": "PC5",
    },
    "R_R2R": {"1": "1", "2": "2"},
    "R_75": {"1": "1", "2": "2"},
    "R_47": {"1": "1", "2": "2"},
    "R_33": {"1": "1", "2": "2"},
    "R_4K7": {"1": "1", "2": "2"},
    "R_10K": {"1": "1", "2": "2"},
    "R_20K": {"1": "1", "2": "2"},
    "R_1K": {"1": "1", "2": "2"},
    "R_2K": {"1": "1", "2": "2"},
    "R_4K": {"1": "1", "2": "2"},
    "C_100N": {"1": "1", "2": "2"},
    "C_10U_AUD": {"1": "1", "2": "2"},
    "TVS_5V": {"1": "1", "2": "2"},
    "PPTC": {"1": "1", "2": "2"},
    "L_YTRAP": {"1": "1", "2": "2"},
    "OSC_4FSC": {"1": "Tri-State", "4": "GND", "5": "OUT", "8": "Vcc"},
    "OSC8M": {"1": "Tri-State", "4": "GND", "5": "OUT", "8": "Vcc"},
    "OSC_DOT": {"1": "Tri-State", "4": "GND", "5": "OUT", "8": "Vcc"},
    "XTAL_20M": {"1": "1", "2": "2"},
    "XTAL_16M": {"1": "1", "2": "2"},
    "C_22P": {"1": "1", "2": "2"},
    "AD725": {
        "1": "STND",
        "2": "AGND",
        "3": "4FSC",
        "4": "APOS",
        "5": "CE",
        "6": "RIN",
        "7": "GIN",
        "8": "BIN",
        "9": "CRMA",
        "10": "COMP",
        "11": "LUMA",
        "12": "YTRAP",
        "13": "DGND",
        "14": "DPOS",
        "15": "VSYNC",
        "16": "HSYNC",
    },
}


def power_pin_nums(mpn: str) -> Optional[Tuple[str, str]]:
    """Return (vcc_pin_num, gnd_pin_num) for decoupling."""
    table = {
        "W65C02S": (CPU_VDD, CPU_VSS),
        "AS6C62256": (SRAM_VCC, SRAM_GND),
        "SN74HC573": (HC573_VCC, HC573_GND),
        "SN74HC157": (HC157_VCC, HC157_GND),
        "SN74HC245": (HC245_VCC, HC245_GND),
        "ATF22V10": (PLD_VCC, PLD_GND),
        "ATmega1284P": (M1284_VCC, M1284_GND),
        "ATmega328P": (M328_VCC, M328_GND),
        "SST39SF040": (FLASH_VCC, FLASH_GND),
        "AT27C256R": (PROM_VCC, PROM_GND),
        "24C64": (EE_VCC, EE_GND),
        "SN74HC14": (HC14_VCC, HC14_GND),
        "OSC8M": (OSC_VDD, OSC_GND),
        "OSC_DOT": (OSC_VDD, OSC_GND),
        "OSC_4FSC": (OSC_VDD, OSC_GND),
    }
    return table.get(mpn)

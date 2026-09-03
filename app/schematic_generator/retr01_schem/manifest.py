"""
Declarative wiring manifest — pin keys are physical DIP numbers (pinmap.py).

KiCad stock pinouts: /usr/share/kicad/symbols (see kicad_pin_extract.json).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import List

from . import pinmap as P
from .bom import HC573_PORT_HEX, HC573_REFDES


@dataclass(frozen=True)
class Connection:
    net: str
    a_refdes: str
    a_pin: str
    b_refdes: str
    b_pin: str
    source: str


def _manifest_core() -> List[Connection]:
    m: List[Connection] = []

    m += [
        Connection("+5V", "Y1", P.OSC_VDD, "U1", P.CPU_VDD, "wire_power_clock_reset"),
        Connection("+5V", "Y1", P.OSC_VDD, "U2", P.HC14_VCC, "wire_power_clock_reset"),
        Connection("PHI2", "Y1", P.OSC_OUT, "U1", P.CPU_PHI2, "wire_power_clock_reset"),
        Connection("PHI2", "Y1", P.OSC_OUT, "U2", P.HC14_1A, "wire_power_clock_reset"),
        Connection("PHI2_BUF", "U2", P.HC14_1Y, "U1", P.CPU_PHI2, "docs/passive_rf_etc"),
        Connection("RESET_N", "U2", P.HC14_2A, "U1", P.CPU_RESB, "wire_power_clock_reset"),
        Connection("RESET_N", "U1", P.CPU_RESB, "UPLDX", P.UPLDX_RES, "wire_beam"),
    ]

    for i in range(15):
        m.append(Connection(f"CPU_A{i}", "U1", P.CPU_A[i], "U3", P.SRAM_A[i], "wire_memory"))
    for i in range(8):
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U3", P.SRAM_Q[i], "wire_memory"))

    for i in range(8):
        m.append(Connection(f"CPU_A_LO{i}", "U1", P.CPU_A[i], "UPLDA", P.UPLDA_A[i], "wire_decode"))
    m += [
        Connection("CPU_BE", "U1", P.CPU_BE, "UPLDA", P.UPLDA_BE, "wire_decode"),
        Connection("CPU_RWB", "U1", P.CPU_RWB, "UPLDA", P.UPLDA_RWB, "wire_decode"),
        Connection("IO_FE_N", "UPLDA", P.UPLDA_FE, "UPLDA", P.UPLDA_FE, "wire_decode"),
    ]

    for ref, port in zip(HC573_REFDES, HC573_PORT_HEX):
        for i in range(8):
            m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], ref, P.HC573_D[i], "docs/graphics.md HC573"))
        m.append(Connection(f"SEL_FE{port}", "UPLDA", P.UPLDA_SEL[port], ref, P.HC573_LE, "docs/graphics.md LE"))

    m += [
        Connection("SEL_FE06", "UPLDA", P.UPLDA_SEL["06"], "U1284", P.M1284_FE06, "docs/graphics.md FE06"),
        Connection("SEL_FE07", "UPLDA", P.UPLDA_SEL["07"], "U1284", P.M1284_FE07, "docs/graphics.md FE07"),
    ]

    m += [
        Connection("SEL_FE10", "UPLDA", P.UPLDA_SEL["10"], "UPLDB", P.UPLDB_SEL10, "docs/memory.md FE10"),
        Connection("SEL_FE11", "UPLDA", P.UPLDA_SEL["11"], "UPLDB", P.UPLDB_SEL11, "docs/memory.md FE11"),
    ]
    for i in range(8):
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "UPLDB", P.UPLDB_LD[i], "docs/memory.md FE1x"))

    m += [
        Connection("DOT_CLK", "Y2", P.OSC_OUT, "UPLDX", P.UPLDX_DOT, "wire_beam"),
        Connection("IRQ_N", "UPLDY", P.UPLDY_EQ, "U1", P.CPU_IRQB, "wire_beam"),
    ]
    for i in range(8):
        m.append(Connection(f"BEAM_Y{i}", "UPLDX", P.UPLDX_Y[i], "UPLDY", P.UPLDY_P[i], "wire_beam"))
        m.append(Connection(f"RASTER_Y{i}", "U5D", P.HC573_Q[i], "UPLDY", P.UPLDY_Q[i], "wire_beam U5D"))

    for mux in ("U7A", "U7B", "U7C"):
        m += [
            Connection("MUX_AB", "Y1", P.OSC_OUT, mux, P.HC157_S, "wire_vram"),
            Connection("MUX_G_N", "GND", "GND", mux, P.HC157_E, "wire_vram"),
        ]
    for mux, base in (("U7A", 0), ("U7B", 4), ("U7C", 8)):
        for i in range(4):
            bit = base + i
            m.append(Connection(f"CPU_VA{bit}", "UPLDB", P.UPLDB_CPU_VA[bit], mux, P.HC157_I0[i], "wire_vram"))
            m.append(Connection(f"PPU_VA{bit}", "UPLDB", P.UPLDB_VA[bit], mux, P.HC157_I1[i], "wire_vram"))
            m.append(Connection(f"VRAM_A{bit}", mux, P.HC157_Z[i], "U6", P.SRAM_A[bit], "wire_vram"))
    for bit in range(12, 15):
        m.append(Connection(f"VRAM_A{bit}", "UPLDB", P.UPLDB_CPU_VA[bit], "U6", P.SRAM_A[bit], "wire_vram"))
    for i in range(8):
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U6", P.SRAM_Q[i], "wire_vram data"))

    for mux in ("U7D", "U7E", "U7F"):
        m += [
            Connection("LB_MUX_AB", "U1284", P.M1284_HBLANK, mux, P.HC157_S, "wire_linebuf"),
            Connection("LB_MUX_G_N", "GND", "GND", mux, P.HC157_E, "wire_linebuf"),
        ]
    for mux, base in (("U7D", 0), ("U7E", 4), ("U7F", 8)):
        for i in range(4):
            bit = base + i
            # MCU LB addr from PB; beam side from UPLDX Y low bits
            mcu_pin = P.M1284_P2[bit % 8]
            m.append(Connection(f"MCU_LB_A{bit}", "U1284", mcu_pin, mux, P.HC157_I0[i], "wire_linebuf"))
            m.append(Connection(f"BEAM_LB_A{bit}", "UPLDX", P.UPLDX_Y[i], mux, P.HC157_I1[i], "wire_linebuf"))
            m.append(Connection(f"LB_A{bit}", mux, P.HC157_Z[i], "U41", P.SRAM_A[bit], "wire_linebuf"))
    for bit in range(12, 15):
        m.append(Connection(f"LB_A{bit}", "U1284", P.M1284_P2[bit % 8], "U41", P.SRAM_A[bit], "wire_linebuf"))

    for i in range(8):
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U20A", P.HC245_A[i], "HC245 CPU"))
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U20A", P.HC245_B[i], "HC245 CPU"))
    m += [
        Connection("DIR_CPU245", "UPLDB", P.UPLDB_DIR_CPU, "U20A", P.HC245_DIR, "HC245 DIR/OE"),
        Connection("OE_CPU245", "UPLDB", P.UPLDB_OE_CPU, "U20A", P.HC245_OE, "HC245 DIR/OE"),
        Connection("DIR_VID245", "UPLDB", P.UPLDB_DIR_VID, "U20B", P.HC245_DIR, "HC245 DIR/OE"),
        Connection("OE_VID245", "UPLDB", P.UPLDB_OE_VID, "U20B", P.HC245_OE, "HC245 DIR/OE"),
        Connection("DIR_CART245", "UPLDB", P.UPLDB_DIR_CART, "U20C", P.HC245_DIR, "HC245 DIR/OE"),
        Connection("OE_CART245", "UPLDB", P.UPLDB_OE_CART, "U20C", P.HC245_OE, "HC245 DIR/OE"),
    ]

    for i in range(8):
        m.append(Connection(f"CART_D{i}", "J36", P.cart_b(i + 4), "U20C", P.HC245_B[i], "docs/cart.md"))
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U20C", P.HC245_A[i], "cart 245"))
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U1284", P.M1284_DQ[i], "wire_io OAM"))
        m.append(Connection(f"CPU_D{i}", "U1", P.CPU_D[i], "U328", P.M328_DQ[i], "wire_io APU"))

    for i in range(6):
        m.append(Connection(f"PROM_A{i}", "UPLDV", P.UPLDV_IDX[i], "U24", P.PROM_A[i], "wire_video_prom"))

    m.append(Connection("NMI_N", "UPLDX", P.UPLDX_NMI, "U1", P.CPU_NMIB, "board_step"))
    return m


def _manifest_cart_edge() -> List[Connection]:
    """Motherboard side of the 36-pin edge — stops at J36 (no U40/U50)."""
    m: List[Connection] = []
    src = "docs/cart.md 36-pin"
    for pin in (P.cart_a(1), P.cart_a(18), P.cart_b(1)):
        m.append(Connection("GND", "J36", pin, "GND", "GND", src))
    for pin in (P.cart_a(2), P.cart_b(2)):
        m.append(Connection("+5V", "J36", pin, "+5V", "+5V", src))
    m += [
        Connection("I2C_SDA", "U1284", P.M1284_SDA, "J36", P.cart_a(3), src),
        Connection("I2C_SCL", "U1284", P.M1284_SCL, "J36", P.cart_b(3), src),
    ]
    for i in range(14):
        edge = P.cart_a(i + 4)
        m.append(Connection(f"CART_A{i}", "U1", P.CPU_A[i], "J36", edge, src))
    for i, edge_n in enumerate(range(13, 18)):
        bit = 14 + i
        latch = ("U5G", "U5H", "U5I")[min(i, 2)]
        m.append(Connection(f"CART_A{bit}", latch, P.HC573_Q[i % 8], "J36", P.cart_b(edge_n), src))
    # CART_D0..7: U20C B-side ↔ J36 already in _manifest_core.
    m += [
        Connection("CART_OE_N", "UPLDB", P.UPLDB_CART_OE, "J36", P.cart_b(12), src),
        Connection("CART_WE_N", "UPLDB", P.UPLDB_CART_WE, "J36", P.cart_b(18), src),
    ]
    return m

def _manifest_power_io() -> List[Connection]:
    m: List[Connection] = []
    src = "docs/passive_rf_etc.md"
    m += [
        Connection("VIN_RAW", "J1", "1", "D1", "1", src),
        Connection("GND", "J1", "2", "GND", "GND", src),
        Connection("VIN_PROT", "D1", "2", "F1", "1", src),
        Connection("VIN_FUSED", "F1", "2", "FB1", "1", src),
        Connection("+5V", "FB1", "2", "Cbulk", "1", src),
        Connection("GND", "Cbulk", "2", "GND", "GND", src),
        Connection("+5V_ANALOG", "FB2", "2", "Cva", "1", src),
        Connection("+5V", "FB2", "1", "+5V", "+5V", src),
        Connection("GND", "Cva", "2", "GND", "GND", src),
    ]
    for i in range(3):
        m.append(Connection(f"PROM_D{i}", "U24", P.PROM_D[i], f"RR{i}", "1", "docs R-2R red"))
        m.append(Connection("VIDEO_R", f"RR{i}", "2", "J2", "1", "docs R-2R red"))
    for i in range(3):
        m.append(Connection(f"PROM_D{i + 3}", "U24", P.PROM_D[i + 3], f"RG{i}", "1", "docs R-2R green"))
        m.append(Connection("VIDEO_G", f"RG{i}", "2", "J2", "2", "docs R-2R green"))
    for i in range(2):
        m.append(Connection(f"PROM_D{i + 6}", "U24", P.PROM_D[i + 6], f"RB{i}", "1", "docs R-2R blue"))
        m.append(Connection("VIDEO_B", f"RB{i}", "2", "J2", "3", "docs R-2R blue"))
    m += [
        Connection("VIDEO_R", "R75R", "1", "J2", "1", "docs 75 ohm"),
        Connection("GND", "R75R", "2", "GND", "GND", "docs 75 ohm"),
        Connection("VIDEO_G", "R75G", "1", "J2", "2", "docs 75 ohm"),
        Connection("GND", "R75G", "2", "GND", "GND", "docs 75 ohm"),
        Connection("VIDEO_B", "R75B", "1", "J2", "3", "docs 75 ohm"),
        Connection("GND", "R75B", "2", "GND", "GND", "docs 75 ohm"),
        Connection("CSYNC", "UPLDV", P.UPLDV_EQ, "J2", "4", "docs RGBS"),
        Connection("GND", "J2", "5", "GND", "GND", "docs RGBS"),
        Connection("+5V_ANALOG", "U24", P.PROM_VCC, "+5V_ANALOG", "+5V_ANALOG", src),
        Connection("GND", "U24", P.PROM_GND, "GND", "GND", src),
    ]
    ctrl = "docs/controllers.md"
    m += [
        Connection("PAD_VCC_P1", "F2", "2", "J3", "T", ctrl),
        Connection("+5V", "F2", "1", "+5V", "+5V", ctrl),
        Connection("PAD_DATA", "U1284", P.M1284_PAD_DATA, "J3", "R", ctrl),
        Connection("GND", "J3", "S", "GND", "GND", ctrl),
        Connection("PAD_VCC_P2", "F3", "2", "J4", "T", ctrl),
        Connection("+5V", "F3", "1", "+5V", "+5V", ctrl),
        Connection("PAD_DATA", "U1284", P.M1284_PAD_DATA, "J4", "R", ctrl),
        Connection("GND", "J4", "S", "GND", "GND", ctrl),
        Connection("+5V", "Rpu1", "1", "+5V", "+5V", ctrl),
        Connection("PAD_DATA", "Rpu1", "2", "U1284", P.M1284_PAD_DATA, ctrl),
    ]
    labels = ("RIGHT", "LEFT", "DOWN", "UP", "X", "Y", "COIN", "START")
    for i, _lab in enumerate(labels):
        m.append(Connection(f"P1_{labels[i]}", "J5", str(i + 1), "U1284", P.M1284_P1[i], f"{ctrl} J5"))
        m.append(Connection(f"P2_{labels[i]}", "J6", str(i + 1), "U1284", P.M1284_P2[i], f"{ctrl} J6"))
    m += [
        Connection("GND", "J5", "9", "GND", "GND", f"{ctrl} J5"),
        Connection("GND", "J5", "10", "GND", "GND", f"{ctrl} J5"),
        Connection("GND", "J6", "9", "GND", "GND", f"{ctrl} J6"),
        Connection("GND", "J6", "10", "GND", "GND", f"{ctrl} J6"),
        Connection("+5V", "J7", "1", "+5V", "+5V", f"{ctrl} J7"),
        Connection("GND", "J7", "2", "GND", "GND", f"{ctrl} J7"),
        Connection("RESET_N", "J7", "3", "U2", P.HC14_2A, f"{ctrl} J7"),
        Connection("GND", "J7", "4", "GND", "GND", f"{ctrl} J7"),
        Connection("SCALE_1X", "Rscale", "1", "UPLDV", P.UPLDV_SCALE, "SCALE"),
        Connection("GND", "Rscale", "2", "GND", "GND", "SCALE"),
        Connection("SCALE_1X", "SW1", "1", "UPLDV", P.UPLDV_SCALE, "SCALE"),
        Connection("+5V", "SW1", "2", "+5V", "+5V", "SCALE"),
    ]
    for i in range(8):
        m.append(Connection(f"AUD{i}", "U328", P.M328_AUD[i], f"RA{i}", "1", "docs APU R-2R"))
        m.append(Connection("AUDIO_SUM", f"RA{i}", "2", "Raud", "1", "docs APU R-2R"))
    m += [
        Connection("AUDIO_SUM", "Raud", "1", "Caud", "1", "docs APU"),
        Connection("AUDIO_OUT", "Caud", "2", "J8", "1", "docs APU RCA"),
        Connection("GND", "J8", "2", "GND", "GND", "docs APU RCA"),
        # Composite connector present; encoder / Y+C mix path still TBD.
        Connection("GND", "J9", "2", "GND", "GND", "docs composite RCA"),
    ]
    return m


def build_manifest() -> List[Connection]:
    return _manifest_core() + _manifest_cart_edge() + _manifest_power_io()


def manifest_gaps() -> List[str]:
    return [
        "TVS arrays on cart/TRS/arcade — layout",
        "Arcade series 47 ohm footprints — layout",
        "Retr01_Lib .kicad_sym for W65C02S / ATF22V10 / connectors (no stock KiCad symbols)",
        "UPLDA SEL_FE10/FE11 share pin 23 until JEDEC pin lock",
        "HC245 DIR/OE driven from UPLDB (UPLDA I/O budget); confirm in fuse map",
        "Composite encoder / Y+C mix into J9 — TBD (connector footprint present)",
        "J8/J9 footprints are stock BNC; swap to Cinch/RCA .kicad_mod when available",
    ]


def connections_for_refdes(manifest: List[Connection], refdes: str) -> List[Connection]:
    return [c for c in manifest if c.a_refdes == refdes or c.b_refdes == refdes]

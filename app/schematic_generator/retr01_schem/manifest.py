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
        Connection("+5V", "Y1", P.OSC_OE, "+5V", "+5V", "Abracon ACO OE high=run"),
        Connection("GND", "Y1", P.OSC_GND, "GND", "GND", "wire_power_clock_reset"),
        Connection("PHI2_SRC", "Y1", P.OSC_OUT, "Rphi", "1", "docs/passive_rf_etc clock damp"),
        Connection("PHI2", "Rphi", "2", "U1", P.CPU_PHI2, "wire_power_clock_reset"),
        Connection("PHI2", "Rphi", "2", "U2", P.HC14_1A, "wire_power_clock_reset"),
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
        Connection("+5V", "Y2", P.OSC_VDD, "+5V", "+5V", "wire_beam"),
        Connection("+5V", "Y2", P.OSC_OE, "+5V", "+5V", "Abracon ACO OE high=run"),
        Connection("GND", "Y2", P.OSC_GND, "GND", "GND", "wire_beam"),
        Connection("DOT_SRC", "Y2", P.OSC_OUT, "Rdot", "1", "docs/passive_rf_etc clock damp"),
        Connection("DOT_CLK", "Rdot", "2", "UPLDX", P.UPLDX_DOT, "wire_beam"),
        Connection("IRQ_N", "UPLDY", P.UPLDY_EQ, "U1", P.CPU_IRQB, "wire_beam"),
    ]
    for i in range(8):
        m.append(Connection(f"BEAM_Y{i}", "UPLDX", P.UPLDX_Y[i], "UPLDY", P.UPLDY_P[i], "wire_beam"))
        m.append(Connection(f"RASTER_Y{i}", "U5D", P.HC573_Q[i], "UPLDY", P.UPLDY_Q[i], "wire_beam U5D"))

    for mux in ("U7A", "U7B", "U7C"):
        m += [
            Connection("MUX_AB", "Rphi", "2", mux, P.HC157_S, "wire_vram"),
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
        # Cart data: MCU 245 -- 33Ω -- edge + TVS (docs/passive_rf_etc.md)
        m.append(Connection(f"CART_D{i}_MCU", "U20C", P.HC245_B[i], f"Rcd{i}", "1", "docs/cart.md ESD"))
        m.append(Connection(f"CART_D{i}", f"Rcd{i}", "2", "J36", P.cart_b(i + 4), "docs/cart.md ESD"))
        m.append(Connection(f"CART_D{i}", f"TvsCd{i}", "1", "J36", P.cart_b(i + 4), "docs/cart.md ESD"))
        m.append(Connection("GND", f"TvsCd{i}", "2", "GND", "GND", "docs/cart.md ESD"))
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
        Connection("I2C_SDA_MCU", "U1284", P.M1284_SDA, "Rcsda", "1", src),
        Connection("I2C_SDA", "Rcsda", "2", "J36", P.cart_a(3), src),
        Connection("I2C_SDA", "TvsSda", "1", "J36", P.cart_a(3), src),
        Connection("GND", "TvsSda", "2", "GND", "GND", src),
        Connection("I2C_SCL_MCU", "U1284", P.M1284_SCL, "Rcscl", "1", src),
        Connection("I2C_SCL", "Rcscl", "2", "J36", P.cart_b(3), src),
        Connection("I2C_SCL", "TvsScl", "1", "J36", P.cart_b(3), src),
        Connection("GND", "TvsScl", "2", "GND", "GND", src),
    ]
    for i in range(14):
        edge = P.cart_a(i + 4)
        m.append(Connection(f"CART_A{i}", "U1", P.CPU_A[i], "J36", edge, src))
        m.append(Connection(f"CART_A{i}", f"TvsCa{i}", "1", "J36", edge, src))
        m.append(Connection("GND", f"TvsCa{i}", "2", "GND", "GND", src))
    for i, edge_n in enumerate(range(13, 18)):
        bit = 14 + i
        latch = ("U5G", "U5H", "U5I")[min(i, 2)]
        m.append(Connection(f"CART_A{bit}", latch, P.HC573_Q[i % 8], "J36", P.cart_b(edge_n), src))
        m.append(Connection(f"CART_A{bit}", f"TvsCa{bit}", "1", "J36", P.cart_b(edge_n), src))
        m.append(Connection("GND", f"TvsCa{bit}", "2", "GND", "GND", src))
    # CART_D0..7: series+TVS already in _manifest_core.
    m += [
        Connection("CART_OE_MCU", "UPLDB", P.UPLDB_CART_OE, "Rcoe", "1", src),
        Connection("CART_OE_N", "Rcoe", "2", "J36", P.cart_b(12), src),
        Connection("CART_OE_N", "TvsOe", "1", "J36", P.cart_b(12), src),
        Connection("GND", "TvsOe", "2", "GND", "GND", src),
        Connection("CART_WE_MCU", "UPLDB", P.UPLDB_CART_WE, "Rcwe", "1", src),
        Connection("CART_WE_N", "Rcwe", "2", "J36", P.cart_b(18), src),
        Connection("CART_WE_N", "TvsWe", "1", "J36", P.cart_b(18), src),
        Connection("GND", "TvsWe", "2", "GND", "GND", src),
    ]
    return m

def _manifest_power_io() -> List[Connection]:
    m: List[Connection] = []
    src = "docs/passive_rf_etc.md"
    m += [
        Connection("VIN_RAW", "J1", "1", "D1", "1", src),
        Connection("GND", "J1", "2", "GND", "GND", src),
        Connection("GND", "J1", "MP", "GND", "GND", src),
        Connection("VIN_PROT", "D1", "2", "F1", "1", src),
        Connection("VIN_FUSED", "F1", "2", "FB1", "1", src),
        Connection("+5V", "FB1", "2", "Cbulk", "1", src),
        Connection("GND", "Cbulk", "2", "GND", "GND", src),
        Connection("+5V_ANALOG", "FB2", "2", "Cva", "1", src),
        Connection("+5V", "FB2", "1", "+5V", "+5V", src),
        Connection("GND", "Cva", "2", "GND", "GND", src),
    ]
    # Color PROM → binary-weighted guns. Packing (rr<<5)|(gg<<2)|bb (studio palette.c).
    # RR/RG: LSB=4k, mid=2k, MSB=1k. RB: LSB=2k, MSB=1k. Then 75Ω to GND → J2.
    for i, bit in enumerate((5, 6, 7)):  # red D5..D7 LSB→MSB
        m.append(Connection(f"PROM_D{bit}", "U24", P.PROM_D[bit], f"RR{i}", "1", "docs video DAC"))
        m.append(Connection("VIDEO_R", f"RR{i}", "2", "J2", "1", "docs video DAC"))
    for i, bit in enumerate((2, 3, 4)):  # green D2..D4
        m.append(Connection(f"PROM_D{bit}", "U24", P.PROM_D[bit], f"RG{i}", "1", "docs video DAC"))
        m.append(Connection("VIDEO_G", f"RG{i}", "2", "J2", "2", "docs video DAC"))
    for i, bit in enumerate((0, 1)):  # blue D0..D1
        m.append(Connection(f"PROM_D{bit}", "U24", P.PROM_D[bit], f"RB{i}", "1", "docs video DAC"))
        m.append(Connection("VIDEO_B", f"RB{i}", "2", "J2", "3", "docs video DAC"))
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
    # AD725 composite (NTSC): tap RGBS guns, CSYNC on HSYNC, VSYNC/STND/CE high.
    # COMP → 75Ω series → J9. S-Video (LUMA/CRMA) left unconnected. No S-Video jack.
    av = "docs/passive_rf_etc.md AD725"
    m += [
        Connection("VIDEO_R", "CinR", "1", "J2", "1", av),
        Connection("AD725_RIN", "CinR", "2", "U725", P.AD725_RIN, av),
        Connection("VIDEO_G", "CinG", "1", "J2", "2", av),
        Connection("AD725_GIN", "CinG", "2", "U725", P.AD725_GIN, av),
        Connection("VIDEO_B", "CinB", "1", "J2", "3", av),
        Connection("AD725_BIN", "CinB", "2", "U725", P.AD725_BIN, av),
        Connection("CSYNC", "UPLDV", P.UPLDV_EQ, "U725", P.AD725_HSYNC, av),
        Connection("+5V", "U725", P.AD725_VSYNC, "+5V", "+5V", av),
        Connection("+5V", "U725", P.AD725_STND, "+5V", "+5V", av),
        Connection("+5V", "U725", P.AD725_CE, "+5V", "+5V", av),
        Connection("+5V_ANALOG", "U725", P.AD725_APOS, "+5V_ANALOG", "+5V_ANALOG", av),
        Connection("+5V_ANALOG", "U725", P.AD725_DPOS, "+5V_ANALOG", "+5V_ANALOG", av),
        Connection("GND", "U725", P.AD725_AGND, "GND", "GND", av),
        Connection("GND", "U725", P.AD725_DGND, "GND", "GND", av),
        Connection("+5V_ANALOG", "Cd725a", "1", "U725", P.AD725_APOS, av),
        Connection("GND", "Cd725a", "2", "GND", "GND", av),
        Connection("+5V_ANALOG", "Cd725d", "1", "U725", P.AD725_DPOS, av),
        Connection("GND", "Cd725d", "2", "GND", "GND", av),
        Connection("FSC4", "Y3", P.OSC_OUT, "U725", P.AD725_4FSC, av),
        Connection("+5V", "Y3", P.OSC_VDD, "+5V", "+5V", av),
        Connection("+5V", "Y3", P.OSC_OE, "+5V", "+5V", av),
        Connection("GND", "Y3", P.OSC_GND, "GND", "GND", av),
        Connection("YTRAP", "U725", P.AD725_YTRAP, "Lytrap", "1", av),
        Connection("YTRAP_MID", "Lytrap", "2", "Cytrap", "1", av),
        Connection("GND", "Cytrap", "2", "GND", "GND", av),
        Connection("COMP_RAW", "U725", P.AD725_COMP, "R75C", "1", av),
        Connection("COMPOSITE_OUT", "R75C", "2", "J9", "1", av),
        Connection("GND", "J9", "2", "GND", "GND", av),
    ]
    # TRS: Tip=pad4 VCC (PPTC + 100nF + TVS), Ring=pad2 DATA (series R + TVS), Sleeve=pad1 GND.
    # Pads 3+5 NC on 35RAPC2BVN4 (mechanical only). Rpu1 on MCU-side PAD_DATA.
    ctrl = "docs/controllers.md"
    passives = "docs/passive_rf_etc.md"
    m += [
        Connection("+5V", "F2", "1", "+5V", "+5V", ctrl),
        Connection("PAD_VCC_P1", "F2", "2", "J3", P.TRS_TIP, ctrl),
        Connection("PAD_VCC_P1", "Cpad1", "1", "J3", P.TRS_TIP, passives),
        Connection("GND", "Cpad1", "2", "GND", "GND", passives),
        Connection("PAD_VCC_P1", "TvsV1", "1", "J3", P.TRS_TIP, passives),
        Connection("GND", "TvsV1", "2", "GND", "GND", passives),
        Connection("GND", "J3", P.TRS_SLEEVE, "GND", "GND", ctrl),
        Connection("+5V", "F3", "1", "+5V", "+5V", ctrl),
        Connection("PAD_VCC_P2", "F3", "2", "J4", P.TRS_TIP, ctrl),
        Connection("PAD_VCC_P2", "Cpad2", "1", "J4", P.TRS_TIP, passives),
        Connection("GND", "Cpad2", "2", "GND", "GND", passives),
        Connection("PAD_VCC_P2", "TvsV2", "1", "J4", P.TRS_TIP, passives),
        Connection("GND", "TvsV2", "2", "GND", "GND", passives),
        Connection("GND", "J4", P.TRS_SLEEVE, "GND", "GND", ctrl),
        Connection("+5V", "Rpu1", "1", "+5V", "+5V", ctrl),
        Connection("PAD_DATA", "Rpu1", "2", "U1284", P.M1284_PAD_DATA, ctrl),
        Connection("PAD_DATA", "U1284", P.M1284_PAD_DATA, "Rdata1", "1", passives),
        Connection("PAD_DATA_P1", "Rdata1", "2", "J3", P.TRS_RING, passives),
        Connection("PAD_DATA_P1", "TvsD1", "1", "J3", P.TRS_RING, passives),
        Connection("GND", "TvsD1", "2", "GND", "GND", passives),
        Connection("PAD_DATA", "U1284", P.M1284_PAD_DATA, "Rdata2", "1", passives),
        Connection("PAD_DATA_P2", "Rdata2", "2", "J4", P.TRS_RING, passives),
        Connection("PAD_DATA_P2", "TvsD2", "1", "J4", P.TRS_RING, passives),
        Connection("GND", "TvsD2", "2", "GND", "GND", passives),
    ]
    labels = ("RIGHT", "LEFT", "DOWN", "UP", "X", "Y", "COIN", "START")
    for i, lab in enumerate(labels):
        n = i + 1
        m.append(Connection(f"P1_{lab}_J", "J5", str(n), f"Rarc1_{n}", "1", f"{ctrl} J5"))
        m.append(Connection(f"P1_{lab}", f"Rarc1_{n}", "2", "U1284", P.M1284_P1[i], f"{ctrl} J5"))
        m.append(Connection(f"P2_{lab}_J", "J6", str(n), f"Rarc2_{n}", "1", f"{ctrl} J6"))
        m.append(Connection(f"P2_{lab}", f"Rarc2_{n}", "2", "U1284", P.M1284_P2[i], f"{ctrl} J6"))
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
    # APU classic R-2R: AUD[i]--2R--node; LSB node--2R--GND; nodes linked by R; MSB→build-out.
    for i in range(8):
        m.append(Connection(f"AUD{i}", "U328", P.M328_AUD[i], f"Ra2r{i}", "1", "docs APU R-2R"))
    m += [
        Connection("ASUM0", "Ra2r0", "2", "Raterm", "1", "docs APU R-2R"),
        Connection("GND", "Raterm", "2", "GND", "GND", "docs APU R-2R"),
    ]
    for i in range(7):
        m.append(Connection(f"ASUM{i}", f"Rar{i}", "1", f"Ra2r{i}", "2", "docs APU R-2R"))
        next_net = "AUDIO_SUM" if i == 6 else f"ASUM{i + 1}"
        m.append(Connection(next_net, f"Rar{i}", "2", f"Ra2r{i + 1}", "2", "docs APU R-2R"))
    m += [
        Connection("AUDIO_SUM", "Raud", "1", "Caud", "1", "docs APU"),
        Connection("AUDIO_OUT", "Caud", "2", "J8", "1", "docs APU RCA"),
        Connection("GND", "J8", "2", "GND", "GND", "docs APU RCA"),
        # AVR crystals (HC-49/U + 22 pF)
        Connection("XTAL1284", "Y4", "1", "U1284", P.M1284_XTAL1, "docs AVR xtal"),
        Connection("XTAL1284b", "Y4", "2", "U1284", P.M1284_XTAL2, "docs AVR xtal"),
        Connection("XTAL1284", "Cxtal4a", "1", "U1284", P.M1284_XTAL1, "docs AVR xtal"),
        Connection("GND", "Cxtal4a", "2", "GND", "GND", "docs AVR xtal"),
        Connection("XTAL1284b", "Cxtal4b", "1", "U1284", P.M1284_XTAL2, "docs AVR xtal"),
        Connection("GND", "Cxtal4b", "2", "GND", "GND", "docs AVR xtal"),
        Connection("XTAL328", "Y5", "1", "U328", P.M328_XTAL1, "docs AVR xtal"),
        Connection("XTAL328b", "Y5", "2", "U328", P.M328_XTAL2, "docs AVR xtal"),
        Connection("XTAL328", "Cxtal5a", "1", "U328", P.M328_XTAL1, "docs AVR xtal"),
        Connection("GND", "Cxtal5a", "2", "GND", "GND", "docs AVR xtal"),
        Connection("XTAL328b", "Cxtal5b", "1", "U328", P.M328_XTAL2, "docs AVR xtal"),
        Connection("GND", "Cxtal5b", "2", "GND", "GND", "docs AVR xtal"),
    ]
    return m


def build_manifest() -> List[Connection]:
    return _manifest_core() + _manifest_cart_edge() + _manifest_power_io()


def manifest_gaps() -> List[str]:
    return [
        "PCB J3/J4 still CUI SJ1-3533NG horizontal - swap to Retr01_Lib:Jack_3.5mm_Switchcraft_35RAPC2BVN4_Vertical before fab",
        "Optional: replace PinSocket_2x18 with EDAC 395-036-559-212 manufacturer CAD",
        "Retr01_Lib .kicad_sym for W65C02S / ATF22V10 (netlist/Quilter OK without. Human sch later)",
        "U725 AD725: first spin fully THT - replace SOIC footprint with DIP-16 + SOIC-to-DIP adapter before fab",
        "AD725 / RGBS 0.7 Vpp bench tune on first spin",
    ]


def connections_for_refdes(manifest: List[Connection], refdes: str) -> List[Connection]:
    return [c for c in manifest if c.a_refdes == refdes or c.b_refdes == refdes]

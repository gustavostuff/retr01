"""BOM and manifest validation (no SKiDL required)."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from retr01_schem.bom import (
    BOM_BY_REFDES,
    BoardId,
    CART_IC_COUNT,
    HC157_REFDES,
    HC245_REFDES,
    HC573_PORT_HEX,
    HC573_REFDES,
    IC_COUNT_TARGET,
    PLD_REFDES,
    SYSTEM_IC_COUNT,
    silicon_ic_entries,
)
from retr01_schem.board import export_manifest_json, validate_bom_counts, validate_j36_contract
from retr01_schem.cart_manifest import build_cart_manifest, j36_pin_to_net
from retr01_schem.manifest import build_manifest, manifest_gaps


class TestBom(unittest.TestCase):
    def test_ic_count(self):
        self.assertEqual(len(silicon_ic_entries(BoardId.MOBO)), IC_COUNT_TARGET)
        self.assertEqual(len(silicon_ic_entries(BoardId.CART)), CART_IC_COUNT)
        self.assertEqual(len(silicon_ic_entries(None)), SYSTEM_IC_COUNT)

    def test_refdes_unique(self):
        self.assertEqual(len(BOM_BY_REFDES), len(set(BOM_BY_REFDES)))

    def test_cart_silicon_not_on_mobo(self):
        self.assertNotIn("U40", BOM_BY_REFDES)
        self.assertNotIn("U50", BOM_BY_REFDES)
        cart_refs = {e.refdes for e in silicon_ic_entries(BoardId.CART)}
        self.assertEqual(cart_refs, {"U40", "U50"})

    def test_expected_families(self):
        for ref in HC573_REFDES + HC157_REFDES + HC245_REFDES + PLD_REFDES:
            self.assertIn(ref, BOM_BY_REFDES)

    def test_cart_edge_and_connectors(self):
        for ref in ("J36", "J1", "J2", "J3", "J4", "J5", "J6", "J7", "J8", "J9"):
            self.assertIn(ref, BOM_BY_REFDES)

    def test_kicad_stock_footprints(self):
        self.assertIn("PinSocket_2x18", BOM_BY_REFDES["J36"].footprint)
        self.assertIn("35RAPC2BVN4", BOM_BY_REFDES["J3"].footprint)
        self.assertEqual(BOM_BY_REFDES["J3"].footprint, BOM_BY_REFDES["J4"].footprint)
        for ref in ("Cpad1", "Cpad2", "TvsV1", "TvsV2", "TvsD1", "TvsD2", "Rdata1", "Rdata2", "Rpu1", "F2", "F3"):
            self.assertIn(ref, BOM_BY_REFDES)
        self.assertIn("RCJ-01x", BOM_BY_REFDES["J8"].footprint)
        self.assertEqual(BOM_BY_REFDES["J8"].footprint, BOM_BY_REFDES["J9"].footprint)
        self.assertIn("Oscillator_DIP-14", BOM_BY_REFDES["Y1"].footprint)
        self.assertEqual(BOM_BY_REFDES["Y1"].footprint, BOM_BY_REFDES["Y2"].footprint)
        self.assertEqual(BOM_BY_REFDES["Y1"].footprint, BOM_BY_REFDES["Y3"].footprint)
        self.assertIn("PJ-063AH", BOM_BY_REFDES["J1"].footprint)
        self.assertIn("AD725ARZ", BOM_BY_REFDES["U725"].role)

    def test_arcade_pin_names(self):
        from retr01_schem.pinmap import M1284_P1, M1284_P2, PIN_TEMPLATES

        self.assertEqual(PIN_TEMPLATES["ARCADE_P1"][0], "1")
        self.assertEqual(PIN_TEMPLATES["SN74HC573"][-1], "20")
        self.assertEqual(M1284_P1[0], "40")  # PA0
        self.assertEqual(M1284_P2[0], "1")  # PB0

    def test_pins_are_physical_numbers(self):
        from retr01_schem.pinmap import HC573_D, HC573_LE, HC245_DIR, SRAM_A

        self.assertEqual(HC573_D[0], "2")
        self.assertEqual(HC573_LE, "11")
        self.assertEqual(HC245_DIR, "1")
        self.assertEqual(SRAM_A[0], "10")
        letter_ok = {"GND", "+5V", "MP"}
        for c in build_manifest()[:50]:
            if c.a_refdes not in ("GND", "+5V", "+5V_ANALOG"):
                self.assertTrue(
                    c.a_pin.isdigit() or c.a_pin in letter_ok,
                    f"non-numeric pin {c}",
                )
            if c.b_refdes not in ("GND", "+5V", "+5V_ANALOG", "VIN_RAW", "VIN_PROT", "VIN_FUSED"):
                self.assertTrue(
                    c.b_pin.isdigit() or c.b_pin in letter_ok,
                    f"bad pin {c}",
                )

    def test_trs_five_pad_pinout(self):
        from retr01_schem.pinmap import PIN_TEMPLATES, TRS_NC, TRS_RING, TRS_SLEEVE, TRS_TIP

        self.assertEqual(PIN_TEMPLATES["TRS_P1"], ["1", "2", "3", "4", "5"])
        self.assertEqual(BOM_BY_REFDES["J3"].dip_pins, 5)
        self.assertEqual((TRS_TIP, TRS_RING, TRS_SLEEVE), ("4", "2", "1"))
        m = build_manifest()
        j3 = [(c.net, c.a_pin if c.a_refdes == "J3" else c.b_pin) for c in m if "J3" in (c.a_refdes, c.b_refdes)]
        pins = {p for _, p in j3}
        self.assertIn(TRS_TIP, pins)
        self.assertIn(TRS_RING, pins)
        self.assertIn(TRS_SLEEVE, pins)
        self.assertTrue(set(TRS_NC).isdisjoint(pins))  # NC pads left unconnected
        tip_nets = {n for n, p in j3 if p == TRS_TIP}
        self.assertIn("PAD_VCC_P1", tip_nets)

    def test_hc573_silicon_map(self):
        self.assertEqual(HC573_PORT_HEX, ("00", "02", "03", "04", "05", "08", "90", "91", "92"))
        self.assertIn("FE00", BOM_BY_REFDES["U5A"].role)

    def test_validate_helper(self):
        self.assertEqual(validate_bom_counts(), [])


class TestManifest(unittest.TestCase):
    def test_manifest_non_empty(self):
        m = build_manifest()
        self.assertGreater(len(m), 200)

    def test_cpu_bus_wired(self):
        nets = {c.net for c in build_manifest()}
        self.assertTrue(any(n.startswith("CPU_A") for n in nets))
        self.assertTrue(any(n.startswith("CPU_D") for n in nets))

    def test_cart_edge_and_i2c(self):
        m = build_manifest()
        nets = {c.net for c in m}
        self.assertIn("I2C_SDA", nets)
        self.assertIn("I2C_SCL", nets)
        j36 = [c for c in m if c.a_refdes == "J36" or c.b_refdes == "J36"]
        self.assertGreaterEqual(len(j36), 36)
        refs = {c.a_refdes for c in m} | {c.b_refdes for c in m}
        self.assertNotIn("U40", refs)
        self.assertNotIn("U50", refs)

    def test_cart_manifest_has_flash_eeprom(self):
        m = build_cart_manifest()
        refs = {c.a_refdes for c in m} | {c.b_refdes for c in m}
        self.assertIn("U40", refs)
        self.assertIn("U50", refs)
        self.assertIn("J36", refs)

    def test_j36_contract(self):
        self.assertEqual(validate_j36_contract(), [])
        mobo = j36_pin_to_net(build_manifest())
        cart = j36_pin_to_net(build_cart_manifest())
        self.assertEqual(mobo, cart)
        self.assertEqual(len(mobo), 36)

    def test_hc245_dir_oe(self):
        nets = {c.net for c in build_manifest()}
        for n in ("DIR_CPU245", "OE_CPU245", "DIR_CART245", "OE_CART245"):
            self.assertIn(n, nets)

    def test_hc157_all_wired(self):
        refs = {c.a_refdes for c in build_manifest()} | {c.b_refdes for c in build_manifest()}
        for r in HC157_REFDES:
            self.assertIn(r, refs)

    def test_manifest_json_roundtrip(self):
        out = ROOT / "output" / "test_manifest.json"
        export_manifest_json(out)
        data = json.loads(out.read_text())
        self.assertIn("connections", data)
        self.assertIn("gaps", data)
        self.assertEqual(len(data["connections"]), len(build_manifest()))

    def test_closed_gaps_not_listed(self):
        gaps = "\n".join(manifest_gaps())
        self.assertNotIn("36-pin", gaps)
        self.assertNotIn("I2C", gaps)
        self.assertNotIn("Arcade header P1/P2 pin order", gaps)
        self.assertNotIn("SCALE DIP + pull", gaps)

    def test_arcade_and_scale_nets(self):
        nets = {c.net for c in build_manifest()}
        self.assertIn("P1_RIGHT", nets)
        self.assertIn("P2_START", nets)
        self.assertIn("SCALE_1X", nets)
        self.assertIn("AUDIO_SUM", nets)
        self.assertIn("COMPOSITE_OUT", nets)
        self.assertIn("FSC4", nets)
        refs = {c.a_refdes for c in build_manifest()} | {c.b_refdes for c in build_manifest()}
        self.assertIn("J7", refs)
        self.assertIn("SW1", refs)
        self.assertIn("J8", refs)
        self.assertIn("J9", refs)
        self.assertIn("U725", refs)
        self.assertIn("Y3", refs)
        self.assertIn("Ra2r0", refs)
        self.assertIn("Raterm", refs)
        self.assertIn("RR2", refs)  # red MSB 1k
        self.assertIn("Rcd0", refs)
        self.assertIn("TvsCa0", refs)
        self.assertIn("Rarc1_1", refs)
        self.assertIn("Rphi", refs)

    def test_video_prom_bit_mapping(self):
        """Studio packing (R<<5)|(G<<2)|B → D7..D5 red, D4..D2 green, D1..D0 blue."""
        m = build_manifest()
        red_nets = {c.net for c in m if {c.a_refdes, c.b_refdes} & {"RR0", "RR1", "RR2"} and c.net.startswith("PROM_D")}
        self.assertEqual(red_nets, {"PROM_D5", "PROM_D6", "PROM_D7"})
        green_nets = {c.net for c in m if {c.a_refdes, c.b_refdes} & {"RG0", "RG1", "RG2"} and c.net.startswith("PROM_D")}
        self.assertEqual(green_nets, {"PROM_D2", "PROM_D3", "PROM_D4"})
        blue_nets = {c.net for c in m if {c.a_refdes, c.b_refdes} & {"RB0", "RB1"} and c.net.startswith("PROM_D")}
        self.assertEqual(blue_nets, {"PROM_D0", "PROM_D1"})

    def test_ad725_outside_ic_count(self):
        self.assertIn("U725", BOM_BY_REFDES)
        self.assertFalse(BOM_BY_REFDES["U725"].in_ic_count)
        self.assertIn("SOIC-16", BOM_BY_REFDES["U725"].footprint)


if __name__ == "__main__":
    unittest.main()

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
        self.assertIn("PinHeader_2x18", BOM_BY_REFDES["J36"].footprint)
        self.assertIn("SJ1-3533NG", BOM_BY_REFDES["J3"].footprint)
        self.assertEqual(BOM_BY_REFDES["J3"].footprint, BOM_BY_REFDES["J4"].footprint)
        self.assertIn("BNC_", BOM_BY_REFDES["J8"].footprint)
        self.assertEqual(BOM_BY_REFDES["J8"].footprint, BOM_BY_REFDES["J9"].footprint)

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
        letter_ok = {"T", "R", "S", "GND", "+5V"}
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
        refs = {c.a_refdes for c in build_manifest()} | {c.b_refdes for c in build_manifest()}
        self.assertIn("J7", refs)
        self.assertIn("SW1", refs)
        self.assertIn("J8", refs)
        self.assertIn("J9", refs)


if __name__ == "__main__":
    unittest.main()

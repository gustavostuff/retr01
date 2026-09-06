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
    FE_HARD_PLD_HEX,
    FE_SOFT_1284_HEX,
    HC157_REFDES,
    HC245_REFDES,
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
        for ref in HC157_REFDES + HC245_REFDES + PLD_REFDES:
            self.assertIn(ref, BOM_BY_REFDES)
        for ref in ("U5A", "U5B", "U5C", "U5D", "U5E", "U5F", "U5G", "U5H", "U5I"):
            self.assertNotIn(ref, BOM_BY_REFDES)

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
        self.assertIn("Oscillator_DIP-8", BOM_BY_REFDES["Y1"].footprint)
        self.assertEqual(BOM_BY_REFDES["Y1"].footprint, BOM_BY_REFDES["Y2"].footprint)
        self.assertEqual(BOM_BY_REFDES["Y1"].footprint, BOM_BY_REFDES["Y3"].footprint)
        self.assertIn("ACH-", BOM_BY_REFDES["Y1"].role)
        self.assertIn("PJ-063AH", BOM_BY_REFDES["J1"].footprint)
        self.assertIn("AD725ARZ", BOM_BY_REFDES["U725"].role)
        self.assertIn("DIP-16", BOM_BY_REFDES["U725"].footprint)
        # KiCad 9+/10 vertical axials (standing) for board density.
        self.assertIn("P2.54mm_Vertical", BOM_BY_REFDES["Rphi"].footprint)
        self.assertIn("P2.54mm_Vertical", BOM_BY_REFDES["TvsV1"].footprint)
        self.assertIn("Vertical_Fastron_MECC", BOM_BY_REFDES["Lytrap"].footprint)
        self.assertTrue(BOM_BY_REFDES["TvsV1"].bringup_omit)
        self.assertTrue(BOM_BY_REFDES["Rarc1_1"].bringup_omit)

    def test_arcade_pin_names(self):
        from retr01_schem.pinmap import M1284_P1, M1284_P2, PIN_TEMPLATES

        self.assertEqual(PIN_TEMPLATES["ARCADE_P1"][0], "1")
        self.assertEqual(PIN_TEMPLATES["SN74HC245"][-1], "20")
        self.assertEqual(M1284_P1[0], "40")  # PA0
        self.assertEqual(M1284_P2[0], "1")  # PB0

    def test_pins_are_physical_numbers(self):
        from retr01_schem.pinmap import HC245_DIR, SRAM_A

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

    def test_fexx_ownership(self):
        self.assertEqual(FE_HARD_PLD_HEX, ("02", "03", "04"))
        self.assertEqual(FE_SOFT_1284_HEX, ("00", "05", "08", "90", "91", "92"))
        self.assertEqual(SYSTEM_IC_COUNT, 23)
        self.assertEqual(IC_COUNT_TARGET, 21)
        nets = {c.net for c in build_manifest()}
        self.assertIn("SEL_FE02", nets)
        self.assertIn("SEL_FE08", nets)
        refs = {c.a_refdes for c in build_manifest()} | {c.b_refdes for c in build_manifest()}
        self.assertNotIn("U5A", refs)
        # Cart high-A from UPLDV.
        cart_hi = [
            c
            for c in build_manifest()
            if c.net.startswith("CART_A1") and "J36" in (c.a_refdes, c.b_refdes)
        ]
        drivers = {c.a_refdes if c.b_refdes == "J36" else c.b_refdes for c in cart_hi}
        self.assertIn("UPLDV", drivers)

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
        self.assertIn("Rphi", refs)
        # Default BRINGUP profile: no TVS / arcade series on the netlist.
        self.assertNotIn("TvsCa0", refs)
        self.assertNotIn("Rarc1_1", refs)
        self.assertIn("J5", refs)

    def test_full_esd_profile_has_tvs_and_arcade_series(self):
        from retr01_schem.bom import PassiveProfile, set_passive_profile

        set_passive_profile(PassiveProfile.FULL)
        try:
            refs = {c.a_refdes for c in build_manifest()} | {c.b_refdes for c in build_manifest()}
            self.assertIn("TvsCa0", refs)
            self.assertIn("Rarc1_1", refs)
            from retr01_schem.bom import entries_for_board, BoardId

            populated = {e.refdes for e in entries_for_board(BoardId.MOBO)}
            self.assertIn("TvsCa0", populated)
            self.assertIn("Rarc1_1", populated)
        finally:
            set_passive_profile(PassiveProfile.BRINGUP)

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
        self.assertIn("DIP-16", BOM_BY_REFDES["U725"].footprint)


class TestDecouplingQuilter(unittest.TestCase):
    """Per-IC local VCC nets so Quilter can parent each CD* bypass cap."""

    @classmethod
    def setUpClass(cls):
        from retr01_schem.parts import skidl_available

        cls.skidl = skidl_available()

    def test_local_vcc_nets_exclusive(self):
        if not self.skidl:
            self.skipTest("skidl not installed")
        from skidl import reset

        from retr01_schem.board import build_board
        from retr01_schem.bom import silicon_ic_entries, BoardId
        from retr01_schem.pinmap import power_pin_nums

        reset()
        built = build_board(include_sim_only=False)
        parts = built["parts"]
        nets = built["nets"]
        entries = silicon_ic_entries(BoardId.MOBO)
        for n, entry in enumerate(entries, start=1):
            pins = power_pin_nums(entry.mpn)
            if pins is None:
                continue
            local_name = f"+5V_{entry.refdes}"
            self.assertIn(local_name, nets, msg=entry.refdes)
            self.assertIn(f"CD{n}", parts, msg=entry.refdes)
            self.assertIn(f"RD{n}", parts, msg=entry.refdes)
            local = nets[local_name]
            pin_refs = []
            for pin in local.pins:
                ref = getattr(getattr(pin, "part", None), "ref", None)
                if ref:
                    pin_refs.append(ref)
            # Local net: IC VCC + CD* + RD* only (no other silicon).
            self.assertIn(entry.refdes, pin_refs)
            self.assertIn(f"CD{n}", pin_refs)
            self.assertIn(f"RD{n}", pin_refs)
            others = [
                r
                for r in pin_refs
                if r not in (entry.refdes, f"CD{n}", f"RD{n}") and r.startswith("U")
            ]
            self.assertEqual(others, [], msg=f"{local_name} leaked to {others}")

    def test_u24_bridges_to_analog(self):
        if not self.skidl:
            self.skipTest("skidl not installed")
        from skidl import reset

        from retr01_schem.board import build_board

        reset()
        built = build_board(include_sim_only=False)
        parts = built["parts"]
        # Find which CD/RD index is U24.
        from retr01_schem.bom import silicon_ic_entries, BoardId
        from retr01_schem.pinmap import power_pin_nums

        idx = None
        for n, entry in enumerate(silicon_ic_entries(BoardId.MOBO), start=1):
            if entry.refdes == "U24" and power_pin_nums(entry.mpn):
                idx = n
                break
        self.assertIsNotNone(idx)
        bridge = parts[f"RD{idx}"]
        names = {str(getattr(n, "name", "") or "") for n in bridge["2"].nets}
        self.assertIn("+5V_ANALOG", names)

    def test_ad725_local_bypass_nets(self):
        if not self.skidl:
            self.skipTest("skidl not installed")
        from skidl import reset

        from retr01_schem.board import build_board

        reset()
        built = build_board(include_sim_only=False)
        nets = built["nets"]
        parts = built["parts"]
        self.assertIn("+5V_U725_APOS", nets)
        self.assertIn("+5V_U725_DPOS", nets)
        self.assertIn("RD725a", parts)
        self.assertIn("RD725d", parts)
        a_refs = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["+5V_U725_APOS"].pins
        }
        d_refs = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["+5V_U725_DPOS"].pins
        }
        self.assertEqual(a_refs, {"U725", "Cd725a", "RD725a"})
        self.assertEqual(d_refs, {"U725", "Cd725d", "RD725d"})
        ag = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["GND_U725_AGND"].pins
        }
        dg = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["GND_U725_DGND"].pins
        }
        self.assertEqual(ag, {"U725", "Cd725a", "RD725ag"})
        self.assertEqual(dg, {"U725", "Cd725d", "RD725dg"})

    def test_cbulk_cytrap_isolated(self):
        if not self.skidl:
            self.skipTest("skidl not installed")
        from skidl import reset

        from retr01_schem.board import build_board

        reset()
        built = build_board(include_sim_only=False)
        nets = built["nets"]
        parts = built["parts"]
        self.assertIn("+5V_BULK", nets)
        self.assertIn("YTRAP_RET", nets)
        self.assertIn("RDbulk", parts)
        self.assertIn("RDytrap", parts)
        bulk_refs = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["+5V_BULK"].pins
        }
        # No silicon IC on the bulk stub.
        self.assertTrue(bulk_refs <= {"Cbulk", "FB1", "RDbulk"})
        self.assertNotIn("U2", bulk_refs)
        ret_refs = {
            getattr(getattr(p, "part", None), "ref", None) for p in nets["YTRAP_RET"].pins
        }
        self.assertEqual(ret_refs, {"Cytrap", "RDytrap"})
        self.assertEqual(str(getattr(parts["Cbulk"], "value", "")), "220uF")


if __name__ == "__main__":
    unittest.main()

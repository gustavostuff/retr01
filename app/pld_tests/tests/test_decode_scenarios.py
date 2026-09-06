"""UPLDA decode scenarios: every real $FExx port + qualifies + shares."""

from __future__ import annotations

import unittest

from retr01_pld.decode import ALL_SEL, PORT_TO_SEL, REAL_PORTS, exclusive_sels, sel_active


class TestDecodeScenarios(unittest.TestCase):
    def test_every_real_port_asserts_exactly_one_sel(self) -> None:
        for port in REAL_PORTS:
            with self.subTest(port=f"${port:02X}"):
                active = sel_active(port)
                self.assertEqual(exclusive_sels(active), 1)
                self.assertTrue(active[PORT_TO_SEL[port]])

    def test_shared_pairs_same_sel_pin(self) -> None:
        shares = [
            (0x00, 0x06, "S00"),
            (0x02, 0x07, "S02"),
            (0x92, 0x93, "S92"),
        ]
        for a, b, pin in shares:
            with self.subTest(share=f"${a:02X}/${b:02X}"):
                self.assertEqual(PORT_TO_SEL[a], pin)
                self.assertEqual(PORT_TO_SEL[b], pin)
                self.assertEqual(sel_active(a)[pin], True)
                self.assertEqual(sel_active(b)[pin], True)

    def test_vram_triple_share_fe10_11_12(self) -> None:
        for port in (0x10, 0x11, 0x12):
            with self.subTest(port=f"${port:02X}"):
                self.assertEqual(PORT_TO_SEL[port], "S10")
                self.assertTrue(sel_active(port)["S10"])

    def test_read_cycle_no_sel(self) -> None:
        for port in REAL_PORTS:
            with self.subTest(port=f"${port:02X}"):
                active = sel_active(port, rwb=True)
                self.assertEqual(exclusive_sels(active), 0)

    def test_outside_fe_page_no_sel(self) -> None:
        active = sel_active(0x00, fe=False)
        self.assertEqual(exclusive_sels(active), 0)

    def test_be_low_no_sel(self) -> None:
        active = sel_active(0x04, be=False)
        self.assertEqual(exclusive_sels(active), 0)

    def test_unknown_port_no_sel(self) -> None:
        for port in range(256):
            if port in PORT_TO_SEL:
                continue
            active = sel_active(port)
            self.assertEqual(exclusive_sels(active), 0, f"spurious SEL for ${port:02X}")

    def test_hard_pld_ports_present(self) -> None:
        for port in (0x02, 0x03, 0x04):
            self.assertIn(port, PORT_TO_SEL)

    def test_soft_1284_ports_present(self) -> None:
        for port in (0x00, 0x05, 0x06, 0x07, 0x08, 0x90, 0x91, 0x92):
            self.assertIn(port, PORT_TO_SEL)

    def test_all_sel_names_covered(self) -> None:
        used = set(PORT_TO_SEL.values())
        self.assertEqual(used, set(ALL_SEL))


if __name__ == "__main__":
    unittest.main()

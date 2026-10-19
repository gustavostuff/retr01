"""Counter levels from the beam bench, with no serial port open."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import pins
import verify
import verify_beam_x
import verify_beam_y


class BeamXLevels(unittest.TestCase):
    def test_checkpoints_match_the_bench(self) -> None:
        expect = {
            0: (0, 0, 0),
            1: (1, 0, 0),
            2: (0, 1, 0),
            3: (1, 1, 0),
            4: (0, 0, 0),
            264: (0, 0, 1),
            288: (0, 0, 1),
            289: (1, 0, 0),
            341: (0, 0, 0),
        }
        for edges, (q0, q1, hsync) in expect.items():
            self.assertEqual(verify_beam_x.level(edges, "q0"), bool(q0))
            self.assertEqual(verify_beam_x.level(edges, "q1"), bool(q1))
            self.assertEqual(verify_beam_x.level(edges, "hsync"), bool(hsync))

    def test_virtual_run_passes(self) -> None:
        self.assertEqual(verify_beam_x.run_virtual(), "IC Beam X logic works ok")

    def test_log_rewrites_from_the_start(self) -> None:
        path = verify_beam_x.LOG
        verify_beam_x.run_virtual()
        path.write_text(path.read_text(encoding="utf-8") + "OLD\n", encoding="utf-8")
        verify_beam_x.run_virtual()
        text = path.read_text(encoding="utf-8")
        self.assertNotIn("OLD", text)
        self.assertTrue(text.startswith("Beam X\nreset\nQ0 pin 14\n"))
        self.assertIn("edges 0 count 0 Q0 0\n", text)
        self.assertIn("edges 341 count 0 Q0 0\n", text)
        self.assertIn("reset\nHSYNC pin 23\n", text)
        self.assertIn("edges 264 count 264 HSYNC 1\n", text)
        self.assertTrue(text.endswith("IC Beam X logic works ok\n"))


class BeamYLevels(unittest.TestCase):
    def test_hsync_rise_advances_the_line(self) -> None:
        self.assertEqual(verify_beam_y.y_count(263), 0)
        self.assertEqual(verify_beam_y.y_count(264), 1)
        self.assertEqual(verify_beam_y.y_count(264 + 341), 2)
        self.assertEqual(verify_beam_y.y_count(verify_beam_y.x_edges(262)), 0)

    def test_checkpoints_match_the_bench(self) -> None:
        expect = {
            0: (0, 0, 0),
            1: (1, 0, 0),
            2: (0, 1, 0),
            3: (1, 1, 0),
            243: (1, 1, 1),
            245: (1, 0, 1),
            246: (0, 1, 0),
            262: (0, 0, 0),
        }
        for rises, (q0, q1, vsync) in expect.items():
            edges = verify_beam_y.x_edges(rises)
            self.assertEqual(verify_beam_y.level(edges, "q0"), bool(q0))
            self.assertEqual(verify_beam_y.level(edges, "q1"), bool(q1))
            self.assertEqual(verify_beam_y.level(edges, "vsync"), bool(vsync))

    def test_virtual_run_passes(self) -> None:
        self.assertEqual(verify_beam_y.run_virtual(), "IC Beam Y logic works ok")

    def test_log_rewrites_from_the_start(self) -> None:
        path = verify_beam_y.LOG
        verify_beam_y.run_virtual()
        path.write_text(path.read_text(encoding="utf-8") + "OLD\n", encoding="utf-8")
        verify_beam_y.run_virtual()
        text = path.read_text(encoding="utf-8")
        self.assertNotIn("OLD", text)
        self.assertTrue(text.startswith("Beam Y\nreset\nQ0 pin 14\n"))
        self.assertIn("rises 0 count 0 Q0 0\n", text)
        self.assertIn("rises 262 count 0 Q0 0\n", text)
        self.assertIn("reset\nVSYNC pin 23\n", text)
        self.assertIn("rises 243 count 243 VSYNC 1\n", text)
        self.assertTrue(text.endswith("IC Beam Y logic works ok\n"))


class ClockHz(unittest.TestCase):
    def test_ten_hertz_holds_each_level_for_50_ms(self) -> None:
        self.assertAlmostEqual(pins.half_period(10), 0.05)

    def test_hz_flag_sets_the_clock(self) -> None:
        virtual, clock, hz = verify.parse_args(["--hz", "25"])
        self.assertFalse(virtual)
        self.assertFalse(clock)
        self.assertEqual(hz, 25)
        virtual, clock, hz = verify.parse_args(["--virtual", "--hz=4"])
        self.assertTrue(virtual)
        self.assertFalse(clock)
        self.assertEqual(hz, 4)
        virtual, clock, hz = verify.parse_args(["--clock", "--hz", "10"])
        self.assertFalse(virtual)
        self.assertTrue(clock)
        self.assertEqual(hz, 10)

    def test_hz_flag_rejects_zero(self) -> None:
        with self.assertRaises(ValueError):
            verify.parse_args(["--hz", "0"])


if __name__ == "__main__":
    unittest.main()

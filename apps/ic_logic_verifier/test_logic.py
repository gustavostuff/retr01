"""Counter levels from the beam bench, with no serial port open."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

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
        self.assertEqual(verify_beam_x.run_virtual(), "count ok")


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
        self.assertEqual(verify_beam_y.run_virtual(), "count ok")


if __name__ == "__main__":
    unittest.main()

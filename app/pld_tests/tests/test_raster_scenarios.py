"""Raster compare scenarios: equal, single-bit NE, full 8-bit sweep samples."""

from __future__ import annotations

import unittest

from retr01_pld.raster_eq import (
    eq_active_low_when_equal,
    eq_high_when_ne,
    ne_hi,
    ne_lo,
)


class TestRasterEqScenarios(unittest.TestCase):
    def test_equal_all_zeros_and_all_ones(self) -> None:
        for v in (0x00, 0xFF, 0x5A, 0xA5):
            with self.subTest(v=v):
                self.assertFalse(eq_high_when_ne(v, v))
                self.assertTrue(eq_active_low_when_equal(v, v))

    def test_each_bit_flip_causes_ne(self) -> None:
        base = 0x00
        for bit in range(8):
            with self.subTest(bit=bit):
                p = base
                q = 1 << bit
                self.assertTrue(eq_high_when_ne(p, q))
                self.assertFalse(eq_active_low_when_equal(p, q))
                if bit < 4:
                    self.assertTrue(ne_lo(p, q))
                    self.assertFalse(ne_hi(p, q))
                else:
                    self.assertFalse(ne_lo(p, q))
                    self.assertTrue(ne_hi(p, q))

    def test_cascade_both_halves(self) -> None:
        p, q = 0x01, 0x10
        self.assertTrue(ne_lo(p, q))
        self.assertTrue(ne_hi(p, q))
        self.assertTrue(eq_high_when_ne(p, q))

    def test_full_byte_space_sample(self) -> None:
        # Exhaustive 256x256 is fine and mirrors real compare coverage.
        for p in range(256):
            for q in range(256):
                ne = (p != q)
                self.assertEqual(eq_high_when_ne(p, q), ne)
                self.assertEqual(eq_active_low_when_equal(p, q), not ne)


if __name__ == "__main__":
    unittest.main()

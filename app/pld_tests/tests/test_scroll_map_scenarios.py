"""Scroll X/Y and MAP register load / hold scenarios."""

from __future__ import annotations

import unittest

from retr01_pld.registers import CeReg


class TestScrollAndMapRegisters(unittest.TestCase):
    def test_scroll_x_load_hold_and_clamp_bits(self) -> None:
        sx = CeReg(8)
        self.assertEqual(sx.clock(0x00, True), 0x00)
        self.assertEqual(sx.clock(0x7F, True), 0x7F)  # max BG1 scroll X
        self.assertEqual(sx.clock(0x55, False), 0x7F)  # hold
        self.assertEqual(sx.clock(0xFF, True), 0xFF)
        self.assertEqual(sx.clock(0x00, False), 0xFF)

    def test_scroll_y_load_hold(self) -> None:
        sy = CeReg(8)
        self.assertEqual(sy.clock(0x77, True), 0x77)  # max BG1 scroll Y 119
        self.assertEqual(sy.clock(0x00, False), 0x77)
        self.assertEqual(sy.clock(0x00, True), 0x00)

    def test_each_scroll_bit_independent(self) -> None:
        for bit in range(8):
            with self.subTest(bit=bit):
                r = CeReg(8)
                v = 1 << bit
                self.assertEqual(r.clock(v, True), v)
                self.assertEqual(r.clock(0, False), v)
                self.assertEqual(r.clock(0, True), 0)

    def test_map_a14_18_five_bits(self) -> None:
        cart = CeReg(5)
        # After FE91/FE92 soft seek, UPLDV captures D[4:0] on LE_MAP.
        self.assertEqual(cart.clock(0x1F, True), 0x1F)
        self.assertEqual(cart.clock(0x00, False), 0x1F)
        for bit in range(5):
            with self.subTest(bit=bit):
                cart.clock(0, True)
                v = 1 << bit
                self.assertEqual(cart.clock(v, True), v)

    def test_raster_q_register(self) -> None:
        q = CeReg(8)
        self.assertEqual(q.clock(0x40, True), 0x40)
        self.assertEqual(q.clock(0x12, False), 0x40)


if __name__ == "__main__":
    unittest.main()

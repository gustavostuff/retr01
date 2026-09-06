"""Galette fit: expect_fit must pack, expect_fail must overflow."""

from __future__ import annotations

import unittest

from retr01_pld import EXPECT_FAIL, EXPECT_FIT
from retr01_pld.galette_runner import fit_pld


class TestGaletteFit(unittest.TestCase):
    def test_all_expect_fit_pack(self) -> None:
        plds = sorted(EXPECT_FIT.glob("*.pld"))
        self.assertTrue(plds, "missing expect_fit/*.pld")
        for pld in plds:
            with self.subTest(pld=pld.name):
                result = fit_pld(pld)
                self.assertTrue(
                    result.ok,
                    f"{pld.name} failed fit rc={result.returncode}\n"
                    f"{result.stdout}\n{result.stderr}",
                )

    def test_expect_fail_overflow(self) -> None:
        plds = sorted(EXPECT_FAIL.glob("*.pld"))
        self.assertTrue(plds, "missing expect_fail/*.pld")
        for pld in plds:
            with self.subTest(pld=pld.name):
                result = fit_pld(pld)
                self.assertFalse(result.ok, f"{pld.name} unexpectedly fitted")
                self.assertTrue(
                    result.too_many_products,
                    f"{pld.name} failed for wrong reason:\n{result.stdout}\n{result.stderr}",
                )


if __name__ == "__main__":
    unittest.main()

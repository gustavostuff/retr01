"""Retr01 ATF22V10 / GAL22V10 fit + behavioral scenario tests."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EQUATIONS = ROOT / "equations"
EXPECT_FIT = EQUATIONS / "expect_fit"
EXPECT_FAIL = EQUATIONS / "expect_fail"

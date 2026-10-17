"""Virtual read-back for the chips that already have a burn image."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

sys_path_note = Path(__file__).resolve().parents[1] / "flasher_tool"


def _load(name: str):
    import sys

    folder = Path(__file__).resolve().parent
    sys.path.insert(0, str(sys_path_note))
    spec = importlib.util.spec_from_file_location(f"firmware_{name}", folder / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


verify_beam_x = _load("verify_beam_x")
verify_beam_y = _load("verify_beam_y")
verify_color_prom = _load("verify_color_prom")

from burn import resolve_image
from virtual import VirtualBench


class FirmwareReadBack(unittest.TestCase):
    def test_beam_images_match_after_a_virtual_burn(self) -> None:
        bench = VirtualBench()
        for chip in (verify_beam_x, verify_beam_y, verify_color_prom):
            bench.program(chip.TARGET, resolve_image(chip.TARGET))
            self.assertEqual(chip.run(bench), "match")

    def test_a_fresh_chip_mismatches(self) -> None:
        bench = VirtualBench()
        self.assertIn("mismatch", verify_beam_x.run(bench))


if __name__ == "__main__":
    unittest.main()

"""Tier A, B, and C burn rows for the flasher menu."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

BANNER = """\
Retr01 flasher

Programmers:
  XGecu T48 or TL866II Plus    color PROM, Beam X, Beam Y, Compositor
  HiLetgo CH340 SerialUPDI     MCU-S1 (AVR pin 19)

Up/Down select, Enter flash, q quit
"""

CH340_VENDOR = "1a86"


@dataclass(frozen=True)
class Target:
    name: str
    chip: str
    kind: str
    image: str
    device: str
    generate: str | None = None


TARGETS: tuple[Target, ...] = (
    Target(
        name="Color PROM",
        chip="AT27C256R",
        kind="prom",
        image="apps/flasher_tool/firmware/a/at27c256r_kit.bin",
        device="AT27C256R@DIP28",
        generate="apps/flasher_tool/firmware/a/gen_tier_a.py",
    ),
    Target(
        name="Beam X",
        chip="ATF22V10C",
        kind="pld",
        image="apps/flasher_tool/firmware/a/beam_x.jed",
        device="ATF22V10C",
    ),
    Target(
        name="Beam Y",
        chip="ATF22V10C",
        kind="pld",
        image="apps/flasher_tool/firmware/a/beam_y.jed",
        device="ATF22V10C",
    ),
    Target(
        name="Compositor",
        chip="ATF22V10C",
        kind="pld",
        image="apps/flasher_tool/firmware/b/compositor.jed",
        device="ATF22V10C",
    ),
    Target(
        name="MCU-S1",
        chip="AVR128DB28",
        kind="avr",
        image="apps/flasher_tool/firmware/c/mcu_s1.hex",
        device="avr128db28",
    ),
)


def image_path(target: Target, repo: Path = REPO) -> Path:
    return repo / target.image

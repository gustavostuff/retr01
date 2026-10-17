"""Read Beam X back and compare it to beam_x.jed."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "flasher_tool"))

from burn import resolve_image, row_status
from catalog import REPO, TARGETS

TARGET = next(target for target in TARGETS if target.name == "Beam X")


def status(bench, repo: Path = REPO) -> str:
    return row_status(TARGET, bench, repo)


def run(bench, repo: Path = REPO) -> str:
    state = status(bench, repo)
    if state == "no image":
        return f"image missing: {repo / TARGET.image}"
    if state != "ready":
        return state
    path = resolve_image(TARGET, repo)
    reason = bench.verify(TARGET, path, None)
    if reason:
        return reason
    return "match"

"""Burn one catalog row. The bench is either the real programmers or virtual chips."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

from catalog import REPO, Target, image_path


def resolve_image(target: Target, repo: Path = REPO) -> Path:
    path = image_path(target, repo)
    if path.is_file() or not target.generate:
        return path
    script = repo / target.generate
    subprocess.run([sys.executable, str(script)], check=True)
    return path


def row_status(target: Target, bench, repo: Path = REPO) -> str:
    path = image_path(target, repo)
    if not path.is_file() and not (target.kind == "prom" and target.generate):
        return "no image"
    if target.kind == "avr":
        return "ready" if bench.updi_ports() else "no programmer"
    return bench.zif_status()


def burn(
    target: Target,
    bench,
    repo: Path = REPO,
    confirm=None,
    choose_port=None,
) -> str:
    status = row_status(target, bench, repo)
    if status == "no image":
        return f"image missing: {image_path(target, repo)}"
    if status != "ready":
        return status

    path = resolve_image(target, repo)
    if not path.is_file():
        return f"image missing: {path}"

    port = None
    if target.kind == "avr":
        ports = bench.updi_ports()
        if len(ports) == 1:
            port = ports[0]
        else:
            if choose_port is None:
                return "no CH340 port chosen"
            port = choose_port(ports)

    if target.kind == "prom":
        reason = bench.blank_prom(target)
        if reason:
            return reason
        if confirm is None or not confirm():
            return "cancelled"

    reason = bench.program(target, path, port)
    if reason:
        return reason
    return "done"

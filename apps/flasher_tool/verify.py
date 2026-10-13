"""Keyboard menu that reads a chip back and compares it to the burn file."""

from __future__ import annotations

import sys
import termios
import tty
from pathlib import Path

from burn import resolve_image, row_status
from catalog import REPO, TARGETS, Target, image_path
from flasher import _choose_port, _read_key, clear_console, format_screen
from probe import RealBench
from virtual import VirtualBench

BANNER = """\
Retr01 verify

Programmers:
  XGecu T48 or TL866II Plus    color PROM, Beam X, Beam Y, Compositor
  HiLetgo CH340 SerialUPDI     MCU-S1 (AVR pin 19)

Up/Down select, Enter verify, q quit
"""


def verify_chip(
    target: Target,
    bench,
    repo: Path = REPO,
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

    if bench.verify(target, path, port):
        return "match"
    return "mismatch"


def run_menu(bench, repo: Path = REPO, stdin=None, out=None) -> None:
    stdin = sys.stdin if stdin is None else stdin
    out = sys.stdout if out is None else out
    selected = 0
    note = ""
    interactive = stdin is sys.stdin and stdin.isatty()
    previous = None
    if interactive:
        previous = termios.tcgetattr(stdin)
        tty.setcbreak(stdin)
    try:
        while True:
            statuses = [row_status(target, bench, repo) for target in TARGETS]
            clear_console(out)
            out.write(format_screen(selected, statuses, note, BANNER))
            out.flush()
            key = _read_key(stdin)
            if key == "quit":
                return
            if key == "up":
                selected = (selected - 1) % len(TARGETS)
            elif key == "down":
                selected = (selected + 1) % len(TARGETS)
            elif key == "enter":
                target: Target = TARGETS[selected]
                clear_console(out)
                note = verify_chip(
                    target,
                    bench,
                    repo,
                    choose_port=lambda ports: _choose_port(ports, stdin, out, "verify"),
                )
                out.write(note + "\n")
                out.flush()
    finally:
        if previous is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, previous)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    bench = VirtualBench() if "--virtual" in args else RealBench()
    run_menu(bench, REPO)
    return 0


if __name__ == "__main__":
    sys.exit(main())

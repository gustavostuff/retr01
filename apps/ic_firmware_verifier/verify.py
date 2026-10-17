"""Menu for the firmware read-back. Each row is one chip script."""

from __future__ import annotations

import sys
import termios
import tty
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "flasher_tool"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import verify_beam_x
import verify_beam_y
import verify_color_prom
from flasher import _read_key, clear_console, format_screen
from probe import RealBench
from virtual import VirtualBench

BANNER = """\
Retr01 firmware verifier

Programmer: XGecu T48 or TL866II Plus
Up/Down select, Enter read the chip back, q quit
"""

CHIPS = (verify_color_prom, verify_beam_x, verify_beam_y)


def run_menu(bench, stdin=None, out=None) -> None:
    stdin = sys.stdin if stdin is None else stdin
    out = sys.stdout if out is None else out
    rows = tuple(chip.TARGET for chip in CHIPS)
    selected = 0
    note = ""
    interactive = stdin is sys.stdin and stdin.isatty()
    previous = None
    if interactive:
        previous = termios.tcgetattr(stdin)
        tty.setcbreak(stdin)
    try:
        while True:
            statuses = [chip.status(bench) for chip in CHIPS]
            clear_console(out)
            out.write(format_screen(selected, statuses, note, BANNER, rows))
            out.flush()
            key = _read_key(stdin)
            if key == "quit":
                return
            if key == "up":
                selected = (selected - 1) % len(CHIPS)
            elif key == "down":
                selected = (selected + 1) % len(CHIPS)
            elif key == "enter":
                clear_console(out)
                note = CHIPS[selected].run(bench)
                out.write(note + "\n")
                out.flush()
    finally:
        if previous is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, previous)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    bench = VirtualBench() if "--virtual" in args else RealBench()
    run_menu(bench)
    return 0


if __name__ == "__main__":
    sys.exit(main())

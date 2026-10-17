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
from flasher import _read_key, clear_console
from probe import RealBench
from virtual import VirtualBench

CYAN = "\033[36m"
GREEN = "\033[32m"
RED = "\033[31m"
RESET = "\033[0m"

BANNER = """\
Retr01 firmware verifier

Programmer: XGecu T48 or TL866II Plus
"""

CHIPS = (verify_color_prom, verify_beam_x, verify_beam_y)


def _paint(note: str, color: bool) -> str:
    if not color or not note:
        return note
    tone = GREEN if note.endswith("firmware seems OK") else RED
    return f"{tone}{note}{RESET}"


def _screen(note: str, color: bool) -> str:
    lines = [BANNER.rstrip("\n"), ""]
    for index, chip in enumerate(CHIPS):
        row = f"  {index + 1} = {chip.TARGET.name}"
        lines.append(f"{CYAN}{row}{RESET}" if color else row)
    quit_row = "  Q = Quit"
    lines.append(f"{CYAN}{quit_row}{RESET}" if color else quit_row)
    if note:
        lines.append("")
        lines.append(_paint(note, color))
    return "\n".join(lines) + "\n"


def run_menu(bench, stdin=None, out=None) -> None:
    stdin = sys.stdin if stdin is None else stdin
    out = sys.stdout if out is None else out
    note = ""
    interactive = stdin is sys.stdin and stdin.isatty()
    color = bool(getattr(out, "isatty", lambda: False)())
    previous = None
    if interactive:
        previous = termios.tcgetattr(stdin)
        tty.setcbreak(stdin)
    try:
        while True:
            clear_console(out)
            out.write(_screen(note, color))
            out.flush()
            key = _read_key(stdin)
            if key == "quit":
                return
            if not key.isdigit():
                continue
            index = int(key) - 1
            if index < 0 or index >= len(CHIPS):
                continue
            clear_console(out)
            note = CHIPS[index].run(bench)
            out.write(_paint(note, color) + "\n")
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

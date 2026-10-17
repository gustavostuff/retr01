"""Menu for the breadboard counter checks."""

from __future__ import annotations

import sys
import termios
import tty
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "flasher_tool"))

import verify_beam_x
import verify_beam_y
from flasher import _read_key, clear_console
from probe import ch340_ports

BANNER = """\
Retr01 logic verifier

Clock module TXD drives Beam X pin 1. Watcher RXD sits on one output pin.
Up/Down select, Enter check, q quit
"""

CHIPS = (verify_beam_x, verify_beam_y)


def _wait(stdin, out, prompt: str) -> None:
    out.write(prompt + "\n")
    out.flush()
    while _read_key(stdin) not in ("enter", "quit"):
        pass


def _check(chip, stdin, out) -> str:
    ports = ch340_ports()
    if len(ports) < 2:
        return "need a clock module and a watch module"
    clock_name, watch_name = ports[0], ports[1]
    out.write(f"clock {clock_name}\nwatch {watch_name}\n")
    out.flush()

    def prepare(name: str, pin: int) -> None:
        _wait(
            stdin,
            out,
            f"RXD on pin {pin} ({name}). TXD is low. Close RES, then open it. Enter to clock.",
        )

    return chip.run_serial(clock_name, watch_name, prepare)


def run_menu(stdin=None, out=None) -> int:
    stdin = sys.stdin if stdin is None else stdin
    out = sys.stdout if out is None else out
    selected = 0
    note = ""
    interactive = stdin is sys.stdin and stdin.isatty()
    previous = None
    if interactive:
        previous = termios.tcgetattr(stdin)
        tty.setcbreak(stdin)
    code = 0
    try:
        while True:
            clear_console(out)
            lines = [BANNER.rstrip("\n"), ""]
            for index, chip in enumerate(CHIPS):
                mark = ">" if index == selected else " "
                lines.append(f"  {mark} {chip.NAME:<16} {chip.CHIP}")
            if note:
                lines.append("")
                lines.append(note)
            out.write("\n".join(lines) + "\n")
            out.flush()
            key = _read_key(stdin)
            if key == "quit":
                return code
            if key == "up":
                selected = (selected - 1) % len(CHIPS)
            elif key == "down":
                selected = (selected + 1) % len(CHIPS)
            elif key == "enter":
                clear_console(out)
                note = _check(CHIPS[selected], stdin, out)
                if note != "count ok":
                    code = 1
                out.write(note + "\n")
                out.flush()
    finally:
        if previous is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, previous)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    if "--virtual" in args:
        failed = False
        for chip in CHIPS:
            note = chip.run_virtual()
            print(f"{chip.NAME}: {note}")
            failed = failed or note != "count ok"
        return 1 if failed else 0
    return run_menu()


if __name__ == "__main__":
    sys.exit(main())

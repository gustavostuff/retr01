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

CYAN = "\033[36m"
GREEN = "\033[32m"
RED = "\033[31m"
RESET = "\033[0m"

BANNER = """\
Retr01 logic verifier

Clock module TXD drives Beam X pin 1. Watcher RXD sits on one output pin.
"""

CHIPS = (verify_beam_x, verify_beam_y)


def _paint(note: str, color: bool) -> str:
    if not color or not note:
        return note
    tone = GREEN if note.endswith("logic works ok") else RED
    return f"{tone}{note}{RESET}"


def _wait(stdin, out, prompt: str) -> None:
    out.write(prompt + "\n")
    out.flush()
    while _read_key(stdin) not in ("enter", "quit"):
        pass


def _check(chip, stdin, out, hz: float) -> str:
    ports = ch340_ports()
    if len(ports) < 2:
        return "need a clock module and a watch module"
    clock_name, watch_name = ports[0], ports[1]
    out.write(f"clock {clock_name} at {hz:g} Hz\nwatch {watch_name}\n")
    out.flush()

    def prepare(name: str, pin: int) -> None:
        _wait(
            stdin,
            out,
            f"RXD on pin {pin} ({name}). TXD is low. Close RES, then open it. Enter to clock.",
        )

    return chip.run_serial(clock_name, watch_name, prepare, hz)


def parse_args(argv: list[str]) -> tuple[bool, bool, float]:
    """Return virtual mode, LED clock mode, and the TXD clock in counts per second."""
    virtual = False
    clock = False
    hz = 10.0
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg == "--virtual":
            virtual = True
            index += 1
            continue
        if arg == "--clock":
            clock = True
            index += 1
            continue
        if arg == "--hz" or arg.startswith("--hz="):
            raw = arg.split("=", 1)[1] if arg.startswith("--hz=") else ""
            if arg == "--hz":
                index += 1
                raw = argv[index] if index < len(argv) else ""
            try:
                hz = float(raw)
            except ValueError:
                raise ValueError("clock hz needs a number above 0") from None
            if hz <= 0:
                raise ValueError("clock hz needs a number above 0")
            index += 1
            continue
        raise ValueError(f"unknown argument {arg}")
    return virtual, clock, hz


def run_clock(hz: float) -> int:
    """Toggle the one CH340 TXD pin so the LED counters can be watched."""
    ports = ch340_ports()
    if not ports:
        print("need a clock module")
        return 1
    print(f"clock {ports[0]} at {hz:g} Hz")

    def wait() -> None:
        input("TXD is low. Close RES, then open it. Enter to clock. ")

    from pins import run_clock as toggle

    try:
        toggle(ports[0], hz, wait)
    except KeyboardInterrupt:
        print()
    return 0


def run_menu(stdin=None, out=None, hz: float = 10) -> int:
    stdin = sys.stdin if stdin is None else stdin
    out = sys.stdout if out is None else out
    note = ""
    interactive = stdin is sys.stdin and stdin.isatty()
    color = bool(getattr(out, "isatty", lambda: False)())
    previous = None
    if interactive:
        previous = termios.tcgetattr(stdin)
        tty.setcbreak(stdin)
    code = 0
    try:
        while True:
            clear_console(out)
            lines = [BANNER.rstrip("\n"), f"TXD clock is {hz:g} Hz.", ""]
            for index, chip in enumerate(CHIPS):
                row = f"  {index + 1} = {chip.NAME}"
                lines.append(f"{CYAN}{row}{RESET}" if color else row)
            quit_row = "  Q = Quit"
            lines.append(f"{CYAN}{quit_row}{RESET}" if color else quit_row)
            if note:
                lines.append("")
                lines.append(_paint(note, color))
            out.write("\n".join(lines) + "\n")
            out.flush()
            key = _read_key(stdin)
            if key == "quit":
                return code
            if not key.isdigit():
                continue
            index = int(key) - 1
            if index < 0 or index >= len(CHIPS):
                continue
            clear_console(out)
            note = _check(CHIPS[index], stdin, out, hz)
            if not note.endswith("logic works ok"):
                code = 1
            out.write(_paint(note, color) + "\n")
            out.flush()
    finally:
        if previous is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, previous)


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    try:
        virtual, clock, hz = parse_args(args)
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1
    if clock:
        return run_clock(hz)
    if virtual:
        failed = False
        color = sys.stdout.isatty()
        for chip in CHIPS:
            note = chip.run_virtual()
            print(_paint(note, color))
            failed = failed or not note.endswith("logic works ok")
        return 1 if failed else 0
    return run_menu(hz=hz)


if __name__ == "__main__":
    sys.exit(main())

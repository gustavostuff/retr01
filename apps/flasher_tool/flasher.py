"""Keyboard menu for the Tier A, B, and C burns."""

from __future__ import annotations

import subprocess
import sys
import termios
import tty
from pathlib import Path

from burn import burn, row_status
from catalog import BANNER, REPO, TARGETS, Target
from probe import RealBench
from virtual import VirtualBench


def format_screen(
    selected: int, statuses: list[str], note: str = "", banner: str = BANNER
) -> str:
    lines = [banner.rstrip("\n"), ""]
    for index, target in enumerate(TARGETS):
        mark = ">" if index == selected else " "
        lines.append(
            f"  {mark} {target.name:<16} {target.chip:<12} {statuses[index]}"
        )
    if note:
        lines.append("")
        lines.append(note)
    return "\n".join(lines) + "\n"


def clear_console(out) -> None:
    if out is sys.stdout:
        subprocess.run(["clear"], check=False)
        return
    out.write("\x1b[H\x1b[2J\x1b[3J")


def _read_key(stream) -> str:
    char = stream.read(1)
    if char == "\x1b":
        rest = stream.read(1)
        if rest == "[":
            arrow = stream.read(1)
            if arrow == "A":
                return "up"
            if arrow == "B":
                return "down"
        return "esc"
    if char in ("\r", "\n"):
        return "enter"
    if char in ("q", "Q"):
        return "quit"
    return char or ""


def _confirm_otp(stdin, out) -> bool:
    out.write("Program this PROM? The part is one-time. [y/N] ")
    out.flush()
    answer = ""
    while True:
        char = stdin.read(1)
        if char in ("\r", "\n"):
            break
        if char in ("\x7f", "\b") and answer:
            answer = answer[:-1]
            continue
        if char:
            answer += char
    out.write("\n")
    return answer.strip().lower() == "y"


def _choose_port(ports: list[str], stdin, out, verb: str = "flash") -> str:
    selected = 0
    while True:
        clear_console(out)
        out.write("CH340 port:\n")
        for index, port in enumerate(ports):
            mark = ">" if index == selected else " "
            out.write(f"  {mark} {port}\n")
        out.write(f"Up/Down select, Enter {verb}\n")
        out.flush()
        key = _read_key(stdin)
        if key == "up":
            selected = (selected - 1) % len(ports)
        elif key == "down":
            selected = (selected + 1) % len(ports)
        elif key == "enter":
            return ports[selected]


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
            out.write(format_screen(selected, statuses, note))
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
                note = burn(
                    target,
                    bench,
                    repo,
                    confirm=lambda: _confirm_otp(stdin, out),
                    choose_port=lambda ports: _choose_port(ports, stdin, out),
                )
                out.write(note + "\n")
                out.flush()
    finally:
        if previous is not None:
            termios.tcsetattr(stdin, termios.TCSADRAIN, previous)


def device_check_report(bench) -> tuple[str, bool]:
    """Report the T48 and the CH340 lead. No burn."""
    zif = bench.zif_status()
    ports = bench.updi_ports()
    if ports:
        serial = "ready  " + " ".join(ports)
        serial_ok = True
    else:
        serial = "not found"
        serial_ok = False
    lines = [
        f"{'XGecu T48 or TL866II Plus':<28} {zif}",
        f"{'HiLetgo CH340 SerialUPDI':<28} {serial}",
    ]
    return "\n".join(lines) + "\n", zif == "ready" and serial_ok


def main(argv: list[str] | None = None) -> int:
    args = sys.argv[1:] if argv is None else argv
    bench = VirtualBench() if "--virtual" in args else RealBench()
    if "--check" in args:
        text, ok = device_check_report(bench)
        sys.stdout.write(text)
        return 0 if ok else 1
    run_menu(bench, REPO)
    return 0


if __name__ == "__main__":
    sys.exit(main())

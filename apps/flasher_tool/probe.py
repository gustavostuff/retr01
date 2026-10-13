"""Real T48 and CH340 probes. Commands match minipro 0.7 and pymcuprog."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

from catalog import CH340_VENDOR, Target

# Standing minipro banners and progress lines. A failed burn keeps the rest.
_NOISE = (
    "support is not yet complete",
    "device code:",
    "serial code:",
    "manufactured:",
    "usb speed:",
    "supply voltage:",
    "reading device",
    "writing device",
    "erasing",
    "jed file parsed",
    "declared fuse checksum",
    "declared file checksum",
)


def minipro_write_argv(target: Target, path: Path) -> list[str]:
    argv = ["minipro", "-p", target.device, "-w", str(path)]
    if target.kind == "prom":
        argv.extend(["-s", "-o", "vpp=12.5"])
    return argv


def minipro_verify_argv(target: Target, path: Path) -> list[str]:
    argv = ["minipro", "-p", target.device, "-m", str(path)]
    if target.kind == "prom":
        argv.append("-s")
    return argv


def minipro_blank_argv(target: Target) -> list[str]:
    return ["minipro", "-p", target.device, "-b"]


def programmer_error_lines(text: str) -> str:
    """Programmer lines that explain a failure. Progress and the T48 banner stay out."""
    kept: list[str] = []
    for raw in text.replace("\r", "\n").splitlines():
        line = raw.strip()
        if not line:
            continue
        low = line.lower()
        if low.startswith("found t48") or low.startswith("found tl866"):
            continue
        noisy = any(part in low for part in _NOISE)
        failed = "fail" in low or "error" in low or "invalid" in low
        if noisy and not failed:
            continue
        kept.append(line)
    return "\n".join(kept)


def command_failure(step: str, text: str, code: int) -> str:
    detail = programmer_error_lines(text)
    if not detail:
        detail = f"exit {code}"
    return f"{step}\n{detail}"


def _run(argv: list[str]) -> tuple[int, str]:
    print(" ".join(argv), flush=True)
    proc = subprocess.Popen(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    assert proc.stdout is not None
    chunks: list[str] = []
    for line in proc.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        chunks.append(line)
    return proc.wait(), "".join(chunks)


def blank_check_message(returncode: int, text: str) -> str | None:
    """None when the chip reads blank. Otherwise the programmer's own line."""
    lowered = text.lower()
    failed = returncode != 0 or "not blank" in lowered or "invalid chip id" in lowered
    if not failed:
        return None
    for line in text.splitlines():
        stripped = line.strip()
        low = stripped.lower()
        if "invalid chip id" in low or "not blank" in low:
            return stripped
    if "not blank" in lowered:
        return "chip is not blank"
    return "blank check failed"


def pymcuprog_ping_argv(target: Target, port: str) -> list[str]:
    return [
        "pymcuprog",
        "ping",
        "-d",
        target.device,
        "-t",
        "uart",
        "-u",
        port,
    ]


def pymcuprog_verify_argv(target: Target, path: Path, port: str) -> list[str]:
    return [
        "pymcuprog",
        "verify",
        "-d",
        target.device,
        "-t",
        "uart",
        "-u",
        port,
        "-f",
        str(path),
    ]


def pymcuprog_write_argv(target: Target, path: Path, port: str) -> list[str]:
    return [
        "pymcuprog",
        "write",
        "-d",
        target.device,
        "-t",
        "uart",
        "-u",
        port,
        "-f",
        str(path),
        "--verify",
    ]


def _vendor(tty_name: str, sysfs: Path) -> str | None:
    link = sysfs / tty_name / "device"
    if not link.exists():
        return None
    node = link.resolve()
    for _ in range(8):
        vendor = node / "idVendor"
        if vendor.is_file():
            return vendor.read_text(encoding="ascii").strip().lower()
        if node == node.parent:
            break
        node = node.parent
    return None


def ch340_ports(sysfs: Path = Path("/sys/class/tty")) -> list[str]:
    found: list[str] = []
    if not sysfs.is_dir():
        return found
    for node in sorted(sysfs.glob("ttyUSB*")):
        if _vendor(node.name, sysfs) == CH340_VENDOR:
            found.append("/dev/" + node.name)
    return found


class RealBench:
    def zif_status(self) -> str:
        if shutil.which("minipro") is None:
            return "minipro missing"
        result = subprocess.run(
            ["minipro", "-k"],
            check=False,
            capture_output=True,
            text=True,
        )
        text = f"{result.stdout}\n{result.stderr}".lower()
        if "no programmer" in text:
            return "no programmer"
        return "ready"

    def updi_ports(self) -> list[str]:
        return ch340_ports()

    def blank_prom(self, target: Target) -> str | None:
        result = subprocess.run(
            minipro_blank_argv(target),
            check=False,
            capture_output=True,
            text=True,
        )
        text = f"{result.stdout}\n{result.stderr}"
        return blank_check_message(result.returncode, text)

    def program(self, target: Target, path: Path, port: str | None = None) -> str | None:
        """None when the burn and its check both succeed. Otherwise the failure text."""
        if target.kind == "avr":
            if not port:
                raise RuntimeError("AVR burn needs a serial port")
            code, text = _run(pymcuprog_ping_argv(target, port))
            if code != 0:
                return command_failure("write failed", text, code)
            code, text = _run(pymcuprog_write_argv(target, path, port))
            if code != 0:
                return command_failure("write failed", text, code)
            return None
        code, text = _run(minipro_write_argv(target, path))
        if code != 0:
            return command_failure("write failed", text, code)
        code, text = _run(minipro_verify_argv(target, path))
        if code != 0:
            return command_failure("verify failed", text, code)
        return None

    def verify(self, target: Target, path: Path, port: str | None = None) -> str | None:
        """None when the chip matches the image. Otherwise the failure text."""
        if target.kind == "avr":
            if not port:
                raise RuntimeError("AVR verify needs a serial port")
            code, text = _run(pymcuprog_ping_argv(target, port))
            if code != 0:
                return command_failure("verify failed", text, code)
            code, text = _run(pymcuprog_verify_argv(target, path, port))
            if code != 0:
                return command_failure("verify failed", text, code)
            return None
        code, text = _run(minipro_verify_argv(target, path))
        if code != 0:
            return command_failure("verify failed", text, code)
        return None

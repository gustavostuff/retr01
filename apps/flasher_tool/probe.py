"""Real T48 and CH340 probes. Commands match minipro 0.7 and pymcuprog."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from catalog import CH340_VENDOR, Target


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

    def program(self, target: Target, path: Path, port: str | None = None) -> bool:
        if target.kind == "avr":
            if not port:
                raise RuntimeError("AVR burn needs a serial port")
            ping = pymcuprog_ping_argv(target, port)
            print(" ".join(ping), flush=True)
            if subprocess.run(ping, check=False).returncode != 0:
                return False
            write = pymcuprog_write_argv(target, path, port)
            print(" ".join(write), flush=True)
            return subprocess.run(write, check=False).returncode == 0
        write = minipro_write_argv(target, path)
        verify = minipro_verify_argv(target, path)
        print(" ".join(write), flush=True)
        if subprocess.run(write, check=False).returncode != 0:
            return False
        print(" ".join(verify), flush=True)
        return subprocess.run(verify, check=False).returncode == 0

    def verify(self, target: Target, path: Path, port: str | None = None) -> bool:
        if target.kind == "avr":
            if not port:
                raise RuntimeError("AVR verify needs a serial port")
            ping = pymcuprog_ping_argv(target, port)
            print(" ".join(ping), flush=True)
            if subprocess.run(ping, check=False).returncode != 0:
                return False
            argv = pymcuprog_verify_argv(target, path, port)
            print(" ".join(argv), flush=True)
            return subprocess.run(argv, check=False).returncode == 0
        argv = minipro_verify_argv(target, path)
        print(" ".join(argv), flush=True)
        return subprocess.run(argv, check=False).returncode == 0

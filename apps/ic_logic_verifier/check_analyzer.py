#!/usr/bin/env python3
"""See whether the 24 MHz 8-channel USB logic analyzer is on the bus and opens."""

from __future__ import annotations

import shutil
import subprocess
import sys

GREEN = "\033[32m"
RED = "\033[31m"
RESET = "\033[0m"

USB_IDS = ("0925:3881", "1d50:608c")


def paint(text: str, ok: bool) -> str:
    if not sys.stdout.isatty():
        return text
    tone = GREEN if ok else RED
    return f"{tone}{text}{RESET}"


def usb_lines() -> list[str]:
    if shutil.which("lsusb") is None:
        return []
    proc = subprocess.run(["lsusb"], check=False, capture_output=True, text=True)
    hits = []
    for line in proc.stdout.splitlines():
        if any(vid in line for vid in USB_IDS):
            hits.append(line.strip())
    return hits


def device_line(text: str) -> str:
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("fx2lafw:conn=") and "channels:" in stripped:
            return stripped
    return ""


def main() -> int:
    if shutil.which("sigrok-cli") is None:
        print(paint("sigrok-cli is not installed", False))
        return 1

    proc = subprocess.run(
        ["sigrok-cli", "-d", "fx2lafw", "--show"],
        check=False,
        capture_output=True,
        text=True,
    )
    text = proc.stdout + proc.stderr
    device = device_line(text)
    usb = usb_lines()
    ready = proc.returncode == 0 and "8 channels" in device and "24 MHz" in text
    if ready:
        print(paint("Logic analyzer is connected and ready", True))
        print(device)
        for line in usb:
            print(line)
        return 0

    print(paint("Logic analyzer is not ready", False))
    if usb:
        for line in usb:
            print(line)
    else:
        print("No 0925:3881 or 1d50:608c device on the USB bus")
    if device:
        print(device)
    err = proc.stderr.strip()
    if err:
        print(err)
    return 1


if __name__ == "__main__":
    sys.exit(main())

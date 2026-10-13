"""In-memory chips. A burn writes these instead of a USB programmer."""

from __future__ import annotations

from pathlib import Path

from catalog import Target

PROM_SIZE = 32 * 1024
BLANK = 0xFF


class VirtualProm:
    """OTP image. A second write is refused while any byte is programmed."""

    def __init__(self, size: int = PROM_SIZE) -> None:
        self.data = bytearray([BLANK] * size)
        self.writes = 0

    def is_blank(self) -> bool:
        return all(byte == BLANK for byte in self.data)

    def program(self, payload: bytes) -> None:
        if not self.is_blank():
            raise RuntimeError("chip is not blank")
        if len(payload) > len(self.data):
            raise RuntimeError("image is larger than the chip")
        self.data[: len(payload)] = payload
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        if self.data[: len(payload)] != payload:
            return False
        return all(byte == BLANK for byte in self.data[len(payload) :])


class VirtualPld:
    def __init__(self) -> None:
        self.image: bytes | None = None
        self.writes = 0

    def program(self, payload: bytes) -> None:
        self.image = bytes(payload)
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        return self.image == payload


class VirtualAvr:
    def __init__(self) -> None:
        self.image: bytes | None = None
        self.port: str | None = None
        self.writes = 0

    def program(self, payload: bytes, port: str) -> None:
        self.image = bytes(payload)
        self.port = port
        self.writes += 1

    def matches(self, payload: bytes) -> bool:
        return self.image == payload


class VirtualBench:
    """Stand-in for the T48 and the CH340 lead."""

    def __init__(self) -> None:
        self.prom = VirtualProm()
        self.plds: dict[str, VirtualPld] = {}
        self.avr = VirtualAvr()
        self.minipro_installed = True
        self.programmer_connected = True
        self.ports = ["/dev/ttyUSB0"]
        self.fail_verify = False

    def zif_status(self) -> str:
        if not self.minipro_installed:
            return "minipro missing"
        if not self.programmer_connected:
            return "no programmer"
        return "ready"

    def updi_ports(self) -> list[str]:
        return list(self.ports)

    def blank_prom(self, _target: Target) -> str | None:
        if self.prom.is_blank():
            return None
        return "chip is not blank"

    def program(self, target: Target, path: Path, port: str | None = None) -> bool:
        payload = path.read_bytes()
        if target.kind == "prom":
            self.prom.program(payload)
            ok = self.prom.matches(payload)
        elif target.kind == "pld":
            chip = self.plds.setdefault(target.name, VirtualPld())
            chip.program(payload)
            ok = chip.matches(payload)
        elif target.kind == "avr":
            if not port:
                raise RuntimeError("AVR burn needs a serial port")
            self.avr.program(payload, port)
            ok = self.avr.matches(payload)
        else:
            raise RuntimeError(f"unknown kind {target.kind}")
        if self.fail_verify:
            return False
        return ok

    def verify(self, target: Target, path: Path, port: str | None = None) -> bool:
        payload = path.read_bytes()
        if target.kind == "prom":
            ok = self.prom.matches(payload)
        elif target.kind == "pld":
            chip = self.plds.get(target.name)
            ok = chip is not None and chip.matches(payload)
        elif target.kind == "avr":
            if not port:
                raise RuntimeError("AVR verify needs a serial port")
            ok = self.avr.port == port and self.avr.matches(payload)
        else:
            raise RuntimeError(f"unknown kind {target.kind}")
        if self.fail_verify:
            return False
        return ok
